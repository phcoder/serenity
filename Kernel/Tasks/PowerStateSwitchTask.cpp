/*
 * Copyright (c) 2023, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Platform.h>
#if ARCH(X86_64)
#    include <Kernel/Arch/x86_64/I8042Reboot.h>
#    include <Kernel/Arch/x86_64/Shutdown.h>
#elif ARCH(AARCH64)
#    include <Kernel/Arch/aarch64/RPi/Watchdog.h>
#endif
#include <AK/StringView.h>
#include <Kernel/Arch/PowerState.h>
#include <Kernel/FileSystem/FileSystem.h>
#include <Kernel/FileSystem/VirtualFileSystem.h>
#include <Kernel/Firmware/ACPI/Parser.h>
#include <Kernel/Library/Panic.h>
#include <Kernel/Sections.h>
#include <Kernel/TTY/ConsoleManagement.h>
#include <Kernel/Tasks/FinalizerTask.h>
#include <Kernel/Tasks/PowerStateSwitchTask.h>
#include <Kernel/Tasks/Process.h>
#include <Kernel/Tasks/Scheduler.h>

namespace Kernel {

static constexpr StringView power_state_switch_task_name_view = "Power State Switch Task"sv;

Thread* g_power_state_switch_task;
bool g_in_system_shutdown { false };

void PowerStateSwitchTask::power_state_switch_task(void* raw_entry_data)
{
    Thread::current()->set_priority(THREAD_PRIORITY_HIGH);
    auto entry_data = bit_cast<PowerStateCommand>(raw_entry_data);
    switch (entry_data) {
    case PowerStateCommand::Shutdown:
        MUST(PowerStateSwitchTask::perform_shutdown());
        break;
    case PowerStateCommand::Reboot:
        MUST(PowerStateSwitchTask::perform_reboot());
        break;
    default:
        PANIC("Unknown power state command: {}", to_underlying(entry_data));
    }

    // Although common, the system may not halt through this task.
    // Clear the power state switch task so that it can be spawned again.
    g_power_state_switch_task = nullptr;
}

void PowerStateSwitchTask::spawn(PowerStateCommand command)
{
    // FIXME: If we switch power states during memory pressure, don't let the system crash just because of our task name.
    NonnullOwnPtr<KString> power_state_switch_task_name = MUST(KString::try_create(power_state_switch_task_name_view));

    VERIFY(g_power_state_switch_task == nullptr);
    auto [_, power_state_switch_task_thread] = MUST(Process::create_kernel_process(
        move(power_state_switch_task_name), power_state_switch_task, bit_cast<void*>(command)));
    g_power_state_switch_task = move(power_state_switch_task_thread);
}

ErrorOr<void> PowerStateSwitchTask::perform_reboot()
{
    dbgln("acquiring FS locks...");
    FileSystem::lock_all();
    dbgln("syncing mounted filesystems...");
    FileSystem::sync();

    dbgln("attempting reboot via ACPI");
    if (ACPI::is_enabled())
        ACPI::Parser::the()->try_acpi_reboot();
    arch_specific_reboot();

    dbgln("reboot attempts failed, applications will stop responding.");
    dmesgln("Reboot can't be completed. It's safe to turn off the computer!");
    Processor::halt();
}

ErrorOr<void> PowerStateSwitchTask::perform_shutdown()
{
    // We assume that by this point userland has tried as much as possible to shut down everything in an orderly fashion.
    // Therefore, we force kill remaining processes, including Kernel processes, except the finalizer and ourselves.
    dbgln("Killing remaining processes...");
    Optional<Process&> finalizer_process;
    Process::all_instances().for_each([&](Process& process) {
        if (process.pid() == g_finalizer->process().pid())
            finalizer_process = process;
    });
    VERIFY(finalizer_process.has_value());

    // Allow init process and finalizer task to be killed.
    g_in_system_shutdown = true;

    // Make sure to kill all user processes first, otherwise we might get weird hangups.
    TRY(kill_processes(ProcessKind::User, finalizer_process->pid()));
    TRY(kill_processes(ProcessKind::Kernel, finalizer_process->pid()));

    finalizer_process->die();
    finalizer_process->finalize();
    size_t alive_process_count = 0;
    Process::all_instances().for_each([&](Process& process) {
        if (process.pid() != Process::current().pid() && !process.is_dead())
            alive_process_count++;
    });
    // Don't panic here (since we may panic in a bit anyways) but report the probable cause of an unclean shutdown.
    if (alive_process_count != 0)
        dbgln("We're not the last process alive; proper shutdown may fail!");

    ConsoleManagement::the().switch_to_debug();

    dbgln("Locking all file systems...");
    FileSystem::lock_all();
    FileSystem::sync();

    dbgln("Unmounting all file systems...");

    auto unmount_was_successful = true;
    while (unmount_was_successful) {
        unmount_was_successful = false;
        Vector<Mount&, 16> mounts;
        TRY(VirtualFileSystem::the().for_each_mount([&](auto const& mount) -> ErrorOr<void> {
            TRY(mounts.try_append(const_cast<Mount&>(mount)));
            return {};
        }));
        if (mounts.is_empty())
            break;
        auto const remaining_mounts = mounts.size();

        while (!mounts.is_empty()) {
            auto& mount = mounts.take_last();
            mount.guest_fs().flush_writes();

            auto mount_path = TRY(mount.absolute_path());
            auto& mount_inode = mount.guest();
            auto const result = VirtualFileSystem::the().unmount(mount_inode, mount_path->view());
            if (result.is_error()) {
                dbgln("Error during unmount of {}: {}", mount_path, result.error());
                // FIXME: For unknown reasons the root FS stays busy even after everything else has shut down and was unmounted.
                //        Until we find the underlying issue, allow an unclean shutdown here.
                if (remaining_mounts <= 1)
                    dbgln("BUG! One mount remaining; the root file system may not be unmountable at all. Shutting down anyways.");
            } else {
                unmount_was_successful = true;
            }
        }
    }

    dbgln("Attempting system shutdown...");

    arch_specific_poweroff();

    dbgln("shutdown attempts failed, applications will stop responding.");
    dmesgln("Shutdown can't be completed. It's safe to turn off the computer!");
    Processor::halt();
}

ErrorOr<void> PowerStateSwitchTask::kill_processes(ProcessKind kind, ProcessID finalizer_pid)
{
    bool kill_kernel_processes = kind == ProcessKind::Kernel;

    Process::all_instances().for_each([&](Process& process) {
        if (process.pid() != Process::current().pid() && process.pid() != finalizer_pid && process.is_kernel_process() == kill_kernel_processes) {
            process.die();
        }
    });

    // Although we *could* finalize processes ourselves (g_in_system_shutdown allows this),
    // we're nice citizens and let the finalizer task perform final duties before we kill it.
    Scheduler::notify_finalizer();
    int alive_process_count = 1;
    MonotonicTime last_status_time = TimeManagement::the().monotonic_time();
    while (alive_process_count > 0) {
        Scheduler::yield();
        alive_process_count = 0;
        Process::all_instances().for_each([&](Process& process) {
            if (process.pid() != Process::current().pid() && !process.is_dead() && process.pid() != finalizer_pid && process.is_kernel_process() == kill_kernel_processes)
                alive_process_count++;
        });

        if (TimeManagement::the().monotonic_time() - last_status_time > Duration::from_seconds(2)) {
            last_status_time = TimeManagement::the().monotonic_time();
            dmesgln("Waiting on {} processes to exit...", alive_process_count);

            if constexpr (PROCESS_DEBUG) {
                Process::all_instances().for_each_const([&](Process const& process) {
                    if (process.pid() != Process::current().pid() && !process.is_dead() && process.pid() != finalizer_pid && process.is_kernel_process() == kill_kernel_processes) {
                        dbgln("Process {:2} kernel={} dead={} dying={} ({})",
                            process.pid(), process.is_kernel_process(), process.is_dead(), process.is_dying(),
                            process.name().with([](auto& name) { return name->view(); }));
                    }
                });
            }
        }
    }

    return {};
}

}
