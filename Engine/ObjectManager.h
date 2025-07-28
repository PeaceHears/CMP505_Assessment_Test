#pragma once

#include "pch.h"

// Forward declarations
class ModelClass;
class Terrain;
class Shader;
class Light;
class Camera;

class ObjectManager
{
public:
    ObjectManager();
    ~ObjectManager();

    // Make the class non-copyable and non-movable
    ObjectManager(const ObjectManager&) = delete;
    ObjectManager& operator=(const ObjectManager&) = delete;
    ObjectManager(ObjectManager&&) = delete;
    ObjectManager& operator=(ObjectManager&&) = delete;

    // Public interface
    void CreateObjects(int count, ID3D11Device* device, const Terrain& terrain);

    void UpdateAll(ModelClass& drone, const Terrain& terrain, ID3D11Device* device);

    void RenderAll(Shader& shader,
        const ID3D11DeviceContext& context,
        const DirectX::SimpleMath::Matrix& projection,
        const ID3D11ShaderResourceView& texture,
        const Camera& camera,
        const Light& light);

    void CheckObjectCollisionWithTerrain(float& localPositionX, float& localPositionZ,
        DirectX::SimpleMath::Vector3& worldPosition,
        ModelClass& object,
        bool& isCheckingRegionProgress,
        const Terrain& terrain);

    // Accessors for game state
    size_t GetObjectCount() const { return m_objects.size(); }
    int GetMatchedColourCount() const { return m_matchedColourCount; }
    const bool CheckAllColoursMatched() const { return m_matchedColourCount == static_cast<int>(m_objects.size()); }

private:
    void CheckObjectCollisionWithTerrain(DirectX::SimpleMath::Vector3& worldPosition, ModelClass& model, const Terrain& terrain);

    // Private Member Variables
    std::vector<std::unique_ptr<ModelClass>> m_objects;
    int m_matchedColourCount = 0;
};