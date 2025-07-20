// Game.h

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "Shader.h"
#include "Light.h"
#include "Input.h"
#include "Camera.h"
#include "RenderTexture.h"
#include "Terrain.h"
#include "GameTimer.h"
#include "Enums.h"
#include "modelclass.h"

// Forward declarations to reduce header dependencies
class FractalObstacle;

// A basic game implementation that creates a D3D11 device and provides a game loop.
class Game final : public DX::IDeviceNotify, public std::enable_shared_from_this<Game>
{
public:
    Game() noexcept(false);
    // Make the class non-copyable and non-movable
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    Game(Game&&) = delete;
    Game& operator=(Game&&) = delete;
    ~Game();

    // Initialization and management
    void Initialize(HWND window, int width, int height);
    void GetDefaultSize(int& width, int& height) const;

    // Basic game loop
    void Tick();

    // IDeviceNotify
    void OnDeviceLost() override;
    void OnDeviceRestored() override;

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowMoved();
    void OnWindowSizeChanged(int width, int height);

#ifdef DXTK_AUDIO
    void NewAudioDevice();
#endif

private:
    // --- Core Engine Components ---
    void Update(DX::StepTimer const& timer);
    void Render();
    void Clear();
    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();
    void SetupImGUI();

    // --- Game Logic ---
    void HandleTimerExpiration();
    void SetupDrone();
    void UpdateCameraMovement();
    void UpdateDroneMovement();
    void ChangeTargetRegion();
    bool IsTargetRegion(const Enums::COLOUR& colour) const;
    void CheckDroneRegionProgress(const float localX, const float localZ);
    void HandleTargetRegionReached(const Enums::COLOUR& regionColour);
    void RestartScene();
    void CheckWin();
    bool IsWin();
    void OnWin();

    // --- Object and Collision Management ---
    void CreateObjectsVector(int count);
    void CheckObjectCollisionWithTerrain(float& localPositionX, float& localPositionZ,
        DirectX::SimpleMath::Vector3& worldPosition, ModelClass& model,
        const bool isPlayer = false);
    void CheckDroneCollisions();
    void CheckObjectColoursWithRegionColours();

    // --- Procedural Generation ---
    void InitializeRegionRules();
    void GenerateFractalObstacles();

    // --- Rendering Sub-systems ---
    void RenderScene(ID3D11DeviceContext* context);
    void RenderFractalObstacles(ID3D11DeviceContext* context);
    void RenderObjectsAtRandomLocations(ID3D11DeviceContext* context);
    void DrawGUIIndicators();
    void DrawLevelIndicator();
    void DrawMatchedColouredObjectCountIndicator();

    // --- Post-Processing ---
    void RenderWithPostProcess();
    void RenderWithoutPostProcess();
    void CreatePostProcessResources();
    void SetupPostProcessImGUI();

    // --- Private Member Variables ---

    // Core Resources
    std::unique_ptr<DX::DeviceResources> m_deviceResources;
    DX::StepTimer                        m_timer;
    Input                                m_input;
    InputCommands                        m_gameInputCommands;

    // DirectXTK Objects
    std::unique_ptr<DirectX::CommonStates>                                  m_states;
    std::unique_ptr<DirectX::EffectFactory>                                 m_fxFactory;
    std::unique_ptr<DirectX::SpriteBatch>                                   m_sprites;
    std::unique_ptr<DirectX::SpriteFont>                                    m_font;
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>>  m_batch;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>                               m_batchInputLayout;

    // Shaders and Textures
    Shader                                   m_BasicShaderPair;
    Shader                                   m_PostProcessShader;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_texture1;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_texture2;

    // Scene objects
    Terrain                                  m_Terrain;
    ModelClass                               m_Drone;
    ModelClass                               m_ObstacleModel;
    std::vector<std::unique_ptr<ModelClass>> m_objects;
    std::vector<FractalObstacle>             m_fractalObstacles;

    // Lights
    Light                                    m_Light;
    Light                                    m_Drone_Light;

    // Camera
    Camera                                   m_Camera01;
    DirectX::SimpleMath::Vector3             m_cameraPosition;
    DirectX::SimpleMath::Vector3             m_cameraRotation;

    // Rendering and Post-Processing
    std::unique_ptr<RenderTexture>           m_PostProcessRenderTexture;
    int                                      m_postProcessEffectType = 0;
    float                                    m_postProcessVignetteIntensity = 0.5f;

    // Game State
    GameTimer                                m_gameTimer;
    bool                                     m_isTimerPaused = false;
    int                                      level = 1;
    int                                      matchedColourCount = 0;

    // UI and Input State
    int                                      m_lastMouseX = 0;
    int                                      m_lastMouseY = 0;
    bool                                     isMouseHoveringOverImGui = false;

    // Procedural Generation State
    enum class ObstacleType { SPIKES, CRYSTALS, VINES };
    struct RegionRule
    {
        Enums::COLOUR regionColour;
        ObstacleType obstacleType;
        std::string axiom;
        std::vector<std::pair<char, std::string>> rules;
        int iterations;
    };
    std::vector<RegionRule>                  m_regionRules;
    Enums::COLOUR                            m_targetRegionColour;
    DirectX::SimpleMath::Vector4             m_targetRegionColourVector;

    // Terrain and Object Positioning
    float                                    m_terrainScale = 0.1f;
    DirectX::SimpleMath::Vector3             m_terrainTranslation = DirectX::SimpleMath::Vector3(0.0f, -0.6f, 0.0f);
    float                                    m_localDroneX = 0.0f;
    float                                    m_localDroneZ = 0.0f;

    // Matrices
    DirectX::SimpleMath::Matrix              m_world;
    DirectX::SimpleMath::Matrix              m_view;
    DirectX::SimpleMath::Matrix              m_projection;

#ifdef DXTK_AUDIO
    std::unique_ptr<DirectX::AudioEngine>                                   m_audEngine;
    std::unique_ptr<DirectX::WaveBank>                                      m_waveBank;
    std::unique_ptr<DirectX::SoundEffect>                                   m_soundEffect;
    std::unique_ptr<DirectX::SoundEffectInstance>                           m_effect1;
    std::unique_ptr<DirectX::SoundEffectInstance>                           m_effect2;
    uint32_t                                                                m_audioEvent;
    float                                                                   m_audioTimerAcc;
    bool                                                                    m_retryDefault;
#endif
};