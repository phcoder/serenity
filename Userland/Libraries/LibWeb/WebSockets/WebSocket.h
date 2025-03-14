/*
 * Copyright (c) 2021-2022, Dex♪ <dexes.ttp@gmail.com>
 * Copyright (c) 2023, Kenneth Myhra <kennethmyhra@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ByteBuffer.h>
#include <AK/URL.h>
#include <LibCore/Object.h>
#include <LibWeb/Bindings/PlatformObject.h>
#include <LibWeb/DOM/EventTarget.h>
#include <LibWeb/Forward.h>
#include <LibWeb/HTML/Window.h>
#include <LibWeb/WebIDL/ExceptionOr.h>

#define ENUMERATE_WEBSOCKET_EVENT_HANDLERS(E) \
    E(onerror, HTML::EventNames::error)       \
    E(onclose, HTML::EventNames::close)       \
    E(onopen, HTML::EventNames::open)         \
    E(onmessage, HTML::EventNames::message)

namespace Web::WebSockets {

class WebSocketClientSocket;
class WebSocketClientManager;

class WebSocket final : public DOM::EventTarget {
    WEB_PLATFORM_OBJECT(WebSocket, DOM::EventTarget);

public:
    enum class ReadyState : u16 {
        Connecting = 0,
        Open = 1,
        Closing = 2,
        Closed = 3,
    };

    static WebIDL::ExceptionOr<JS::NonnullGCPtr<WebSocket>> construct_impl(JS::Realm&, String const& url, Optional<Variant<String, Vector<String>>> const& protocols);

    virtual ~WebSocket() override;

    WebIDL::ExceptionOr<String> url() const { return TRY_OR_THROW_OOM(vm(), m_url.to_string()); }
    void set_url(AK::URL url) { m_url = move(url); }

#undef __ENUMERATE
#define __ENUMERATE(attribute_name, event_name)       \
    void set_##attribute_name(WebIDL::CallbackType*); \
    WebIDL::CallbackType* attribute_name();
    ENUMERATE_WEBSOCKET_EVENT_HANDLERS(__ENUMERATE)
#undef __ENUMERATE

    ReadyState ready_state() const;
    String extensions() const;
    WebIDL::ExceptionOr<String> protocol() const;

    String const& binary_type() { return m_binary_type; }
    void set_binary_type(String const& type) { m_binary_type = type; }

    WebIDL::ExceptionOr<void> close(Optional<u16> code, Optional<String> reason);
    WebIDL::ExceptionOr<void> send(Variant<JS::Handle<JS::Object>, JS::Handle<FileAPI::Blob>, String> const& data);

private:
    void on_open();
    void on_message(ByteBuffer message, bool is_text);
    void on_error();
    void on_close(u16 code, String reason, bool was_clean);

    WebSocket(JS::Realm&);

    virtual JS::ThrowCompletionOr<void> initialize(JS::Realm&) override;

    ErrorOr<void> establish_web_socket_connection(AK::URL& url_record, Vector<String>& protocols, HTML::EnvironmentSettingsObject& client);

    AK::URL m_url;
    String m_binary_type { "blob"_string.release_value_but_fixme_should_propagate_errors() };
    RefPtr<WebSocketClientSocket> m_websocket;
};

class WebSocketClientSocket : public RefCounted<WebSocketClientSocket> {
public:
    virtual ~WebSocketClientSocket();

    struct CertificateAndKey {
        DeprecatedString certificate;
        DeprecatedString key;
    };

    struct Message {
        ByteBuffer data;
        bool is_text { false };
    };

    enum class Error {
        CouldNotEstablishConnection,
        ConnectionUpgradeFailed,
        ServerClosedSocket,
    };

    virtual Web::WebSockets::WebSocket::ReadyState ready_state() = 0;
    virtual DeprecatedString subprotocol_in_use() = 0;

    virtual void send(ByteBuffer binary_or_text_message, bool is_text) = 0;
    virtual void send(StringView text_message) = 0;
    virtual void close(u16 code = 1005, DeprecatedString reason = {}) = 0;

    Function<void()> on_open;
    Function<void(Message)> on_message;
    Function<void(Error)> on_error;
    Function<void(u16 code, DeprecatedString reason, bool was_clean)> on_close;
    Function<CertificateAndKey()> on_certificate_requested;

protected:
    explicit WebSocketClientSocket();
};

class WebSocketClientManager : public Core::Object {
    C_OBJECT_ABSTRACT(WebSocketClientManager)
public:
    static void initialize(RefPtr<WebSocketClientManager>);
    static WebSocketClientManager& the();

    virtual RefPtr<WebSocketClientSocket> connect(AK::URL const&, DeprecatedString const& origin, Vector<DeprecatedString> const& protocols) = 0;

protected:
    explicit WebSocketClientManager();
};

}
