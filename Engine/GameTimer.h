#pragma once
#include "StepTimer.h"

class GameTimer
{
public:
    GameTimer();
    ~GameTimer();

    void SetStartTime(DX::StepTimer& stepTimer, float totalTime = 0.0f);
    void Start();
    void Stop();
    void Restart(const float newTotalTime = -1.0f);
    std::string GetFormattedTime() const;
	void Render(std::unique_ptr<DirectX::SpriteBatch>& sprites, std::unique_ptr<DirectX::SpriteFont>& font);
    bool IsExpired() const;
    void UpdateRemainingTime();

private:
    float m_totalTime;  // Tracks total time including preset start time
    float m_startTime = 0.0f;
	float m_remainingTime = 0.0f;
    bool m_isRunning = false;

    DX::StepTimer* m_gameStepTimer = nullptr;

    float GetRemainingTime();
};

