#pragma once
#include "ModelClass.h"
#include "LSystem.h"
#include <stack>

struct TurtleState
{
    DirectX::SimpleMath::Vector3 position;
    DirectX::SimpleMath::Vector3 direction;
    float segmentLength;
    float angle;
};

struct Segment
{
    DirectX::SimpleMath::Vector3 position;
    DirectX::SimpleMath::Vector3 rotation;
    float length;
};

class FractalObstacle
{
public:
    FractalObstacle(ID3D11Device* device, const DirectX::SimpleMath::Vector3& startPosition, 
        const float angle, const float segmentLength);
    void Generate(const LSystem& lsystem);
    const std::vector<Segment>& GetSegments() const { return m_segments; }
    void Render(ID3D11DeviceContext* deviceContext);

private:
    ID3D11Device* m_device;
    std::vector<Segment> m_segments;
    std::stack<TurtleState> stateStack;
    TurtleState currentState;
};