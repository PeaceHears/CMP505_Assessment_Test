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
    m_deviceResources->RegisterDeviceNotify(shared_from_this());

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
    //m_cameraPosition = Vector3(-4.0f, 18.0f, 0.8f);
    m_cameraPosition = Vector3(0.0f, 5.0f, 4.0f);
    
    m_cameraRotation = Vector3(-90.0f, -180.0f, 0.0f);
	m_Camera01.setPosition(m_cameraPosition);
	m_Camera01.setRotation(m_cameraRotation);	//orientation is -90 becuase zero will be looking up at the sky straight up. 

	m_Terrain.GeneratePerlinNoiseTerrain(m_deviceResources->GetD3DDevice(), 10.0f, 5);
    m_Terrain.GenerateVoronoiRegions(m_deviceResources->GetD3DDevice(), 5);

    ChangeTargetRegion();

	m_gameTimer.SetStartTime(m_timer, 300.0f);
    m_gameTimer.Start();

    SetupDrone();

    CreateObjectsVector(10);

    InitializeRegionRules();
    GenerateFractalObstacles();
	
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
    float mouseDeltaX = static_cast<float>(m_gameInputCommands.mouseX - m_lastMouseX);
    float mouseDeltaY = static_cast<float>(m_gameInputCommands.mouseY - m_lastMouseY);

    // Update last mouse positions
    m_lastMouseX = m_gameInputCommands.mouseX;
    m_lastMouseY = m_gameInputCommands.mouseY;

    // Camera Rotation
    Vector3 rotation = m_Camera01.getRotation();

    // Sensitivity adjustment for rotation
    float sensitivity = 0.2f;

    // Horizontal rotation (Yaw) - left/right mouse movement
    rotation.y -= mouseDeltaX * sensitivity;

    // Vertical rotation (Pitch) - up/down mouse movement
    rotation.x -= mouseDeltaY * sensitivity;

    // Clamp vertical rotation to prevent camera flipping
    rotation.x = Utils::Clamp(rotation.x, -89.0f, 89.0f);

    if (!isMouseHoveringOverImGui)
    {
        // Set the new rotation for the camera
        m_Camera01.setRotation(rotation);
    }

    UpdateCameraMovement();
    UpdateDroneMovement();

    CheckObjectColoursWithRegionColours();

	m_Camera01.Update();	//camera update.
	m_Terrain.Update();		//terrain update.  doesnt do anything at the moment. 

	m_view = m_Camera01.getCameraMatrix();
	m_world = Matrix::Identity;

	/*create our UI*/
    SetupImGUI();

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

    //RenderWithPostProcess();
    RenderWithoutPostProcess();
}

