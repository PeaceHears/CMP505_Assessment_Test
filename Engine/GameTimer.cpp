#include "pch.h"
#include "GameTimer.h"

GameTimer::GameTimer()
{
    m_startTime = 0.0f;
    m_totalTime = 0.0f;
    m_remainingTime = 0.0f;
    m_isRunning = false;
}

GameTimer::~GameTimer()
{
	Stop();
}

void GameTimer::SetStartTime(DX::StepTimer& stepTimer, const float totalTime)
{
    m_gameStepTimer = &stepTimer;

    // Set new total time
    m_totalTime = static_cast<double>(totalTime);
    m_remainingTime = static_cast<double>(totalTime);

    // Reset start time and running state
    m_startTime = 0.0f;
    m_isRunning = false;
}

void GameTimer::Start()
{
    m_startTime = m_gameStepTimer->GetTotalSeconds();
    m_isRunning = true;
}

void GameTimer::Stop()
{
    UpdateRemainingTime();
    m_isRunning = false;
}

void GameTimer::Restart(const float newTotalTime)
{
    if (newTotalTime > 0)
    {
        m_totalTime = newTotalTime;
    }

    m_remainingTime = m_totalTime;
    Start();
}

float GameTimer::GetRemainingTime()
{
    UpdateRemainingTime();
    return m_remainingTime;
}

void GameTimer::UpdateRemainingTime()
{
    if (m_isRunning)
    {
        double currentTime = m_gameStepTimer->GetTotalSeconds();
        double elapsedTime = currentTime - m_startTime;

        // Subtract elapsed time from total time
        m_remainingTime = std::max(0.0, m_totalTime - elapsedTime);

        // Stop if time is up
        if (m_remainingTime <= 0.0)
        {
            m_isRunning = false;
            m_remainingTime = 0.0;
        }
    }
}

bool GameTimer::IsExpired() const
{
    return m_remainingTime <= 0.0f;
}

std::string GameTimer::GetFormattedTime() const
{
    float timeToDisplay = m_remainingTime;

    int minutes = static_cast<int>(timeToDisplay) / 60;
    int seconds = static_cast<int>(timeToDisplay) % 60;
    int milliseconds = static_cast<int>((timeToDisplay - std::floor(timeToDisplay)) * 1000);

    char buffer[16];
    sprintf_s(buffer, "%02d:%02d.%03d", minutes, seconds, milliseconds);
    return std::string(buffer);
}

void GameTimer::Render(std::unique_ptr<DirectX::SpriteBatch>& sprites, std::unique_ptr<DirectX::SpriteFont>& font)
{
    std::string timerText = GetFormattedTime();
    std::wstring wTimerText(timerText.begin(), timerText.end());

    // Color the timer based on remaining time
    DirectX::XMVECTOR color = DirectX::Colors::Green;

    float remainingTime = GetRemainingTime();

    if (remainingTime <= 30.0f)
    {
        color = DirectX::Colors::Yellow;
    }

    if (remainingTime <= 10.0f)
    {
        color = DirectX::Colors::Red;
    }

    sprites->Begin();

    font->DrawString(sprites.get(),
        wTimerText.c_str(),
        DirectX::XMFLOAT2(400.0f, 10.0f),
        color);

    sprites->End();
}
