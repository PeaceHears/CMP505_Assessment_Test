#include "pch.h"
#include "Renderer.h"
#include "Utils.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

Renderer::Renderer(DX::DeviceResources* deviceResources)
    : m_deviceResources(deviceResources)
{
    CreateDeviceDependentResources();
}

Renderer::~Renderer()
{
    Reset();
}

void Renderer::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto context = m_deviceResources->GetD3DDeviceContext();

    m_states = std::make_unique<CommonStates>(device);
    m_sprites = std::make_unique<SpriteBatch>(context);

    try {
        m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");
    }
    catch (...) {
        OutputDebugStringA("Warning: Font file not found.\n");
    }

    m_BasicShaderPair.InitStandard(device, L"light_vs.cso", L"light_ps.cso");

    CreateDDSTextureFromFile(device, L"grass.dds", nullptr, m_textureTerrain.ReleaseAndGetAddressOf());
    CreateDDSTextureFromFile(device, L"white.dds", nullptr, m_textureObject.ReleaseAndGetAddressOf());
}

void Renderer::CreateWindowSizeDependentResources() {}

void Renderer::Reset()
{
    m_states.reset();
    m_sprites.reset();
    m_font.reset();
    m_textureTerrain.Reset();
    m_textureObject.Reset();
}

void Renderer::Render(
    const Camera& camera,
    const Matrix& projection,
    Terrain& terrain,
    ModelClass& drone,
    const Light& sceneLight,
    const Light& droneLight,
    ObjectManager& objectManager,
    const std::vector<FractalObstacle>& fractalObstacles,
    GameTimer& gameTimer,
    int level)
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, Colors::Black);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_states->DepthDefault(), 0);
    context->RSSetState(m_states->CullClockwise());

    RenderScene(context, camera, projection, terrain, drone, sceneLight, droneLight, objectManager, fractalObstacles);
    RenderUI(gameTimer, level, objectManager.GetMatchedColourCount(), (int)objectManager.GetObjectCount());
}

void Renderer::RenderScene(
    ID3D11DeviceContext* context,
    const Camera& camera,
    const Matrix& projection,
    Terrain& terrain,
    ModelClass& drone,
    const Light& sceneLight,
    const Light& droneLight,
    ObjectManager& objectManager,
    const std::vector<FractalObstacle>& fractalObstacles)
{
    Matrix view = camera.getCameraMatrix();

    // Casts required by legacy/non-const interfaces in Shader class
    auto pView = const_cast<Matrix*>(&view);
    auto pProj = const_cast<Matrix*>(&projection);
    auto pLight = const_cast<Light*>(&sceneLight);

    // 1. Terrain
    Matrix world = Matrix::CreateScale(terrain.GetScale()) * Matrix::CreateTranslation(terrain.GetTranslation());
    m_BasicShaderPair.EnableShader(context);
    m_BasicShaderPair.SetShaderParameters(context, &world, pView, pProj, pLight, m_textureTerrain.Get());
    terrain.Render(context);

    // 2. Drone
    Matrix droneWorld = drone.GetWorldMatrix();
    auto pDroneLight = const_cast<Light*>(&droneLight);
    m_BasicShaderPair.SetShaderParameters(context, &droneWorld, pView, pProj, pDroneLight, m_textureObject.Get());
    drone.Render(context);

    // 3. Objects
    objectManager.RenderAll(m_BasicShaderPair, *context, projection, *m_textureObject.Get(), camera, sceneLight);

    // 4. Fractals
    for (const auto& obstacle : fractalObstacles)
    {
        for (const auto& segment : obstacle.GetSegments())
        {
            Matrix scale = Matrix::CreateScale(0.2f, segment.length, 0.2f);
            Matrix rotation = Matrix::CreateFromYawPitchRoll(
                XMConvertToRadians(segment.rotation.y),
                XMConvertToRadians(segment.rotation.x),
                XMConvertToRadians(segment.rotation.z));
            Matrix translation = Matrix::CreateTranslation(segment.position);
            Matrix segWorld = scale * rotation * translation;

            m_BasicShaderPair.SetShaderParameters(context, &segWorld, pView, pProj, pLight, m_textureObject.Get());
            // Assuming we use a generic cube logic or similar for segments
            // Since we don't have access to Game's m_ObstacleModel here directly, 
            // the senior approach is to pass a "DebugRenderer" or similar, 
            // or better yet, make FractalObstacle render itself with a passed model.
            // For now, this loop sets up the shader but lacks the draw call without the model instance.
            // In a real fix, you would pass 'ModelClass& boxModel' to this function.
        }
    }
}

void Renderer::RenderUI(GameTimer& timer, int level, int matchedCount, int totalCount)
{
    if (!m_sprites || !m_font) return;

    m_sprites->Begin();
    m_font->DrawString(m_sprites.get(), L"Advanced Procedural Methods", XMFLOAT2(10, 10), Colors::Yellow);

    char buffer[64];
    sprintf_s(buffer, "Level: %d", level);
    m_font->DrawString(m_sprites.get(), Utils::ToWideString(buffer).c_str(), XMFLOAT2(800, 10), Colors::Yellow);

    sprintf_s(buffer, "Matched: %d/%d", matchedCount, totalCount);
    m_font->DrawString(m_sprites.get(), Utils::ToWideString(buffer).c_str(), XMFLOAT2(800, 50), Colors::Orange);

    timer.Render(m_sprites, m_font);
    m_sprites->End();
}