void Game::RenderWithPostProcess()
{
    auto context = m_deviceResources->GetD3DDeviceContext();

    // Clear the post-process render texture
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    context->ClearRenderTargetView(m_PostProcessRenderTexture->getRenderTargetView(), clearColor);
    context->ClearDepthStencilView(m_deviceResources->GetDepthStencilView(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
        1.0f,
        0);

    auto renderTargetView = m_PostProcessRenderTexture->getRenderTargetView();

    // Set render target to post-process texture
    context->OMSetRenderTargets(1, &renderTargetView, m_deviceResources->GetDepthStencilView());

    // Set the viewport for the render texture
    D3D11_VIEWPORT viewport;
    viewport.Width = static_cast<float>(m_PostProcessRenderTexture->getTextureWidth());
    viewport.Height = static_cast<float>(m_PostProcessRenderTexture->getTextureHeight());
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    context->RSSetViewports(1, &viewport);

    // Render scene components (your existing rendering logic)
    // Set rendering states
    context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_states->DepthDefault(), 0);
    context->RSSetState(m_states->CullClockwise());

    RenderScene(context);

    // Restore the default render target
    auto backBufferRTV = m_deviceResources->GetRenderTargetView();
    context->OMSetRenderTargets(1, &backBufferRTV, m_deviceResources->GetDepthStencilView());

    // Clear the back buffer
    float clearColorBackBuffer[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    context->ClearRenderTargetView(backBufferRTV, clearColorBackBuffer);

    // Apply post-processing effect
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Enable the post-process shader
    m_PostProcessShader.EnableShader(context);

    // Set shader parameters
    DirectX::SimpleMath::Matrix identity = DirectX::SimpleMath::Matrix::Identity;
    m_PostProcessShader.SetShaderParameters(
        context,
        &identity,
        &identity,
        &identity,
        &m_Light,
        m_PostProcessRenderTexture->getShaderResourceView(),
        m_postProcessEffectType,
        m_postProcessVignetteIntensity
    );

    // Draw full-screen triangle
    context->Draw(3, 0);

    DrawGUIIndicators();

    // Render ImGui
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // Present the frame
    m_deviceResources->Present();
}

void Game::RenderWithoutPostProcess()
{
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

    RenderScene(m_deviceResources->GetD3DDeviceContext());

    DrawGUIIndicators();

    // Render ImGui
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // Present the frame
    m_deviceResources->Present();
}

void Game::RenderScene(ID3D11DeviceContext* context)
{
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

    m_BasicShaderPair.EnableShader(context);
    m_BasicShaderPair.SetShaderParameters(context, &droneWorldMatrix, &m_view, &m_projection, &m_Drone_Light, m_texture2.Get());
    m_Drone.Render(context);

    RenderObjectsAtRandomLocations(context);

    RenderFractalObstacles(context);

}

void Game::DrawGUIIndicators()
{
    // Draw Title to the screen
    m_sprites->Begin();
    m_font->DrawString(m_sprites.get(), L"Advanced Procedural Methods", XMFLOAT2(10, 10), Colors::Yellow);
    m_sprites->End();

    // Draw Game Timer to the screen 
    m_gameTimer.Render(m_sprites, m_font);

    DrawLevelIndicator();
    DrawMatchedColouredObjectCountIndicator();
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    m_deviceResources->PIXBeginEvent(L"Clear");

    // Clear the views.
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, Colors::Black);
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

    // Recreate post-process render texture with new dimensions
    m_PostProcessRenderTexture = std::make_unique<RenderTexture>(
        m_deviceResources->GetD3DDevice(),
        width,
        height,
        0.1f,
        100.0f
    );
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
    m_Terrain.SetScale(m_terrainScale);
    m_Terrain.SetTranslation(m_terrainTranslation);

	//setup our test model
    m_Drone.InitializeModel(device,"drone.obj", true);
    m_ObstacleModel.InitializeBox(device, 0.2f, 1.0f, 0.2f);

	//load and set up our Vertex and Pixel Shaders
	m_BasicShaderPair.InitStandard(device, L"light_vs.cso", L"light_ps.cso");

    CreatePostProcessResources();

	//load Textures
	CreateDDSTextureFromFile(device, L"seafloor.dds",		nullptr,	m_texture1.ReleaseAndGetAddressOf());
	CreateDDSTextureFromFile(device, L"EvilDrone_Diff.dds", nullptr,	m_texture2.ReleaseAndGetAddressOf());
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

void Game::SetupImGUI()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Procedural Terrain Generation");

    if (ImGui::IsWindowHovered())
    {
        isMouseHoveringOverImGui = true;
    }
    else
    {
        isMouseHoveringOverImGui = false;
    }

    const auto& cameraPosition = m_Camera01.getPosition();
    ImGui::Text("Camera X Position: %.2f", cameraPosition.x);
    ImGui::Text("Camera Y Position: %.2f", cameraPosition.y);
    ImGui::Text("Camera Z Position: %.2f", cameraPosition.z);

    if (ImGui::SliderFloat("Camera X Position: %.2f", &m_cameraPosition.x, -360.0f, 360.0f))
    {
        m_Camera01.setPosition(m_cameraPosition);
    }
    if (ImGui::SliderFloat("Camera Y Position: %.2f", &m_cameraPosition.y, -360.0f, 360.0f))
    {
        m_Camera01.setPosition(m_cameraPosition);
    }
    if (ImGui::SliderFloat("Camera Z Position: %.2f", &m_cameraPosition.z, -360.0f, 360.0f))
    {
        m_Camera01.setPosition(m_cameraPosition);
    }

    ImGui::Separator();

    if (ImGui::SliderFloat("Camera X Rotation: %.2f", &m_cameraRotation.x, -360.0f, 360.0f))
    {
        m_Camera01.setRotation(m_cameraRotation);
    }
    if (ImGui::SliderFloat("Camera Y Rotation: %.2f", &m_cameraRotation.y, -360.0f, 360.0f))
    {
        m_Camera01.setRotation(m_cameraRotation);
    }
    if (ImGui::SliderFloat("Camera Z Rotation: %.2f", &m_cameraRotation.z, -360.0f, 360.0f))
    {
        m_Camera01.setRotation(m_cameraRotation);
    }

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

    static int manualSeed = 0;
    ImGui::InputInt("Random Seed", &manualSeed);
    if (ImGui::Button("Set Custom Seed"))
    {
        m_Terrain.SetRandomSeed(static_cast<unsigned int>(manualSeed));
    }

    // Smoothing controls
    static float smoothingIntensity = 0.5f;
    ImGui::SliderFloat("Smoothing Intensity", &smoothingIntensity, 0.0f, 1.0f);
    ImGui::Text("Press 'S' to smooth terrain");

    if (ImGui::Button("Smooth Terrain"))
    {
        m_Terrain.SmoothTerrain(m_deviceResources->GetD3DDevice(), smoothingIntensity);
    }

    if (ImGui::Button("Fault Terrain"))
    {
        m_Terrain.GenerateFaultTerrain(m_deviceResources->GetD3DDevice());
    }

    if (ImGui::Button("Particle Deposition Terrain"))
    {
        m_Terrain.GenerateParticleDepositionTerrain(m_deviceResources->GetD3DDevice());
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

    SetupPostProcessImGUI();
}

void Game::SetupPostProcessImGUI()
{
    ImGui::Begin("Post-Processing Effects");

    m_postProcessEffectType = 1;
    const char* effects[] = {
        "Normal",
        "Invert",
        "Grayscale",
        "Sepia",
        "Vignette"
    };
    ImGui::Combo("Effect Type", &m_postProcessEffectType, effects, IM_ARRAYSIZE(effects));

    // Add additional effect-specific parameters here
    switch (m_postProcessEffectType)
    {
        case 1: // Invert
            break;
        case 2: // Grayscale
            break;
        case 3: // Sepia
            break;
        case 4: // Vignette
            ImGui::SliderFloat("Vignette Intensity", &m_postProcessVignetteIntensity, 0.0f, 1.0f);
            break;
    }

    ImGui::End();
}

void Game::HandleTimerExpiration()
{
    if (IsWin())
    {
        OnWin();
    }
    else
    {
        RestartScene();
    }
}

void Game::SetupDrone()
{
    // Set the fixed position where the drone should appear
    Vector3 dronePosition = m_Camera01.getPosition() + Vector3(0, -5.0f, -10.0f); // Adjust height and distance
    m_Drone.SetScale(Vector3(0.1f, 0.1f, 0.1f)); // Set the scale of the drone model
    m_Drone.SetRotation(Vector3(0.0f, 0.0f, 0.0f)); // Set the desired rotation for static effect
    m_Drone.SetPosition(dronePosition); // Set the drone position
    m_Drone.UpdateBoundingSphere();

    m_Drone.ChangeColour(m_deviceResources->GetD3DDevice(), m_targetRegionColour, m_targetRegionColourVector);
}

void Game::UpdateDroneMovement()
{
    // Keep drone at a fixed offset from camera
    Vector3 cameraPosition = m_Camera01.getPosition();

    // Define a fixed offset relative to camera
    Vector3 droneOffset = Vector3(0, -0.5f, -1.0f);

    // Create drone position based on camera
    Vector3 dronePosition = cameraPosition + droneOffset;

    // Perform terrain collision check
    CheckObjectCollisionWithTerrain(m_localDroneX, m_localDroneZ, dronePosition, m_Drone, true);

    // Update drone position and bounding sphere
    m_Drone.SetPosition(dronePosition);
    m_Drone.UpdateBoundingSphere();

    // Check for collisions
    CheckDroneCollisions();
}

void Game::UpdateCameraMovement()
{
    // Movement speed
    float moveSpeed = 0.1f;

    // Check if drone is colliding with terrain
    const bool isDroneColliding = m_Drone.IsCollidingWithTerrain();

    // Calculate camera movement based on WASD input
    Vector3 cameraMovement = Vector3::Zero;

    // Get camera's forward and right vectors
    Vector3 forwardVector = m_Camera01.GetForwardVector();
    Vector3 rightVector = m_Camera01.GetRightVector();

    // Flatten forward and right vectors to prevent vertical movement from input
    forwardVector.y = 0.0f;
    rightVector.y = 0.0f;
    forwardVector.Normalize();
    rightVector.Normalize();

    // Forward and backward movement
    if (m_gameInputCommands.forward)
    {
        cameraMovement += forwardVector * moveSpeed;
    }
    if (m_gameInputCommands.back)
    {
        cameraMovement -= forwardVector * moveSpeed;
    }

    // Strafe left and right movement
    if (m_gameInputCommands.left)
    {
        cameraMovement -= rightVector * moveSpeed;
    }
    if (m_gameInputCommands.right)
    {
        cameraMovement += rightVector * moveSpeed;
    }

    // Vertical movement with terrain collision check
    if (m_gameInputCommands.up)
    {
        cameraMovement.y += moveSpeed;
    }
    if (m_gameInputCommands.down)
    {
        // Only allow downward movement if drone is NOT colliding with terrain
        if (!isDroneColliding)
        {
            cameraMovement.y -= moveSpeed;
        }
    }

    // Update camera position
    Vector3 cameraPosition = m_Camera01.getPosition();
    cameraPosition += cameraMovement;
    m_Camera01.setPosition(cameraPosition);

    // Update drone position relative to camera
    Vector3 dronePosition = cameraPosition +
        Vector3(0, -0.5f, -1.0f);  // Fixed offset

    m_Drone.SetPosition(dronePosition);
}

void Game::ChangeTargetRegion()
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
    HandleTargetRegionReached(currentRegionColour);
}

