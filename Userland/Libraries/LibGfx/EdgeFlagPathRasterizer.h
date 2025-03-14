/*
 * Copyright (c) 2023, MacDue <macdue@dueutil.tech>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Array.h>
#include <AK/GenericShorthands.h>
#include <AK/Vector.h>
#include <LibGfx/Bitmap.h>
#include <LibGfx/PaintStyle.h>
#include <LibGfx/Painter.h>
#include <LibGfx/Path.h>

namespace Gfx {

namespace Detail {

static auto constexpr coverage_lut = [] {
    Array<u8, 256> coverage_lut {};
    for (u32 sample = 0; sample <= 255; sample++)
        coverage_lut[sample] = popcount(sample);
    return coverage_lut;
}();

template<unsigned SamplesPerPixel>
struct Sample {
    static_assert(!first_is_one_of(SamplesPerPixel, 8u, 16u, 32u), "EdgeFlagPathRasterizer: Invalid samples per pixel!");
};

// See paper for diagrams for how these offsets work, but they allow for nicely spread out samples in each pixel.
template<>
struct Sample<8> {
    using Type = u8;
    static constexpr Array nrooks_subpixel_offsets {
        (5.0f / 8.0f),
        (0.0f / 8.0f),
        (3.0f / 8.0f),
        (6.0f / 8.0f),
        (1.0f / 8.0f),
        (4.0f / 8.0f),
        (7.0f / 8.0f),
        (2.0f / 8.0f),
    };

    static u8 compute_coverage(Type sample)
    {
        return coverage_lut[sample];
    }
};

template<>
struct Sample<16> {
    using Type = u16;
    static constexpr Array nrooks_subpixel_offsets {
        (1.0f / 16.0f),
        (8.0f / 16.0f),
        (4.0f / 16.0f),
        (15.0f / 16.0f),
        (11.0f / 16.0f),
        (2.0f / 16.0f),
        (6.0f / 16.0f),
        (14.0f / 16.0f),
        (10.0f / 16.0f),
        (3.0f / 16.0f),
        (7.0f / 16.0f),
        (12.0f / 16.0f),
        (0.0f / 16.0f),
        (9.0f / 16.0f),
        (5.0f / 16.0f),
        (13.0f / 16.0f),
    };

    static u8 compute_coverage(Type sample)
    {
        return (
            coverage_lut[(sample >> 0) & 0xff]
            + coverage_lut[(sample >> 8) & 0xff]);
    }
};

template<>
struct Sample<32> {
    using Type = u32;
    static constexpr Array nrooks_subpixel_offsets {
        (28.0f / 32.0f),
        (13.0f / 32.0f),
        (6.0f / 32.0f),
        (23.0f / 32.0f),
        (0.0f / 32.0f),
        (17.0f / 32.0f),
        (10.0f / 32.0f),
        (27.0f / 32.0f),
        (4.0f / 32.0f),
        (21.0f / 32.0f),
        (14.0f / 32.0f),
        (31.0f / 32.0f),
        (8.0f / 32.0f),
        (25.0f / 32.0f),
        (18.0f / 32.0f),
        (3.0f / 32.0f),
        (12.0f / 32.0f),
        (29.0f / 32.0f),
        (22.0f / 32.0f),
        (7.0f / 32.0f),
        (16.0f / 32.0f),
        (1.0f / 32.0f),
        (26.0f / 32.0f),
        (11.0f / 32.0f),
        (20.0f / 32.0f),
        (5.0f / 32.0f),
        (30.0f / 32.0f),
        (15.0f / 32.0f),
        (24.0f / 32.0f),
        (9.0f / 32.0f),
        (2.0f / 32.0f),
        (19.0f / 32.0f),
    };

    static u8 compute_coverage(Type sample)
    {
        return (
            coverage_lut[(sample >> 0) & 0xff]
            + coverage_lut[(sample >> 8) & 0xff]
            + coverage_lut[(sample >> 16) & 0xff]
            + coverage_lut[(sample >> 24) & 0xff]);
    }
};

struct Edge {
    float x;
    int min_y;
    int max_y;
    float dxdy;
    i8 winding;
    Edge* next_edge;
};

}

template<unsigned SamplesPerPixel = 32>
class EdgeFlagPathRasterizer {
public:
    EdgeFlagPathRasterizer(IntSize);

    void fill(Painter&, Path const&, Color, Painter::WindingRule, FloatPoint offset = {});
    void fill(Painter&, Path const&, PaintStyle const&, float opacity, Painter::WindingRule, FloatPoint offset = {});

private:
    using SubpixelSample = Detail::Sample<SamplesPerPixel>;
    using SampleType = typename SubpixelSample::Type;

    static u8 coverage_to_alpha(u8 coverage)
    {
        constexpr auto alpha_shift = AK::log2(256 / SamplesPerPixel);
        if (!coverage)
            return 0;
        return (coverage << alpha_shift) - 1;
    }

    void fill_internal(Painter&, Path const&, auto color_or_function, Painter::WindingRule, FloatPoint offset);
    Detail::Edge* plot_edges_for_scanline(int scanline, auto plot_edge, Detail::Edge* active_edges = nullptr);
    void accumulate_even_odd_scanline(Painter&, int scanline, auto& color_or_function);
    void accumulate_non_zero_scanline(Painter&, int scanline, auto& color_or_function);
    Color scanline_color(int scanline, int offset, u8 alpha, auto& color_or_function);
    void write_pixel(Painter&, int scanline, int offset, SampleType sample, auto& color_or_function);

    struct WindingCounts {
        // NOTE: This only allows up to 256 winding levels. Increase this if required (i.e. to an i16).
        i8 counts[SamplesPerPixel];
    };

    IntSize m_size;
    IntPoint m_blit_origin;
    IntRect m_clip;

    Vector<SampleType> m_scanline;
    Vector<WindingCounts> m_windings;
    Vector<Detail::Edge*> m_edge_table;
};

extern template class EdgeFlagPathRasterizer<8>;
extern template class EdgeFlagPathRasterizer<16>;
extern template class EdgeFlagPathRasterizer<32>;

}
