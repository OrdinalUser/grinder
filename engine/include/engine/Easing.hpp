#pragma once
#include <cmath>

namespace Engine::Easing {

    // Core function type
    using Func = float(*)(float);

    // --- Linear ---
    inline float Linear(float t) { return t; }

    // --- Quadratic ---
    inline float InQuad(float t) { return t * t; }
    inline float OutQuad(float t) { return t * (2.0f - t); }
    inline float InOutQuad(float t) {
        return (t < 0.5f) ? 2*t*t : -1 + (4 - 2*t)*t;
    }

    // --- Cubic ---
    inline float InCubic(float t) { return t*t*t; }
    inline float OutCubic(float t) {
        t -= 1.f;
        return t*t*t + 1.f;
    }
    inline float InOutCubic(float t) {
        return t < 0.5f ? 4*t*t*t : (t-1)*(2*t-2)*(2*t-2) + 1;
    }

    // --- Quart ---
    inline float InQuart(float t) { return t*t*t*t; }
    inline float OutQuart(float t) { t-=1; return 1 - t*t*t*t; }
    inline float InOutQuart(float t) {
        return (t < 0.5f) ? 8*t*t*t*t : 1 - 8 * (t-1)*(t-1)*(t-1)*(t-1);
    }

    // --- Quint ---
    inline float InQuint(float t) { return t*t*t*t*t; }
    inline float OutQuint(float t) { t-=1; return 1 + t*t*t*t*t; }

    // --- Sine ---
    inline float InSine(float t) { return 1 - cosf((t * 3.1415926f) / 2); }
    inline float OutSine(float t) { return sinf((t * 3.1415926f) / 2); }
    inline float InOutSine(float t) { return -(cosf(3.1415926f*t) - 1) * 0.5f; }

    // (Expand if needed…)

} // namespace Engine::Easing