void Game::HandleTargetRegionReached(const Enums::COLOUR& regionColour)
{
    m_Drone.ChangeColour(m_deviceResources->GetD3DDevice(),
        regionColour,
        m_Terrain.GetVoronoiRegionColourVector(regionColour));
}

void Game::DrawLevelIndicator()
{
    char buffer[16];
    sprintf_s(buffer, "Level: %d", level);

    m_sprites->Begin();
    m_font->DrawString(m_sprites.get(), std::string(buffer).c_str(), XMFLOAT2(800, 10), Colors::Yellow);
    m_sprites->End();
}

void Game::DrawMatchedColouredObjectCountIndicator()
{
    char buffer[50];
    sprintf_s(buffer, "Matched Coloured Objects: %d/%d", matchedColourCount, (int) m_objects.size());

    m_sprites->Begin();
    m_font->DrawString(m_sprites.get(), std::string(buffer).c_str(), XMFLOAT2(800, 50), Colors::Orange);
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

void Game::CreateObjectsVector(int count)
{
    for (size_t i = 0; i < count; i++)
    {
        const float randomScale = Utils::GetRandomFloat(0.1f, 0.5f);
        const auto randomPosition = m_Terrain.GetRandomPosition();

        const auto randomVoronoiRegionColour = m_Terrain.GetRandomVoronoiRegionColour();

        auto model = std::make_unique<ModelClass>();

        model->InitializeModel(m_deviceResources->GetD3DDevice(), "drone.obj", true);
        model->ChangeColour(m_deviceResources->GetD3DDevice(),
            randomVoronoiRegionColour,
            m_Terrain.GetVoronoiRegionColourVector(randomVoronoiRegionColour));

        model->SetPosition(randomPosition);
        model->SetScale(Vector3(randomScale, randomScale, randomScale));
        model->UpdateBoundingSphere();

        m_objects.push_back(std::move(model));
    }
}

void Game::RenderObjectsAtRandomLocations(ID3D11DeviceContext* context)
{
    for each (const auto& object in m_objects)
    {
        float localPositionX = 0.0f;
        float localPositionY = 0.0f;
        auto worldPosition = object->GetPosition();

        CheckObjectCollisionWithTerrain(localPositionX, localPositionY, worldPosition, *object);

        m_BasicShaderPair.EnableShader(context);
        m_BasicShaderPair.SetShaderParameters(context, &object->GetWorldMatrix(), &m_view, &m_projection, &m_Light, m_texture2.Get());
        object->Render(context);
    }
}

void Game::CheckObjectCollisionWithTerrain(float& localPositionX, float& localPositionZ,
                                            DirectX::SimpleMath::Vector3& worldPosition, ModelClass& model,
                                            const bool isPlayer)
{
    localPositionX = (worldPosition.x - m_terrainTranslation.x) / m_terrainScale;
    const auto localPositionY = (worldPosition.y - m_terrainTranslation.y) / m_terrainScale;
    localPositionZ = (worldPosition.z - m_terrainTranslation.z) / m_terrainScale;

    model.SetLocalPosition(DirectX::SimpleMath::Vector3(localPositionX, localPositionY, localPositionZ));

    // Check if model is over the terrain
    const bool isOverTerrain = (localPositionX >= 0 && localPositionX < m_Terrain.GetWidth() &&
                                localPositionZ >= 0 && localPositionZ < m_Terrain.GetHeight());

    model.SetCollidingWithTerrain(false);

    if (isOverTerrain)
    {
        // Get terrain height in world space
        const float terrainLocalY = m_Terrain.GetHeightAt(localPositionX, localPositionZ);
        const float terrainWorldY = (terrainLocalY * m_terrainScale) + m_terrainTranslation.y;

        // Model collision parameters
        const float modelRadius = model.GetBoundingRadius();
        const float modelBottom = worldPosition.y - modelRadius;

        // Check penetration from above or below
        const bool isPenetratingFromAbove = (modelBottom <= terrainWorldY);

        // Resolve collision based on movement direction
        if (isPenetratingFromAbove)
        {
            // From above: Push up to terrain surface
            worldPosition.y = terrainWorldY + modelRadius;
            model.SetCollidingWithTerrain(true);

            if (isPlayer)
            {
                CheckDroneRegionProgress(m_localDroneX, m_localDroneZ);
            }
        }
    }

    model.SetPosition(worldPosition);
}

void Game::CheckDroneCollisions()
{
    const auto droneColour = m_Drone.GetColour();

    for each (auto& object in m_objects)
    {
        if (m_Drone.CheckCollision(*object))
        {
            if (object->IsCollidingWithModel())
            {
                continue;
            }

            object->SetCollidingWithModel(true);
            object->ChangeColour(m_deviceResources->GetD3DDevice(), droneColour, m_Terrain.GetVoronoiRegionColourVector(droneColour));
        }
        else
        {
            object->SetCollidingWithModel(false);
        }
    }
}

void Game::CheckObjectColoursWithRegionColours()
{
    matchedColourCount = 0;

    for each (auto& object in m_objects)
    {
        const auto objectPosition = object->GetLocalPosition();
        const auto objectColour = object->GetColour();
        const auto regionColour = m_Terrain.GetRegionColourAtPosition(objectPosition.x, objectPosition.z);

        if (objectColour == regionColour)
        {
            matchedColourCount++;
        }
    }

    CheckWin();
}

void Game::CheckWin()
{
    if (IsWin())
    {
        OnWin();
    }
}

bool Game::IsWin()
{
    if (matchedColourCount == m_objects.size())
    {
        return true;
    }

    return false;
}

void Game::OnWin()
{
    RestartScene();
    level++;
}

void Game::CreatePostProcessResources()
{
    // Create render texture for post-processing
    m_PostProcessRenderTexture = std::make_unique<RenderTexture>(
        m_deviceResources->GetD3DDevice(),
        800,  // Width 
        600,  // Height
        1,
        2
    );

    // Initialize post-process shader
    m_PostProcessShader.InitStandard(
        m_deviceResources->GetD3DDevice(),
        L"postprocess_vs.cso",
        L"postprocess_ps.cso",
        true
    );
}

void Game::RestartScene()
{
    m_Terrain.GeneratePerlinNoiseTerrain(m_deviceResources->GetD3DDevice(), 10.0f, 5);
    m_Terrain.GenerateVoronoiRegions(m_deviceResources->GetD3DDevice(), 5);

    ChangeTargetRegion();

    m_Drone.ChangeColour(m_deviceResources->GetD3DDevice(), m_targetRegionColour, m_targetRegionColourVector);

    m_gameTimer.Restart();
}

void Game::OnDeviceLost()
{
    m_states.reset();
    m_fxFactory.reset();
    m_sprites.reset();
    m_font.reset();
	m_batch.reset();
    m_batchInputLayout.Reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}
#pragma endregion
