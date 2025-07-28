#include "pch.h"
#include "ObjectManager.h"
#include "modelclass.h"
#include "Terrain.h"
#include "Shader.h"
#include "Camera.h"
#include "Light.h"
#include "Utils.h"

ObjectManager::ObjectManager() {}
ObjectManager::~ObjectManager() {}

void ObjectManager::CreateObjects(int count, ID3D11Device* device, const Terrain& terrain)
{
    m_objects.clear(); // Clear any existing objects
    for (size_t i = 0; i < count; i++)
    {
        const float randomScale = Utils::GetRandomFloat(0.1f, 0.5f);
        const auto randomPosition = terrain.GetRandomPosition();
        const auto randomVoronoiRegionColour = terrain.GetRandomVoronoiRegionColour();

        auto model = std::make_unique<ModelClass>();
        model->InitializeModel(device, "drone.obj", true);
        model->ChangeColour(device,
            randomVoronoiRegionColour,
            terrain.GetVoronoiRegionColourVector(randomVoronoiRegionColour));
        model->SetPosition(randomPosition);
        model->SetScale(DirectX::SimpleMath::Vector3(randomScale, randomScale, randomScale));
        model->UpdateBoundingSphere();

        m_objects.push_back(std::move(model));
    }
}

void ObjectManager::UpdateAll(ModelClass& drone, const Terrain& terrain, ID3D11Device* device)
{
    const auto droneColour = drone.GetColour();
    m_matchedColourCount = 0;

    for (auto& object : m_objects)
    {
        auto localPositionX = 0.0f;
        auto localPositionY = 0.0f;
        auto worldPosition = object->GetPosition();
        auto isCheckingRegionProgress = true;

        CheckObjectCollisionWithTerrain(localPositionX, localPositionY, worldPosition, *object, isCheckingRegionProgress, terrain);

        if (drone.CheckCollision(*object))
        {
            if (!object->IsCollidingWithModel())
            {
                object->SetCollidingWithModel(true);
                object->ChangeColour(device, droneColour, terrain.GetVoronoiRegionColourVector(droneColour));
            }
        }
        else
        {
            object->SetCollidingWithModel(false);
        }

        const auto objectPosition = object->GetLocalPosition();
        const auto objectColour = object->GetColour();
        const auto regionColour = terrain.GetRegionColourAtPosition(objectPosition.x, objectPosition.z);

        if (objectColour == regionColour)
        {
            m_matchedColourCount++;
        }
    }
}

void ObjectManager::RenderAll(Shader& shader, 
    const ID3D11DeviceContext& context,
    const DirectX::SimpleMath::Matrix& projection, 
    const ID3D11ShaderResourceView& texture,
    const Camera& camera, 
    const Light& light)
{
    auto view = camera.getCameraMatrix();

    for (const auto& object : m_objects)
    {
        DirectX::SimpleMath::Matrix world = object->GetWorldMatrix();
        shader.EnableShader((ID3D11DeviceContext*) &context);
        shader.SetShaderParameters((ID3D11DeviceContext*)&context, &world, &view,
            (DirectX::SimpleMath::Matrix*) &projection, 
            (Light*) &light, 
            (ID3D11ShaderResourceView*) &texture);
        object->Render((ID3D11DeviceContext*)&context);
    }
}

void ObjectManager::CheckObjectCollisionWithTerrain(float& localPositionX, float& localPositionZ,
    DirectX::SimpleMath::Vector3& worldPosition,
    ModelClass& object,
    bool& isCheckingRegionProgress, 
    const Terrain& terrain)
{
	const auto terrainTranslation = terrain.GetTranslation();
	const auto terrainScale = terrain.GetScale();
    const auto positionY = (worldPosition.y - terrainTranslation.y) / terrainScale;

    localPositionX = (worldPosition.x - terrainTranslation.x) / terrainScale;
    localPositionZ = (worldPosition.z - terrainTranslation.z) / terrainScale;

    object.SetLocalPosition(DirectX::SimpleMath::Vector3(localPositionX, positionY, localPositionZ));
    object.SetCollidingWithTerrain(false);

    // Check if object is over the terrain
    const bool isOverTerrain = (localPositionX >= 0 && localPositionX < terrain.GetWidth() &&
        localPositionZ >= 0 && localPositionZ < terrain.GetHeight());

    if (isOverTerrain)
    {
        // Get terrain height in world space
        const float terrainLocalY = terrain.GetHeightAt(localPositionX, localPositionZ);
        const float terrainWorldY = (terrainLocalY * terrainScale) + terrainTranslation.y;

        // Model collision parameters
        const float modelRadius = object.GetBoundingRadius();
        const float modelBottom = worldPosition.y - modelRadius;

        // Check penetration from above or below
        const bool isPenetratingFromAbove = (modelBottom <= terrainWorldY);

        // Resolve collision based on movement direction
        if (isPenetratingFromAbove)
        {
            // From above: Push up to terrain surface
            worldPosition.y = terrainWorldY + modelRadius;
            object.SetCollidingWithTerrain(true);

            isCheckingRegionProgress = true;
        }
    }

    object.SetPosition(worldPosition);
}