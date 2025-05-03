#include "pch.h"
#include "FractalObstacle.h"

FractalObstacle::FractalObstacle(ID3D11Device* device, const DirectX::SimpleMath::Vector3& startPosition,
    const float angle, const float segmentLength)
    : m_device(device)
{
    currentState.position = startPosition;
    currentState.direction = DirectX::SimpleMath::Vector3::UnitY; // Initial growth direction
    currentState.segmentLength = segmentLength;
    currentState.angle = angle; // Degrees
}

void FractalObstacle::Generate(const LSystem& lsystem)
{
    const std::string& lstring = lsystem.GetCurrentString();

    for (char c : lstring)
    {
        switch (c)
        {
            case 'F':
            { // Draw segment
                Segment segment;
                segment.position = currentState.position;

                // Calculate rotation from direction (example for Y-axis alignment)
                float pitch = atan2(currentState.direction.z, currentState.direction.y) * 180.0f / XM_PI;
                segment.rotation = DirectX::SimpleMath::Vector3(pitch, 0.0f, 0.0f);

                segment.length = currentState.segmentLength; // Store the current length
                m_segments.push_back(segment);

                currentState.position += currentState.direction * currentState.segmentLength;

                break;
            }
            case '+': // Turn right
                currentState.direction = DirectX::SimpleMath::Vector3::Transform
                (
                    currentState.direction,
                    DirectX::SimpleMath::Matrix::CreateRotationZ(DirectX::XMConvertToRadians(currentState.angle))
                );

                break;
            case '-': // Turn left
                currentState.direction = DirectX::SimpleMath::Vector3::Transform
                (
                    currentState.direction,
                    DirectX::SimpleMath::Matrix::CreateRotationZ(DirectX::XMConvertToRadians(-currentState.angle))
                );

                break;
            case '[': // Save state
                stateStack.push(currentState);
                currentState.segmentLength *= 0.8f; // Shrink branches

                break;
            case ']': // Restore state
                currentState = stateStack.top();
                stateStack.pop();

                break;
        }
    }
}