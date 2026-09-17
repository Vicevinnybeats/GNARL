#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/WarpProcessor.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace gnarl::dsp;
using Mode = warp::Mode;

namespace
{
    /** Every mode, so a newly added one cannot escape the sweeps below. */
    std::vector<Mode> allModes()
    {
        std::vector<Mode> modes;

        for (int i = 0; i < static_cast<int> (Mode::count); ++i)
            modes.push_back (static_cast<Mode> (i));

        return modes;
    }
}

TEST_CASE ("Amount zero is exact identity for every mode", "[warp]")
{
    // Not "close to" identity: a patch with warp off must pay nothing and
    // change nothing, and a mode that drifts at zero would detune every note.
    for (const auto mode : allModes())
    {
        for (int i = 0; i <= 64; ++i)
        {
            const auto phase = static_cast<float> (i) / 64.0f;
            INFO ("mode " << static_cast<int> (mode) << " phase " << phase);
            CHECK (warp::applyPhase (phase, mode, 0.0f) == phase);
        }
    }
}

TEST_CASE ("Every phase warp stays inside [0, 1]", "[warp]")
{
    // The result indexes a table, so escaping the range would read out of
    // bounds. Swept across the full amount range including the extremes.
    for (const auto mode : allModes())
    {
        for (int a = -16; a <= 16; ++a)
        {
            const auto amount = static_cast<float> (a) / 16.0f;

            for (int i = 0; i < 256; ++i)
            {
                const auto phase = static_cast<float> (i) / 256.0f;
                const auto warped = warp::applyPhase (phase, mode, amount);

                INFO ("mode " << static_cast<int> (mode)
                      << " amount " << amount << " phase " << phase
                      << " -> " << warped);

                REQUIRE (std::isfinite (warped));
                REQUIRE (warped >= 0.0f);
                REQUIRE (warped <= 1.0f);
            }
        }
    }
}

TEST_CASE ("Out-of-range amounts are clamped, not wrapped", "[warp]")
{
    // Modulation overshoots. A clamp keeps the sound at the edge of the
    // control's range; wrapping would jump it to the opposite extreme.
    for (const auto mode : allModes())
    {
        for (int i = 0; i < 32; ++i)
        {
            const auto phase = static_cast<float> (i) / 32.0f;

            CHECK (warp::applyPhase (phase, mode, 5.0f)
                   == warp::applyPhase (phase, mode, 1.0f));
            CHECK (warp::applyPhase (phase, mode, -5.0f)
                   == warp::applyPhase (phase, mode, -1.0f));
        }
    }
}

TEST_CASE ("FM and ring mod are not phase warps", "[warp]")
{
    // They need the other oscillator's output, so they cannot be applied as a
    // phase transform and must be left alone here.
    CHECK_FALSE (warp::isPhaseWarp (Mode::fmFromOther));
    CHECK_FALSE (warp::isPhaseWarp (Mode::ringMod));

    for (int i = 0; i < 32; ++i)
    {
        const auto phase = static_cast<float> (i) / 32.0f;
        CHECK (warp::applyPhase (phase, Mode::fmFromOther, 1.0f) == phase);
        CHECK (warp::applyPhase (phase, Mode::ringMod, 1.0f) == phase);
    }

    for (const auto mode : allModes())
        if (mode != Mode::fmFromOther && mode != Mode::ringMod && mode != Mode::count)
            CHECK (warp::isPhaseWarp (mode));
}

TEST_CASE ("Sync multiplies the read rate", "[warp]")
{
    // At full sync the cycle should be read 8 times per period, so a quarter
    // of the way through the phase has already wrapped twice.
    const auto atQuarter = warp::applyPhase (0.25f, Mode::sync, 1.0f);
    CHECK (atQuarter == Catch::Approx (0.0f).margin (1.0e-5));

    // Monotonic within each sub-cycle: the reset is the only discontinuity.
    auto previous = warp::applyPhase (0.0f, Mode::sync, 1.0f);
    int resets = 0;

    for (int i = 1; i < 1024; ++i)
    {
        const auto current = warp::applyPhase (static_cast<float> (i) / 1024.0f,
                                               Mode::sync, 1.0f);
        if (current < previous)
            ++resets;

        previous = current;
    }

    // 8x sync means 7 wraps inside one cycle.
    CHECK (resets == 7);
}

TEST_CASE ("Mirror makes the waveform even", "[warp]")
{
    // Playing the cycle forwards then backwards means phase(t) == phase(1-t).
    for (int i = 1; i < 128; ++i)
    {
        const auto phase = static_cast<float> (i) / 256.0f;
        const auto mirrored = warp::applyPhase (phase, Mode::mirror, 1.0f);
        const auto opposite = warp::applyPhase (1.0f - phase, Mode::mirror, 1.0f);

        INFO ("phase " << phase);
        CHECK (mirrored == Catch::Approx (opposite).margin (1.0e-5));
    }
}

