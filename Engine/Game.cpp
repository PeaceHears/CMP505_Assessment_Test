#include "pch.h"
#include "Game.h"
#include "Utils.h"
#include "LSystem.h"
#include "FractalObstacle.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

Game::Game() noexcept(false)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
}

Game::~Game()
{
#ifdef DXTK_AUDIO
    if (m_audEngine) m_audEngine->Suspend();
#endif
}

void Game::Initialize(HWND window, int width, int height)
{
    m_deviceResources->RegisterDeviceNotify(shared_from_this());

    m_input.Initialise(window);
    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(m_deviceResources->GetD3DDevice(), m_deviceResources->GetD3DDeviceContext());

    // Lights
    m_Light.setAmbientColour(0.3f, 0.3f, 0.3f, 1.0f);
    m_Light.setDiffuseColour(1.0f, 1.0f, 1.0f, 1.0f);
    m_Light.setPosition(2.0f, 1.0f, 1.0f);
    m_Light.setDirection(-1.0f, -1.0f, 0.0f);

    m_Drone_Light.setAmbientColour(0.3f, 0.3f, 0.3f, 1.0f);
    m_Drone_Light.setDiffuseColour(0.5f, 0.5f, 0.5f, 1.0f);
    m_Drone_Light.setPosition(2.0f, 1.0f, 1.0f);

    // Camera
    m_cameraPosition = Vector3(0.0f, 5.0f, 4.0f);
    m_cameraRotation = Vector3(-90.0f, -180.0f, 0.0f);
    m_Camera01.setPosition(m_cameraPosition);
    m_Camera01.setRotation(m_cameraRotation);

    // Terrain
    m_Terrain.GeneratePerlinNoiseTerrain(m_deviceResources->GetD3DDevice(), 10.0f, 5);
    m_Terrain.GenerateVoronoiRegions(m_deviceResources->GetD3DDevice(), 5);

    ChangeTargetRegion();

    m_gameTimer.SetStartTime(m_timer, 300.0f);
    m_gameTimer.Start();

    SetupDrone();
    InitializeRegionRules();
    GenerateFractalObstacles();

    m_objectManager = std::make_unique<ObjectManager>();
    m_objectManager->CreateObjects(10, m_deviceResources->GetD3DDevice(), m_Terrain);

#ifdef DXTK_AUDIO
    AUDIO_ENGINE_FLAGS eflags = AudioEngine_Default;
#ifdef _DEBUG
    eflags = eflags | AudioEngine_Debug;
#endif
    m_audEngine = std::make_unique<AudioEngine>(eflags);
    m_waveBank = std::make_unique<WaveBank>(m_audEngine.get(), L"adpcmdroid.xwb");
    m_soundEffect = std::make_unique<SoundEffect>(m_audEngine.get(), L"MusicMono_adpcm.wav");
    m_effect1 = m_soundEffect->CreateInstance();
    m_effect1->Play(true);
#endif
}

void Game::Tick()
{
    m_input.Update();
    m_gameInputCommands = m_input.getGameInput();

    m_timer.Tick([&]() { Update(m_timer); });

    Render();
}

void Game::Update(DX::StepTimer const& timer)
{
    if (m_input.Quit())
    {
        ExitGame();
        return;
    }

    // Camera Mouse Control
    if (!isMouseHoveringOverImGui)
    {
        float deltaX = static_cast<float>(m_gameInputCommands.mouseX - m_lastMouseX);
        float deltaY = static_cast<float>(m_gameInputCommands.mouseY - m_lastMouseY);

        Vector3 rot = m_Camera01.getRotation();
        rot.y -= deltaX * 0.2f;
        rot.x = Utils::Clamp(rot.x - deltaY * 0.2f, -89.0f, 89.0f);
        m_Camera01.setRotation(rot);
    }
    m_lastMouseX = m_gameInputCommands.mouseX;
    m_lastMouseY = m_gameInputCommands.mouseY;

    UpdateCameraMovement();
    UpdateDroneMovement();

    // Update Systems
    m_objectManager->UpdateAll(m_Drone, m_Terrain, m_deviceResources->GetD3DDevice());

    if (IsWin()) OnWin();

    m_Camera01.Update();
    m_Terrain.Update();
    m_gameTimer.UpdateRemainingTime();

    if (m_gameTimer.IsExpired()) HandleTimerExpiration();

#ifdef DXTK_AUDIO
    if (m_retryAudio) {
        m_retryAudio = false;
        if (m_audEngine->Reset()) m_effect1->Play(true);
    }
    else if (!m_audEngine->Update()) {
        if (m_audEngine->IsCriticalError()) m_retryAudio = true;
    }
#endif
}

