#pragma once
#include "pch.h"
#include "modelclass.h"
#include <string>

namespace Utils
{
    // --- Random Number Generation ---
    // Uses a static engine to avoid costly re-seeding
    int GetRandomInt(int min, int max);
    float GetRandomFloat(float min, float max);

    // --- Math Helpers ---
    template <typename T>
    const T& Clamp(const T& value, const T& min, const T& max)
    {
        return (value < min) ? min : (value > max) ? max : value;
    }

    float Lerp(float a, float b, float t);

    // --- String Helpers ---
    std::wstring ToWideString(const std::string& str);

    // --- Collision Helpers ---
    namespace Collision
    {
        bool SphereSphere(const ModelClass::BoundingSphere& a, const ModelClass::BoundingSphere& b);
        bool OBBOBB(const ModelClass::OBB& a, const ModelClass::OBB& b);
    }
}