TEST_CASE ("Quantize produces a finite number of distinct values", "[warp]")
{
    // The point of the mode: a stepped phase. If it produced a continuum it
    // would just be a slightly wrong identity.
    std::vector<float> seen;

    for (int i = 0; i < 4096; ++i)
    {
        const auto value = warp::applyPhase (static_cast<float> (i) / 4096.0f,
                                             Mode::quantize, 1.0f);

        if (std::find_if (seen.begin(), seen.end(), [value] (float v)
            { return std::abs (v - value) < 1.0e-6f; }) == seen.end())
        {
            seen.push_back (value);
        }
    }

    CHECK (seen.size() == 2);   // amount 1.0 -> 2 steps
}

TEST_CASE ("Asymmetry moves the cycle midpoint", "[warp]")
{
    // Positive amount should push the midpoint later in the cycle, which is
    // what generates even harmonics from a symmetric table.
    const auto positive = warp::applyPhase (0.5f, Mode::asymmetry, 0.8f);
    const auto negative = warp::applyPhase (0.5f, Mode::asymmetry, -0.8f);

    CHECK (positive < 0.5f);
    CHECK (negative > 0.5f);
}

TEST_CASE ("PWM narrows the active part of the cycle", "[warp]")
{
    // At high amount the cycle completes early and then holds, which is the
    // waveform equivalent of a narrow pulse.
    const auto warped = warp::applyPhase (0.5f, Mode::pwm, 0.9f);
    CHECK (warped == Catch::Approx (1.0f).margin (1.0e-5));

    // And it must actually reach the hold, not merely approach it.
    CHECK (warp::applyPhase (0.99f, Mode::pwm, 1.0f) == Catch::Approx (1.0f).margin (1.0e-5));
}

TEST_CASE ("Bend directions are opposites", "[warp]")
{
    // Bend + steepens the leading edge, bend - flattens it. At the same
    // amount they must fall on opposite sides of the unwarped phase.
    for (int i = 1; i < 32; ++i)
    {
        const auto phase = static_cast<float> (i) / 32.0f;
        const auto plus = warp::applyPhase (phase, Mode::bendPlus, 1.0f);
        const auto minus = warp::applyPhase (phase, Mode::bendMinus, 1.0f);

        INFO ("phase " << phase);
        CHECK (plus >= phase);
        CHECK (minus <= phase);
    }
}

TEST_CASE ("Bandwidth expansion is at least 1 and rises with amount", "[warp][aliasing]")
{
    // This figure biases mip level selection. Below 1 it would select a FINER
    // level than the unwarped signal needs, which is the one direction that
    // causes audible aliasing rather than merely lost brightness.
    for (const auto mode : allModes())
    {
        const auto atZero = warp::getBandwidthExpansion (mode, 0.0f);
        const auto atFull = warp::getBandwidthExpansion (mode, 1.0f);

        INFO ("mode " << static_cast<int> (mode));

        CHECK (atZero >= 1.0f);
        CHECK (atFull >= atZero);
        CHECK (std::isfinite (atFull));

        // Symmetric in amount: a negative warp expands bandwidth as much as a
        // positive one.
        CHECK (warp::getBandwidthExpansion (mode, -1.0f) == atFull);
    }

    // Warp off must cost nothing.
    CHECK (warp::getBandwidthExpansion (Mode::off, 1.0f) == 1.0f);

    // Quantize is the harshest mode and must be reported as such, or it will
    // alias.
    for (const auto mode : allModes())
        if (mode != Mode::quantize && mode != Mode::count)
            CHECK (warp::getBandwidthExpansion (Mode::quantize, 1.0f)
                   >= warp::getBandwidthExpansion (mode, 1.0f));
}

TEST_CASE ("Phase wrapping handles multiple cycles either way", "[warp]")
{
    CHECK (warp::wrapPhase (0.25f) == Catch::Approx (0.25f));
    CHECK (warp::wrapPhase (1.25f) == Catch::Approx (0.25f));
    CHECK (warp::wrapPhase (3.25f) == Catch::Approx (0.25f));
    CHECK (warp::wrapPhase (-0.75f) == Catch::Approx (0.25f));
    CHECK (warp::wrapPhase (-2.75f) == Catch::Approx (0.25f));

    // Exactly 1.0 wraps to 0, never to 1: a phase of 1.0 would read the guard
    // sample as if it were a real sample.
    CHECK (warp::wrapPhase (1.0f) == 0.0f);
}