void Game::Render()
{
    if (m_timer.GetFrameCount() == 0) return;

    // GUI Frame Start
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    SetupImGUI();
    ImGui::Render();

    // Delegate to Renderer
    m_renderer->Render(
        m_Camera01,
        m_projection,
        m_Terrain,
        m_Drone,
        m_Light,
        m_Drone_Light,
        *m_objectManager,
        m_fractalObstacles,
        m_gameTimer,
        m_level
    );

    // Render ImGui on top
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    m_deviceResources->Present();
}

void Game::ExitGame()
{
    PostQuitMessage(0);
}

void Game::UpdateDroneMovement()
{
    Vector3 targetPos = m_Camera01.getPosition() + Vector3(0, -0.5f, -1.0f);
    bool checkProgress = false;

    // Check collision and snap to terrain if needed
    m_objectManager->CheckObjectCollisionWithTerrain(m_localDroneX, m_localDroneZ, targetPos, m_Drone, checkProgress, m_Terrain);

    if (checkProgress)
    {
        CheckDroneRegionProgress(m_localDroneX, m_localDroneZ);
    }

    m_Drone.SetPosition(targetPos);
    m_Drone.UpdateBoundingSphere();
}

void Game::CheckDroneRegionProgress(float localX, float localZ)
{
    Enums::COLOUR regionCol = m_Terrain.GetRegionColourAtPosition(localX, localZ);
    if (regionCol == m_targetRegionColour)
    {
        m_Drone.ChangeColour(m_deviceResources->GetD3DDevice(), regionCol, m_Terrain.GetVoronoiRegionColourVector(regionCol));
    }
}

// ... Resource creation and other simple helpers ...

void Game::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    m_renderer = std::make_unique<Renderer>(m_deviceResources.get());

    m_Terrain.Initialize(device, 128, 128);
    m_Terrain.SetScale(m_terrainScale);
    m_Terrain.SetTranslation(m_terrainTranslation);

    m_Drone.InitializeModel(device, "drone.obj", true);
    m_ObstacleModel.InitializeBox(device, 0.2f, 1.0f, 0.2f);
}

void Game::CreateWindowSizeDependentResources()
{
    auto size = m_deviceResources->GetOutputSize();
    float aspectRatio = float(size.right) / float(size.bottom);
    float fov = 70.0f * XM_PI / 180.0f;
    if (aspectRatio < 1.0f) fov *= 2.0f;

    m_projection = Matrix::CreatePerspectiveFieldOfView(fov, aspectRatio, 0.01f, 100.0f);
    if (m_renderer) m_renderer->CreateWindowSizeDependentResources();
}

void Game::SetupImGUI()
{
    ImGui::Begin("Procedural Terrain");
    isMouseHoveringOverImGui = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);

    ImGui::Text("Level: %d", m_level);
    if (ImGui::Button("Generate Terrain")) m_Terrain.GenerateHeightMap(m_deviceResources->GetD3DDevice());

    // Add other UI controls here as needed
    ImGui::End();
}

// Logic Helpers
void Game::ChangeTargetRegion() {
    m_targetRegionColour = m_Terrain.GetRandomVoronoiRegionColour();
    m_targetRegionColourVector = m_Terrain.GetVoronoiRegionColourVector(m_targetRegionColour);
}

bool Game::IsWin() { return m_objectManager->CheckAllColoursMatched(); }

void Game::OnWin() {
    m_level++;
    RestartScene();
    m_objectManager->CreateObjects(10, m_deviceResources->GetD3DDevice(), m_Terrain);
}

