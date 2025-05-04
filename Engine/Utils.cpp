#include "pch.h"
#include "Utils.h"
#include <random>

namespace Utils
{
    int Utils::GetRandomInt(int min, int max)
    {
        // Use random device as a seed
        std::random_device rd;

        // Mersenne Twister random number generator
        std::mt19937 gen(rd());

        // Uniform distribution in the specified range
        std::uniform_int_distribution<> dis(min, max);

        // Generate and return random number
        return dis(gen);
    }

    float Utils::GetRandomFloat(float min, float max)
    {
        static std::random_device rd;  // Seed
        static std::mt19937 engine(rd()); // Mersenne Twister engine
        std::uniform_real_distribution<float> dist(min, max);
        return dist(engine);
    }

    const float Utils::Lerp(float a, float b, float t)
    {
        // Linear interpolation
        return a + t * (b - a);
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
            BoundingOrientedBox dxA, dxB;

            // Set center and extents
            dxA.Center = a.center;
            dxA.Extents = a.extents;

            dxA.Orientation = XMFLOAT4
            (
                a.orientation.x,
                a.orientation.y,
                a.orientation.z,
                a.orientation.w
            );

            dxB.Center = b.center;
            dxB.Extents = b.extents;
            dxB.Orientation = XMFLOAT4
            (
                b.orientation.x,
                b.orientation.y,
                b.orientation.z,
                b.orientation.w
            );

            return dxA.Intersects(dxB);
        }
    }
}