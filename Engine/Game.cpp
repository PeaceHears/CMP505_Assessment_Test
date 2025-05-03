//
// Game.cpp
//

#include "pch.h"
#include "Game.h"

#include "Utils.h"
#include "LSystem.h"
#include "FractalObstacle.h"

//toreorganise
#include <fstream>

extern void ExitGame();

using namespace DirectX;
using namespace DirectX::SimpleMath;
using namespace ImGui;

using Microsoft::WRL::ComPtr;

Game::Game() noexcept(false)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);

    m_lastMouseX = 0;
    m_lastMouseY = 0;
}

Game::~Game()
{
#ifdef DXTK_AUDIO
    if (m_audEngine)
    {
        m_audEngine->Suspend();
    }
#endif
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window, int width, int height)
{

	m_input.Initialise(window);

    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

	//setup imgui.  its up here cos we need the window handle too
	//pulled from imgui directx11 example
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(window);		//tie to our window
	ImGui_ImplDX11_Init(m_deviceResources->GetD3DDevice(), m_deviceResources->GetD3DDeviceContext());	//tie to directx

	m_fullscreenRect.left = 0;
	m_fullscreenRect.top = 0;
	m_fullscreenRect.right = 800;
	m_fullscreenRect.bottom = 600;

	m_CameraViewRect.left = 500;
	m_CameraViewRect.top = 0;
	m_CameraViewRect.right = 800;
	m_CameraViewRect.bottom = 240;

	//setup light
	m_Light.setAmbientColour(0.3f, 0.3f, 0.3f, 1.0f);
	m_Light.setDiffuseColour(1.0f, 1.0f, 1.0f, 1.0f);
	m_Light.setPosition(2.0f, 1.0f, 1.0f);
	m_Light.setDirection(-1.0f, -1.0f, 0.0f);

    m_Drone_Light.setAmbientColour(0.3f, 0.3f, 0.3f, 1.0f);
    m_Drone_Light.setDiffuseColour(0.5f, 0.5f, 0.5f, 1.0f);
    m_Drone_Light.setPosition(2.0f, 1.0f, 1.0f);
    m_Drone_Light.setDirection(-1.0f, -1.0f, 0.0f);

	//setup camera
	m_Camera01.setPosition(Vector3(0.0f, 0.0f, 4.0f));
	m_Camera01.setRotation(Vector3(-90.0f, -180.0f, 0.0f));	//orientation is -90 becuase zero will be looking up at the sky straight up. 

	m_Terrain.GeneratePerlinNoiseTerrain(m_deviceResources->GetD3DDevice(), 10.0f, 5);
    m_Terrain.GenerateVoronoiRegions(m_deviceResources->GetD3DDevice(), 5);

	SelectTargetRegion();
    m_Drone.ChangeColour(m_deviceResources->GetD3DDevice(), m_targetRegionColour, m_targetRegionColourVector);

	m_gameTimer.SetStartTime(m_timer, 300.0f);
    m_gameTimer.Start();

    SetupDrone();

    //InitializeRegionRules();
    //GenerateFractalObstacles();
	
#ifdef DXTK_AUDIO
    // Create DirectXTK for Audio objects
    AUDIO_ENGINE_FLAGS eflags = AudioEngine_Default;
#ifdef _DEBUG
    eflags = eflags | AudioEngine_Debug;
#endif

    m_audEngine = std::make_unique<AudioEngine>(eflags);

    m_audioEvent = 0;
    m_audioTimerAcc = 10.f;
    m_retryDefault = false;

    m_waveBank = std::make_unique<WaveBank>(m_audEngine.get(), L"adpcmdroid.xwb");

    m_soundEffect = std::make_unique<SoundEffect>(m_audEngine.get(), L"MusicMono_adpcm.wav");
    m_effect1 = m_soundEffect->CreateInstance();
    m_effect2 = m_waveBank->CreateInstance(10);

    m_effect1->Play(true);
    m_effect2->Play();
#endif
}

