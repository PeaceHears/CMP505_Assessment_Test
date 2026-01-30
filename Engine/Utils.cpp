#include "pch.h"
#include "Utils.h"
#include <random>

namespace Utils
{
    // Static random engine to persist state and performance
    static std::mt19937& GetRandomEngine()
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        return gen;
    }

    int GetRandomInt(int min, int max)
    {
        std::uniform_int_distribution<> dis(min, max);
        return dis(GetRandomEngine());
    }

    float GetRandomFloat(float min, float max)
    {
        std::uniform_real_distribution<float> dis(min, max);
        return dis(GetRandomEngine());
    }

    float Lerp(float a, float b, float t)
    {
        return a + t * (b - a);
    }

    std::wstring ToWideString(const std::string& str)
    {
        if (str.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }

    namespace Collision
    {
        bool SphereSphere(const ModelClass::BoundingSphere& a, const ModelClass::BoundingSphere& b)
        {
            DirectX::SimpleMath::Vector3 delta = a.center - b.center;
            float distanceSq = delta.LengthSquared();
            float radiusSum = a.radius + b.radius;
            return distanceSq <= (radiusSum * radiusSum);
        }

        bool OBBOBB(const ModelClass::OBB& a, const ModelClass::OBB& b)
        {
            DirectX::BoundingOrientedBox dxA, dxB;

            dxA.Center = a.center;
            dxA.Extents = a.extents;
            dxA.Orientation = a.orientation;

            dxB.Center = b.center;
            dxB.Extents = b.extents;
            dxB.Orientation = b.orientation;

            return dxA.Intersects(dxB);
        }
    }
}