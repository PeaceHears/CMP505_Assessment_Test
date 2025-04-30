#pragma once

namespace Utils
{
    int GetRandomInt(int min, int max);

    template <typename T>
    const T& Clamp(const T& value, const T& min, const T& max)
    {
        return (value < min) ? min : (value > max) ? max : value;
    }

    const float Lerp(float a, float b, float t);
}

