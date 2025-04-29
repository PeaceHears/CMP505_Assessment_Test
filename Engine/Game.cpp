//
// Game.cpp
//

#include "pch.h"
#include "Game.h"


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
    m_BasicModel2.ChangeColour(m_deviceResources->GetD3DDevice(), m_targetRegionColourVector);

	m_gameTimer.SetStartTime(m_timer, 10.0f);
    m_gameTimer.Start();

    SetupDrone();
	
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

    // Vertical rotation (Pitch) - up/down mouse movement
    rotation.x -= mouseDeltaY * sensitivity;

    // Clamp vertical rotation to prevent flipping
    rotation.x = std::max(-89.0f, std::min(rotation.x, 89.0f));

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

    //CheckDroneRegionProgress();

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

    // Draw Title to the screen
    m_sprites->Begin();
	m_font->DrawString(m_sprites.get(), L"Advanced Procedural Methods", XMFLOAT2(10, 10), Colors::Yellow);
    m_sprites->End();

    // Draw Game Timer to the screen 
	m_gameTimer.Render(m_sprites, m_font);

	DrawLevelIndicator();

	//Set Rendering states. 
	context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
	context->OMSetDepthStencilState(m_states->DepthDefault(), 0);
	context->RSSetState(m_states->CullClockwise());
//	context->RSSetState(m_states->Wireframe());

	//prepare transform for floor object. 
	m_world = SimpleMath::Matrix::Identity; //set world back to identity
	SimpleMath::Matrix newPosition3 = SimpleMath::Matrix::CreateTranslation(0.0f, -0.6f, 0.0f);
	SimpleMath::Matrix newScale = SimpleMath::Matrix::CreateScale(0.1f);		//scale the terrain down a little. 
	m_world = m_world * newScale * newPosition3;

	m_BasicShaderPair.EnableShader(context);
	m_BasicShaderPair.SetShaderParameters(context, &m_world, &m_view, &m_projection, &m_Light, m_texture1.Get());

	m_Terrain.Render(context);

    SimpleMath::Matrix droneWorldMatrix = m_BasicModel2.GetWorldMatrix();
    SimpleMath::Matrix droneScale = SimpleMath::Matrix::CreateScale(0.1f);
    //droneWorldMatrix *= droneScale;

    m_BasicShaderPair.EnableShader(context);
    m_BasicShaderPair.SetShaderParameters(context, &droneWorldMatrix, &m_view, &m_projection, &m_Drone_Light, m_texture2.Get());

	m_BasicModel2.Render(context);

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
	m_BasicModel2.InitializeModel(device,"drone.obj", true);
	m_BasicModel3.InitializeBox(device, 10.0f, 0.1f, 10.0f);	//box includes dimensions

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

	const auto& dronePosition = m_BasicModel2.GetPosition();
    ImGui::Text("Drone X Position: %.2f", dronePosition.x);
    ImGui::Text("Drone Y Position: %.2f", dronePosition.y);
    ImGui::Text("Drone Z Position: %.2f", dronePosition.z);

    ImGui::Separator();

    const auto& droneWorldPosition = m_BasicModel2.GetWorldPosition();
    ImGui::Text("Drone World X Position: %.2f", droneWorldPosition.x);
    ImGui::Text("Drone World Y Position: %.2f", droneWorldPosition.y);
    ImGui::Text("Drone World Z Position: %.2f", droneWorldPosition.z);

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

	m_BasicModel2.ChangeColour(m_deviceResources->GetD3DDevice(), m_targetRegionColourVector);

    m_gameTimer.Restart();

    level++;
}

void Game::SetupDrone()
{
    // Set the fixed position where the drone should appear
    Vector3 dronePosition = m_Camera01.getPosition() + Vector3(0, 5.0f, -10.0f); // Adjust height and distance
	m_BasicModel2.SetScale(Vector3(0.1f, 0.1f, 0.1f)); // Set the scale of the drone model
    m_BasicModel2.SetRotation(Vector3(0.0f, 0.0f, 0.0f)); // Set the desired rotation for static effect
    m_BasicModel2.SetPosition(dronePosition); // Set the drone position
}

void Game::UpdateDroneMovement()
{
    // Keep drone at a fixed offset from camera
    Vector3 cameraPosition = m_Camera01.getPosition();

    // Define fixed offset (2 units in front of the camera and slightly above)
    Vector3 droneOffset = m_Camera01.GetForwardVector() * 1.0f + m_Camera01.GetUpVector() * 0.1f;

    // Update drone position to camera position plus the offset
    Vector3 dronePosition = cameraPosition + droneOffset;

    // Set the updated position to the drone
    m_BasicModel2.SetPosition(dronePosition);

    // --- Collision Detection ---
    dronePosition = m_BasicModel2.GetPosition(); // Get the updated position

    // Convert drone's world position to terrain's local coordinates (considering scaling)
    float localX = dronePosition.x / 0.1f;
    float localZ = dronePosition.z / 0.1f;

    // Get terrain height at this position (local Y)
    float terrainLocalY = m_Terrain.GetHeightAt(localX, localZ);

    // Convert terrain's local Y to world Y (apply scaling and translation)
    float terrainWorldY = (terrainLocalY * 0.1f) - 0.6f;

    // Get the drone's effective radius
    float droneRadius = m_BasicModel2.GetBoundingRadius();

    // Drone's bottom point (center Y minus radius)
    float droneBottom = dronePosition.y - droneRadius;

    // Collision check
    if (droneBottom < terrainWorldY)
    {
        // Push the drone up to sit on the terrain
        dronePosition.y = terrainWorldY + droneRadius;
        m_BasicModel2.SetPosition(dronePosition);
    }
}

void Game::UpdateCameraMovement()
{
    // Movement speed
    float moveSpeed = 0.1f;

    // Calculate camera movement based on WASD input
    Vector3 cameraMovement = Vector3::Zero;

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
    if (m_gameInputCommands.down)
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

void Game::CheckDroneRegionProgress()
{
    Vector3 dronePosition = m_BasicModel2.GetPosition();
    const auto currentRegionColour = m_Terrain.GetRegionColourAtPosition(dronePosition.x, dronePosition.z);

    if (IsTargetRegion(currentRegionColour))
    {
        HandleTargetRegionReached();
    }
}

void Game::HandleTargetRegionReached()
{
    
}

void Game::DrawLevelIndicator()
{
    char buffer[16];
    sprintf_s(buffer, "Level: %d", level);

    m_sprites->Begin();
    m_font->DrawString(m_sprites.get(), std::string(buffer).c_str(), XMFLOAT2(700, 10), Colors::Yellow);
    m_sprites->End();
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