#pragma region Frame Update
// Executes the basic game loop.
void Game::Tick()
{
	//take in input
	m_input.Update();								//update the hardware
	m_gameInputCommands = m_input.getGameInput();	//retrieve the input for our game
	
	//Update all game objects
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

	//Render all game content. 
    Render();

#ifdef DXTK_AUDIO
    // Only update audio engine once per frame
    if (!m_audEngine->IsCriticalError() && m_audEngine->Update())
    {
        // Setup a retry in 1 second
        m_audioTimerAcc = 1.f;
        m_retryDefault = true;
    }
#endif

	
}

// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{	
	//this is hacky,  i dont like this here.  
	auto device = m_deviceResources->GetD3DDevice();

    // Calculate mouse deltas
    static int lastMouseX = m_gameInputCommands.mouseX;
    static int lastMouseY = m_gameInputCommands.mouseY;

    // Calculate delta
    float mouseDeltaX = static_cast<float>(m_gameInputCommands.mouseX - lastMouseX);
    float mouseDeltaY = static_cast<float>(m_gameInputCommands.mouseY - lastMouseY);

    // Update last mouse positions
    lastMouseX = m_gameInputCommands.mouseX;
    lastMouseY = m_gameInputCommands.mouseY;

    // Camera Rotation
    Vector3 rotation = m_Camera01.getRotation();

    // Sensitivity adjustment for rotation
    float sensitivity = 0.1f;

    // Horizontal rotation (Yaw) - left/right mouse movement
    rotation.y -= mouseDeltaX * sensitivity;

    //// Vertical rotation (Pitch) - up/down mouse movement
    //rotation.x -= mouseDeltaY * sensitivity;

    //// Clamp vertical rotation to prevent flipping
    //rotation.x = std::max(-89.0f, std::min(rotation.x, 89.0f));

	rotation.x = -50.0f; // Set fixed pitch angle

    // Set the new rotation for the camera
    m_Camera01.setRotation(rotation);

    UpdateCameraMovement();
    UpdateDroneMovement();

	m_Camera01.Update();	//camera update.
	m_Terrain.Update();		//terrain update.  doesnt do anything at the moment. 

	m_view = m_Camera01.getCameraMatrix();
	m_world = Matrix::Identity;

	/*create our UI*/
	SetupGUI();

    m_gameTimer.UpdateRemainingTime();

    if (m_gameTimer.IsExpired())
    {
        HandleTimerExpiration();
    }

#ifdef DXTK_AUDIO
    m_audioTimerAcc -= (float)timer.GetElapsedSeconds();
    if (m_audioTimerAcc < 0)
    {
        if (m_retryDefault)
        {
            m_retryDefault = false;
            if (m_audEngine->Reset())
            {
                // Restart looping audio
                m_effect1->Play(true);
            }
        }
        else
        {
            m_audioTimerAcc = 4.f;

            m_waveBank->Play(m_audioEvent++);

            if (m_audioEvent >= 11)
                m_audioEvent = 0;
        }
    }
