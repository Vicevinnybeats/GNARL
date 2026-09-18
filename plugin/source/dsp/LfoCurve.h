#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>

namespace gnarl::dsp
{

/**
    A drawable breakpoint curve: the shape of an LFO.

    This is the feature the plugin is bought for, so the model is deliberately
    simple and total: a list of points, each with a time, a value, and a
    tension describing the segment that FOLLOWS it. Everything the editor can
    draw is expressible here, and everything expressible here is drawable.

    FIXED CAPACITY, no allocation. The curve is authored on the message thread
    and read on the audio thread, so it is copied by value into the engine
    rather than shared - a std::vector here would mean either a lock or a
    realtime allocation, and neither is acceptable.

    TIME IS NORMALISED 0..1 across one cycle of the LFO, not in seconds. The
    same curve therefore works at any rate, which is what lets a shape be
    reused across tempos and rate divisions.
*/
class LfoCurve
{
public:
    /** 32 points is more than any usable drawn shape and keeps the whole
        curve inside a couple of cache lines. */
    static constexpr int kMaxPoints = 32;

    enum class Shape
    {
        /** Interpolated, bent by tension. */
        curved = 0,
        /** Holds the start value until the next point. The staircase shapes
            riddim uses constantly. */
        step
    };

    struct Point
    {
        float time = 0.0f;      // 0..1
        float value = 0.0f;     // 0..1
        float tension = 0.0f;   // -1..1, shaping the segment after this point
        Shape shape = Shape::curved;
    };

    LfoCurve() { setDefaultRamp(); }

    /** A falling ramp: the shape a wobble starts from, and the one a producer
        expects to see when they open a fresh LFO. */
    void setDefaultRamp()
    {
        numPoints = 2;
        points[0] = { 0.0f, 1.0f, 0.0f, Shape::curved };
        points[1] = { 1.0f, 0.0f, 0.0f, Shape::curved };
    }

    void clear()
    {
        numPoints = 0;
    }

    /** Appends a point. Points must be added in ascending time order; the
        editor guarantees that, and evaluate() relies on it. */
    bool addPoint (const Point& point)
    {
        if (numPoints >= kMaxPoints)
            return false;

        points[static_cast<std::size_t> (numPoints++)] = point;
        return true;
    }

    int getNumPoints() const noexcept { return numPoints; }

    const Point& getPoint (int index) const noexcept
    {
        return points[static_cast<std::size_t> (juce::jlimit (0, juce::jmax (0, numPoints - 1), index))];
    }

    /**
        Evaluates the curve at a normalised phase.

        REAL-TIME SAFE: a linear scan over at most 32 points with no branching
        beyond the segment test. A binary search would be asymptotically
        better and measurably slower at this size.
    */
    float evaluate (float phase) const noexcept
    {
        if (numPoints == 0)
            return 0.0f;

        const auto t = juce::jlimit (0.0f, 1.0f, phase);

        if (numPoints == 1)
            return points[0].value;

        // Before the first point and after the last, hold the end values
        // rather than wrapping. A curve that does not start at 0 or end at 1
        // is legal, and wrapping would put a discontinuity in it.
        if (t <= points[0].time)
            return points[0].value;

        const auto lastIndex = static_cast<std::size_t> (numPoints - 1);

        if (t >= points[lastIndex].time)
            return points[lastIndex].value;

        for (int i = 0; i < numPoints - 1; ++i)
        {
            const auto& a = points[static_cast<std::size_t> (i)];
            const auto& b = points[static_cast<std::size_t> (i + 1)];

            if (t > b.time)
                continue;

            if (a.shape == Shape::step)
                return a.value;

            const auto span = b.time - a.time;

            if (span <= 0.0f)
                return b.value;

            const auto local = (t - a.time) / span;

            return a.value + (b.value - a.value) * applyTension (local, a.tension);
        }

        return points[lastIndex].value;
    }

    /**
        Bends a 0..1 ramp by a tension in -1..1.

        Positive tension holds low then rises late (exponential); negative
        rises fast then flattens (logarithmic); zero is exactly linear. The
        exponent form is used rather than a bezier because it is monotonic for
        every tension, and a non-monotonic segment in an LFO reads as a glitch
        rather than as a curve.
    */
    static float applyTension (float t, float tension) noexcept
    {
        const auto clampedTension = juce::jlimit (-1.0f, 1.0f, tension);

        if (juce::exactlyEqual (clampedTension, 0.0f))
            return t;

        // Tension +-1 maps to an exponent of 8 or 1/8, which is about as
        // extreme as stays useful; beyond that the segment is visually a step
        // and the step shape should be used instead.
        const auto exponent = std::exp2 (clampedTension * 3.0f);

        return std::pow (juce::jlimit (0.0f, 1.0f, t), exponent);
    }

    /** True when the curve is the untouched default. Lets the UI show a hint
        rather than an apparently-empty editor. */
    bool isDefaultRamp() const noexcept
    {
        return numPoints == 2
            && juce::exactlyEqual (points[0].value, 1.0f)
            && juce::exactlyEqual (points[1].value, 0.0f)
            && juce::exactlyEqual (points[0].tension, 0.0f);
    }

private:
    std::array<Point, kMaxPoints> points {};
    int numPoints = 0;
};

} // namespace gnarl::dsp
