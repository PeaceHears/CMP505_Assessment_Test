#pragma once
#include "modelclass.h"

namespace Utils
{
    int GetRandomInt(int min, int max);
    float GetRandomFloat(float min, float max);

    template <typename T>
    const T& Clamp(const T& value, const T& min, const T& max)
    {
        return (value < min) ? min : (value > max) ? max : value;
    }

    const float Lerp(float a, float b, float t);

    namespace Collision
    {
        bool SphereSphere(const ModelClass::BoundingSphere& a, const ModelClass::BoundingSphere& b);
        bool OBBOBB(const ModelClass::OBB& a, const ModelClass::OBB& b);
    }
}