#endif

  
	if (m_input.Quit())
	{
		ExitGame();
	}
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Game::Render()
{	
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    Clear();

    m_deviceResources->PIXBeginEvent(L"Render");
    auto context = m_deviceResources->GetD3DDeviceContext();
	auto renderTargetView = m_deviceResources->GetRenderTargetView();
	auto depthTargetView = m_deviceResources->GetDepthStencilView();

	//Set Rendering states. 
	context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
	context->OMSetDepthStencilState(m_states->DepthDefault(), 0);
	context->RSSetState(m_states->CullClockwise());
//	context->RSSetState(m_states->Wireframe());

	//Render terrain
	m_world = SimpleMath::Matrix::Identity; //set world back to identity
	SimpleMath::Matrix newPosition3 = SimpleMath::Matrix::CreateTranslation(m_terrainTranslation);
	SimpleMath::Matrix newScale = SimpleMath::Matrix::CreateScale(0.1f);
	m_world = m_world * newScale * newPosition3;

	m_BasicShaderPair.EnableShader(context);
	m_BasicShaderPair.SetShaderParameters(context, &m_world, &m_view, &m_projection, &m_Light, m_texture1.Get());
	m_Terrain.Render(context);

    // Render drone
    SimpleMath::Matrix droneWorldMatrix = m_Drone.GetWorldMatrix();
    SimpleMath::Matrix droneScale = SimpleMath::Matrix::CreateScale(0.1f);
    //droneWorldMatrix *= droneScale;

    m_BasicShaderPair.EnableShader(context);
    m_BasicShaderPair.SetShaderParameters(context, &droneWorldMatrix, &m_view, &m_projection, &m_Drone_Light, m_texture2.Get());
    m_Drone.Render(context);

    //RenderFractalObstacles(context);

    // Draw Title to the screen
    m_sprites->Begin();
    m_font->DrawString(m_sprites.get(), L"Advanced Procedural Methods", XMFLOAT2(10, 10), Colors::Yellow);
    m_sprites->End();

    // Draw Game Timer to the screen 
    m_gameTimer.Render(m_sprites, m_font);

    DrawLevelIndicator();

	//render our GUI
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	

    // Show the new frame.
    m_deviceResources->Present();
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    m_deviceResources->PIXBeginEvent(L"Clear");

    // Clear the views.
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, Colors::CornflowerBlue);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    // Set the viewport.
    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    m_deviceResources->PIXEndEvent();
}

#pragma endregion

#pragma region Message Handlers
// Message handlers
void Game::OnActivated()
{
}

void Game::OnDeactivated()
{
}

void Game::OnSuspending()
{
#ifdef DXTK_AUDIO
    m_audEngine->Suspend();
#endif
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();

#ifdef DXTK_AUDIO
    m_audEngine->Resume();
#endif
}

