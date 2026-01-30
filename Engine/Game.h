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
#include "ModelClass.h"
#include "ObjectManager.h"
#include "Renderer.h"

// DirectXTK Includes
#include "CommonStates.h"
#include "SpriteBatch.h"
#include "SpriteFont.h"

// Forward declarations
class FractalObstacle;

class Game final : public DX::IDeviceNotify, public std::enable_shared_from_this<Game>
{
public:
    Game() noexcept(false);
    ~Game();

    void Initialize(HWND window, int width, int height);
    void GetDefaultSize(int& width, int& height) const;
    void Tick();

    void OnDeviceLost() override;
    void OnDeviceRestored() override;

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
    void Update(DX::StepTimer const& timer);
    void Render();

    // System
    void ExitGame(); // Private exit handler
    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Game Logic
    void SetupImGUI();
    void SetupDrone();
    void UpdateCameraMovement();
    void UpdateDroneMovement();
    void ChangeTargetRegion();
    void CheckDroneRegionProgress(float localX, float localZ);
    void HandleTimerExpiration();
    void RestartScene();
    bool IsWin();
    void OnWin();

    // Procedural Generation
    void InitializeRegionRules();
    void GenerateFractalObstacles();

    // --- Member Variables ---

    std::unique_ptr<DX::DeviceResources>    m_deviceResources;
    std::unique_ptr<Renderer>               m_renderer;
    DX::StepTimer                           m_timer;
    Input                                   m_input;
    InputCommands                           m_gameInputCommands;
    GameTimer                               m_gameTimer;

    // Scene Objects
    Terrain                                 m_Terrain;
    ModelClass                              m_Drone;
    ModelClass                              m_ObstacleModel;
    std::unique_ptr<ObjectManager>          m_objectManager;
    std::vector<FractalObstacle>            m_fractalObstacles;

    Light                                   m_Light;
    Light                                   m_Drone_Light;
    Camera                                  m_Camera01;

    // State Variables
    int                                     m_level = 1;
    DirectX::SimpleMath::Vector3            m_cameraPosition;
    DirectX::SimpleMath::Vector3            m_cameraRotation;
    float                                   m_localDroneX = 0.0f;
    float                                   m_localDroneZ = 0.0f;

    // Terrain Config
    float                                   m_terrainScale = 0.1f;
    DirectX::SimpleMath::Vector3            m_terrainTranslation = DirectX::SimpleMath::Vector3(0.0f, -0.6f, 0.0f);

    // Gameplay Config
    Enums::COLOUR                           m_targetRegionColour;
    DirectX::SimpleMath::Vector4            m_targetRegionColourVector;

    // Matrices
    DirectX::SimpleMath::Matrix             m_projection;

    // Input State
    int                                     m_lastMouseX = 0;
    int                                     m_lastMouseY = 0;
    bool                                    isMouseHoveringOverImGui = false;

    // L-System Rules
    enum class ObstacleType { SPIKES, CRYSTALS, VINES };
    struct RegionRule
    {
        Enums::COLOUR regionColour;
        ObstacleType obstacleType;
        std::string axiom;
        std::vector<std::pair<char, std::string>> rules;
        int iterations;
    };
    std::vector<RegionRule>                 m_regionRules;

#ifdef DXTK_AUDIO
    std::unique_ptr<DirectX::AudioEngine>           m_audEngine;
    std::unique_ptr<DirectX::WaveBank>              m_waveBank;
    std::unique_ptr<DirectX::SoundEffect>           m_soundEffect;
    std::unique_ptr<DirectX::SoundEffectInstance>   m_effect1;
    bool                                            m_retryAudio = false;
#endif
};