void Game::RestartScene() {
    m_Terrain.GeneratePerlinNoiseTerrain(m_deviceResources->GetD3DDevice(), 10.f, 5);
    m_Terrain.GenerateVoronoiRegions(m_deviceResources->GetD3DDevice(), 5);
    ChangeTargetRegion();
    m_Drone.ChangeColour(m_deviceResources->GetD3DDevice(), m_targetRegionColour, m_targetRegionColourVector);
    m_gameTimer.Restart();
}

void Game::HandleTimerExpiration() {
    if (IsWin()) OnWin(); else RestartScene();
}

void Game::SetupDrone() {
    m_Drone.SetScale(Vector3(0.1f));
    m_Drone.ChangeColour(m_deviceResources->GetD3DDevice(), m_targetRegionColour, m_targetRegionColourVector);
}

void Game::InitializeRegionRules() {
    m_regionRules.push_back({ m_Terrain.GetRandomVoronoiRegionColour(), ObstacleType::SPIKES, "F", { {'F', "F[+F]F[-F]F"} }, 3 });
    m_regionRules.push_back({ m_Terrain.GetRandomVoronoiRegionColour(), ObstacleType::CRYSTALS, "F", { {'F', "FF+[+F-F-F]-[-F+F+F]"} }, 4 });
    m_regionRules.push_back({ m_Terrain.GetRandomVoronoiRegionColour(), ObstacleType::VINES, "F", { {'F', "F[+FF][-FF]F"} }, 3 });
}

void Game::GenerateFractalObstacles() {
    m_fractalObstacles.clear();
    for (const auto& region : m_Terrain.GetVoronoiRegions()) {
        for (const auto& rule : m_regionRules) {
            if (region.colour != rule.regionColour) continue;
            float angle = 25.0f + Utils::GetRandomFloat(0, 20);
            float len = 1.5f + Utils::GetRandomFloat(0, 3);
            LSystem sys(rule.axiom, rule.rules, rule.iterations);
            sys.Generate();
            FractalObstacle obs(m_deviceResources->GetD3DDevice(), region.position, angle, len);
            obs.Generate(sys);
            m_fractalObstacles.push_back(obs);
            break;
        }
    }
}

void Game::UpdateCameraMovement() {
    const float speed = 0.1f;
    Vector3 move = Vector3::Zero;
    auto fwd = m_Camera01.getForward(); fwd.y = 0; fwd.Normalize();
    auto right = m_Camera01.getRight(); right.y = 0; right.Normalize();

    if (m_gameInputCommands.forward) move += fwd * speed;
    if (m_gameInputCommands.back)    move -= fwd * speed;
    if (m_gameInputCommands.left)    move -= right * speed;
    if (m_gameInputCommands.right)   move += right * speed;
    if (m_gameInputCommands.up)      move.y += speed;
    if (m_gameInputCommands.down)    move.y -= speed;

    m_Camera01.setPosition(m_Camera01.getPosition() + move);
}

// Device Notify
void Game::OnDeviceLost() { m_renderer->Reset(); }
void Game::OnDeviceRestored() { CreateDeviceDependentResources(); CreateWindowSizeDependentResources(); }
void Game::OnActivated() {}
void Game::OnDeactivated() {}
void Game::OnSuspending() {
#ifdef DXTK_AUDIO 
    if (m_audEngine) m_audEngine->Suspend();
#endif 
}
void Game::OnResuming() {
    m_timer.ResetElapsedTime();
#ifdef DXTK_AUDIO 
    if (m_audEngine) m_audEngine->Resume();
#endif 
}
void Game::OnWindowMoved() {
    auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}
void Game::OnWindowSizeChanged(int w, int h) {
    if (m_deviceResources->WindowSizeChanged(w, h)) CreateWindowSizeDependentResources();
}
void Game::GetDefaultSize(int& w, int& h) const { w = 800; h = 600; }
#ifdef DXTK_AUDIO
void Game::NewAudioDevice() { if (m_audEngine && !m_audEngine->IsAudioDevicePresent()) m_retryAudio = true; }
#endif