void Game::OnWindowMoved()
{
    auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void Game::OnWindowSizeChanged(int width, int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();
}

#ifdef DXTK_AUDIO
void Game::NewAudioDevice()
{
    if (m_audEngine && !m_audEngine->IsAudioDevicePresent())
    {
        // Setup a retry in 1 second
        m_audioTimerAcc = 1.f;
        m_retryDefault = true;
    }
}
#endif

// Properties
void Game::GetDefaultSize(int& width, int& height) const
{
    width = 800;
    height = 600;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto device = m_deviceResources->GetD3DDevice();

    m_states = std::make_unique<CommonStates>(device);
    m_fxFactory = std::make_unique<EffectFactory>(device);
    m_sprites = std::make_unique<SpriteBatch>(context);
    m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");
	m_batch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(context);

	//setup our terrain
	m_Terrain.Initialize(device, 128, 128);
    
	//setup our test model
	m_BasicModel.InitializeSphere(device);
    m_Drone.InitializeModel(device,"drone.obj", true);
    m_ObstacleModel.InitializeBox(device, 0.2f, 1.0f, 0.2f);

	//load and set up our Vertex and Pixel Shaders
	m_BasicShaderPair.InitStandard(device, L"light_vs.cso", L"light_ps.cso");
    m_Drone_Light_Shader.InitStandard(device, L"light_vs.cso", L"light_ps.cso");

	//load Textures
	CreateDDSTextureFromFile(device, L"seafloor.dds",		nullptr,	m_texture1.ReleaseAndGetAddressOf());
	CreateDDSTextureFromFile(device, L"EvilDrone_Diff.dds", nullptr,	m_texture2.ReleaseAndGetAddressOf());

	//Initialise Render to texture
	m_FirstRenderPass = new RenderTexture(device, 800, 600, 1, 2);	//for our rendering, We dont use the last two properties. but.  they cant be zero and they cant be the same. 
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    auto size = m_deviceResources->GetOutputSize();
    float aspectRatio = float(size.right) / float(size.bottom);
    float fovAngleY = 70.0f * XM_PI / 180.0f;

    // This is a simple example of change that can be made when the app is in
    // portrait or snapped view.
    if (aspectRatio < 1.0f)
    {
        fovAngleY *= 2.0f;
    }

    // This sample makes use of a right-handed coordinate system using row-major matrices.
    m_projection = Matrix::CreatePerspectiveFieldOfView(
        fovAngleY,
        aspectRatio,
        0.01f,
        100.0f
    );
}

void Game::SetupGUI()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Procedural Terrain Generation");

    const auto& cameraPosition = m_Camera01.getPosition();
    ImGui::Text("Camera X Position: %.2f", cameraPosition.x);
    ImGui::Text("Camera Y Position: %.2f", cameraPosition.y);
    ImGui::Text("Camera Z Position: %.2f", cameraPosition.z);

    ImGui::Separator();

	const auto& dronePosition = m_Drone.GetPosition();
    ImGui::Text("Drone X Position: %.2f", dronePosition.x);
    ImGui::Text("Drone Y Position: %.2f", dronePosition.y);
    ImGui::Text("Drone Z Position: %.2f", dronePosition.z);

    ImGui::Separator();

    const auto& droneWorldPosition = m_Drone.GetWorldPosition();
    ImGui::Text("Drone World X Position: %.2f", droneWorldPosition.x);
    ImGui::Text("Drone World Y Position: %.2f", droneWorldPosition.y);
    ImGui::Text("Drone World Z Position: %.2f", droneWorldPosition.z);

    ImGui::Separator();

    ImGui::Text("Drone Local X Position: %.2f", m_localDroneX);
    ImGui::Text("Drone Local Z Position: %.2f", m_localDroneZ);

    ImGui::Separator();

	const auto& regions = m_Terrain.GetVoronoiRegions();
	ImGui::Text("Voronoi Regions: %d", regions.size());

	// Display the Voronoi regions
	for (const auto& region : regions)
	{
        const auto& regionPosition = region.position;

		ImGui::Text("Region Colour: %d", static_cast<int>(region.colour));

        ImGui::Text("Region X: %.2f", regionPosition.x);
        ImGui::Text("Region Y: %.2f", regionPosition.y);
        ImGui::Text("Region Z: %.2f", regionPosition.z);

        ImGui::Text("Region Min X: %.2f", region.minX);
        ImGui::Text("Region Max X: %.2f", region.maxX);
        ImGui::Text("Region Min Z: %.2f", region.minZ);
        ImGui::Text("Region Max Z: %.2f", region.maxZ);

		ImGui::Separator();
	}

	ImGui::Text("Sin Wave Parameters");
	ImGui::SliderFloat("Wave Amplitude",	m_Terrain.GetAmplitude(), 0.0f, 10.0f);
	ImGui::SliderFloat("Wavelength",		m_Terrain.GetWavelength(), 0.0f, 1.0f);

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    if (ImGui::Button("Generate Terrain"))
    {
        m_Terrain.GenerateHeightMap(m_deviceResources->GetD3DDevice());
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    if (ImGui::Button("Generate Random Terrain"))
    {
        unsigned int randomSeed = static_cast<unsigned int>(std::time(nullptr));

        m_Terrain.SetRandomSeed(randomSeed);
        m_Terrain.GenerateRandomHeightMap(m_deviceResources->GetD3DDevice());
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    static float perlinNoiseScale = 10.0f;
    static int perlinNoiseOctaves = 5;

    ImGui::SliderFloat("Perlin Noise Scale", &perlinNoiseScale, 0.0f, 100.0f);
    ImGui::SliderInt("Perlin Noise Octaves", &perlinNoiseOctaves, 0, 50);

    if (ImGui::Button("Generate Perlin Noise Terrain"))
    {
        m_Terrain.GeneratePerlinNoiseTerrain(m_deviceResources->GetD3DDevice(), perlinNoiseScale, perlinNoiseOctaves);
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    static int numVoronoiRegions = 5;
    ImGui::SliderInt("Number of Voronoi Regions", &numVoronoiRegions, 1, 20);

    if (ImGui::Button("Generate Voronoi Regions"))
    {
        m_Terrain.GenerateVoronoiRegions(m_deviceResources->GetD3DDevice(), numVoronoiRegions);
    }

	ImGui::End();
}

void Game::HandleTimerExpiration()
{
    m_Terrain.GeneratePerlinNoiseTerrain(m_deviceResources->GetD3DDevice(), 10.0f, 5);
	m_Terrain.GenerateVoronoiRegions(m_deviceResources->GetD3DDevice(), 5);

    SelectTargetRegion();

    m_Drone.ChangeColour(m_deviceResources->GetD3DDevice(), m_targetRegionColour, m_targetRegionColourVector);

    m_gameTimer.Restart();

    level++;
}

void Game::SetupDrone()
{
    // Set the fixed position where the drone should appear
    Vector3 dronePosition = m_Camera01.getPosition() + Vector3(0, -5.0f, -10.0f); // Adjust height and distance
    m_Drone.SetScale(Vector3(0.1f, 0.1f, 0.1f)); // Set the scale of the drone model
    m_Drone.SetRotation(Vector3(0.0f, 0.0f, 0.0f)); // Set the desired rotation for static effect
    m_Drone.SetPosition(dronePosition); // Set the drone position
}

void Game::UpdateDroneMovement()
{
    // Keep drone at a fixed offset from camera
    Vector3 cameraPosition = m_Camera01.getPosition();

    // Define fixed offset (2 units in front of the camera and slightly above)
    Vector3 droneOffset = m_Camera01.GetForwardVector() * 1.0f + m_Camera01.GetUpVector() * -0.5f;

    // Update drone position to camera position plus the offset
    Vector3 dronePosition = cameraPosition + droneOffset;


    // --- Collision Detection ---
    // 
    // 
	// ****** To restrict drone movement to terrain bounds ******
    // 
    //float terrainWorldWidth = (m_Terrain.GetWidth() * 0.1f);  // Scaled by 0.1f
    //float terrainWorldHeight = (m_Terrain.GetHeight() * 0.1f);  // Scaled by 0.1f
    //float terrainWorldMinX = 0.0f;                           // Assuming terrain starts at X=0
    //float terrainWorldMinZ = 0.0f;                           // Assuming terrain starts at Z=0
    //float terrainWorldMaxX = terrainWorldWidth;              // Max X in world space
    //float terrainWorldMaxZ = terrainWorldHeight;             // Max Z in world space
    // 
    //// 1. Clamp X/Z to terrain bounds
    //dronePosition.x = Utils::clamp(dronePosition.x, terrainWorldMinX, terrainWorldMaxX);
    //dronePosition.z = Utils::clamp(dronePosition.z, terrainWorldMinZ, terrainWorldMaxZ);

    //// 2. Terrain height check (Y-axis)
    // Convert drone's world position to terrain's local coordinates (considering scaling)
    //float localX = dronePosition.x / m_droneScale;  // Convert to terrain-local space
    //float localZ = dronePosition.z / m_droneScale;

    ////TODO: Try colour check with these local values

    //float terrainLocalY = m_Terrain.GetHeightAt(localX, localZ);
    //float terrainWorldY = (terrainLocalY * 0.1f) - 0.6f;  // Apply scaling/translation

    //// 3. Push drone up if below terrain
    //float droneRadius = m_BasicModel2.GetBoundingRadius();
    //float droneBottom = dronePosition.y - droneRadius;

    //if (droneBottom < terrainWorldY)
    //{
    //    dronePosition.y = terrainWorldY + droneRadius;
    //}

    //m_BasicModel2.SetPosition(dronePosition);

	// ****** To restrict drone movement to terrain bounds ******


    // ****** To collide drone with only terrain ******

    // TODO: Implement "from down of terrain to up" logic
    // 
    // Convert to terrain-local coordinates (reverse scaling and translation)
    m_localDroneX = (dronePosition.x - m_terrainTranslation.x) / m_terrainScale;
    m_localDroneZ = (dronePosition.z - m_terrainTranslation.z) / m_terrainScale;

    // Check if drone is over the terrain
    const bool isOverTerrain = (m_localDroneX >= 0 && m_localDroneX < m_Terrain.GetWidth() &&
                                m_localDroneZ >= 0 && m_localDroneZ < m_Terrain.GetHeight());

    m_Drone.SetColliding(false);

    if (isOverTerrain)
    {
        // Get terrain height in world space
        const float terrainLocalY = m_Terrain.GetHeightAt(m_localDroneX, m_localDroneZ);
        const float terrainWorldY = (terrainLocalY * m_terrainScale) + m_terrainTranslation.y;

        // Drone collision parameters
        const float droneRadius = m_Drone.GetBoundingRadius();
        const float droneBottom = dronePosition.y - droneRadius;

        // Check penetration from above or below
        const bool isPenetratingFromAbove = (droneBottom < terrainWorldY);

        // Resolve collision based on movement direction
        if (isPenetratingFromAbove)
        {
            // From above: Push up to terrain surface
            dronePosition.y = terrainWorldY + droneRadius;
            m_Drone.SetColliding(true);
        }
    }

    //if (m_BasicModel2.IsColliding())
    //{
    //    // Smoothly move drone upward if colliding
    //    // Lerp towards target Y position
    //    const float SMOOTH_SPEED = 5.0f;

    //    // Calculate interpolation factor (clamped between 0 and 1)
    //    float t = SMOOTH_SPEED * static_cast<float>(m_timer.GetElapsedSeconds());
    //    t = Utils::Clamp(t, 0.0f, 1.0f); // Ensure no overshooting

    //    const float targetYPosition = dronePosition.y + 10.0f;
    //    dronePosition.y = Utils::Lerp(dronePosition.y, targetYPosition, t);
    //}

    m_Drone.SetPosition(dronePosition);

    // Update previous Y for next frame
    m_previousDroneY = dronePosition.y;

    if (m_Drone.IsColliding())
    {
		CheckDroneRegionProgress(m_localDroneX, m_localDroneZ);
    }

    // ****** To collide drone with only terrain ******
}

void Game::UpdateCameraMovement()
{
    // Movement speed
    float moveSpeed = 0.1f;

    // Calculate camera movement based on WASD input
    Vector3 cameraMovement = Vector3::Zero;
	const bool isDroneColliding = m_Drone.IsColliding();

    // Forward and backward movement
    if (m_gameInputCommands.forward)
    {
        cameraMovement += m_Camera01.GetForwardVector() * moveSpeed;
    }
    if (m_gameInputCommands.back)
    {
        cameraMovement -= m_Camera01.GetForwardVector() * moveSpeed;
    }

    // Strafe left and right movement
    if (m_gameInputCommands.left)
    {
        cameraMovement -= m_Camera01.GetRightVector() * moveSpeed;
    }
    if (m_gameInputCommands.right)
    {
        cameraMovement += m_Camera01.GetRightVector() * moveSpeed;
    }

    // Vertical movement (optional)
    if (m_gameInputCommands.up)
    {
        cameraMovement += Vector3::UnitY * moveSpeed; // Straight up
    }
    if (m_gameInputCommands.down && !isDroneColliding)
    {
        cameraMovement -= Vector3::UnitY * moveSpeed; // Straight down
    }

    // Update camera position
    Vector3 cameraPosition = m_Camera01.getPosition();
    cameraPosition += cameraMovement;
    m_Camera01.setPosition(cameraPosition);
}

void Game::SelectTargetRegion()
{
    m_targetRegionColour = m_Terrain.GetRandomVoronoiRegionColour();
	m_targetRegionColourVector = m_Terrain.GetVoronoiRegionColourVector(m_targetRegionColour);
}

bool Game::IsTargetRegion(const Enums::COLOUR& colour) const
{
    return colour == m_targetRegionColour;
}

void Game::CheckDroneRegionProgress(const float localX, const float localZ)
{
    const auto currentRegionColour = m_Terrain.GetRegionColourAtPosition(localX, localZ);

    if (IsTargetRegion(currentRegionColour))
    {
        HandleTargetRegionReached();
    }
}

void Game::HandleTargetRegionReached()
{
    level++;
}

void Game::DrawLevelIndicator()
{
    char buffer[16];
    sprintf_s(buffer, "Level: %d", level);

    m_sprites->Begin();
    m_font->DrawString(m_sprites.get(), std::string(buffer).c_str(), XMFLOAT2(700, 10), Colors::Yellow);
    m_sprites->End();
}

void Game::InitializeRegionRules()
{
    RegionRule regionRule1 = { m_Terrain.GetRandomVoronoiRegionColour(), ObstacleType::SPIKES, "F", { {'F', "F[+F]F[-F]F"} }, 3 };
    m_regionRules.push_back(regionRule1);

    RegionRule regionRule2 = { m_Terrain.GetRandomVoronoiRegionColour(), ObstacleType::CRYSTALS, "F", { {'F', "FF+[+F-F-F]-[-F+F+F]"} }, 4 };
    m_regionRules.push_back(regionRule2);

    RegionRule regionRule3 = { m_Terrain.GetRandomVoronoiRegionColour(), ObstacleType::VINES, "F", { {'F', "F[+FF][-FF]F"} }, 3 };
    m_regionRules.push_back(regionRule3);
}

void Game::GenerateFractalObstacles()
{
    for (const auto& region : m_Terrain.GetVoronoiRegions())
    {
        // Find the rule matching the region's colour
        for (const auto& rule : m_regionRules)
        {
            if (region.colour != rule.regionColour)
            {
                continue;
            }

            // Randomize parameters for variety
            const float angle = 25.0f + (rand() % 20); // 25°–45°
            const float segmentLength = 1.5f + (rand() % 3); // 1.5–4.5 units

            LSystem lsystem(rule.axiom, rule.rules, rule.iterations);
            lsystem.Generate();

            FractalObstacle obstacle
            (
                m_deviceResources->GetD3DDevice(),
                region.position, // Spawn at region's seed point
                angle,
                segmentLength
            );

            obstacle.Generate(lsystem);
            m_fractalObstacles.push_back(obstacle);

            break;
        }
    }
}

void Game::RenderFractalObstacles(ID3D11DeviceContext* context)
{
    for (const auto& obstacle : m_fractalObstacles)
    {
        for (const auto& segment : obstacle.GetSegments())
        {
            Matrix scale = Matrix::CreateScale(0.2f, segment.length, 0.2f); // Scale Y-axis by segment length
            Matrix rotation = Matrix::CreateFromYawPitchRoll(
                XMConvertToRadians(segment.rotation.y),
                XMConvertToRadians(segment.rotation.x),
                XMConvertToRadians(segment.rotation.z)
            );
            Matrix translation = Matrix::CreateTranslation(segment.position);

            Matrix world = scale * rotation * translation;

            m_BasicShaderPair.EnableShader(context);
            m_BasicShaderPair.SetShaderParameters(context, &world, &m_view, &m_projection, &m_Light, m_texture2.Get());
            m_ObstacleModel.Render(context);
        }
    }
}

void Game::OnDeviceLost()
{
    m_states.reset();
    m_fxFactory.reset();
    m_sprites.reset();
    m_font.reset();
	m_batch.reset();
	m_testmodel.reset();
    m_batchInputLayout.Reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}
#pragma endregion
