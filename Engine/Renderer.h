#pragma once
#include "DeviceResources.h"
#include "Shader.h"
#include "Camera.h"
#include "Terrain.h"
#include "ModelClass.h"
#include "Light.h"
#include "ObjectManager.h"
#include "FractalObstacle.h"
#include "GameTimer.h"

class Renderer
{
public:
    Renderer(DX::DeviceResources* deviceResources);
    ~Renderer();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();
    void Reset();

    // Render takes non-const refs for objects that might update internal buffers on draw
    void Render(
        const Camera& camera,
        const DirectX::SimpleMath::Matrix& projection,
        Terrain& terrain,
        ModelClass& drone,
        const Light& sceneLight,
        const Light& droneLight,
        ObjectManager& objectManager,
        const std::vector<FractalObstacle>& fractalObstacles,
        GameTimer& gameTimer,
        int level
    );

private:
    void RenderScene(
        ID3D11DeviceContext* context,
        const Camera& camera,
        const DirectX::SimpleMath::Matrix& projection,
        Terrain& terrain,
        ModelClass& drone,
        const Light& sceneLight,
        const Light& droneLight,
        ObjectManager& objectManager,
        const std::vector<FractalObstacle>& fractalObstacles
    );

    void RenderUI(GameTimer& timer, int level, int matchedCount, int totalCount);

    DX::DeviceResources* m_deviceResources;

    // States
    std::unique_ptr<DirectX::CommonStates> m_states;
    std::unique_ptr<DirectX::SpriteBatch> m_sprites;
    std::unique_ptr<DirectX::SpriteFont> m_font;

    // Assets
    Shader m_BasicShaderPair;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_textureTerrain;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_textureObject;
};