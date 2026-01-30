#include "pch.h"
#include "Game.h"

#ifdef DXTK_AUDIO
#include <Dbt.h>
#endif
using namespace DirectX;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

namespace
{
    std::shared_ptr<Game> g_game;
}

// Entry point
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    if (!XMVerifyCPUSupport()) return 1;

    HRESULT hr = CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);
    if (FAILED(hr)) return 1;

    g_game = std::make_shared<Game>();

    int width, height;
    g_game->GetDefaultSize(width, height);

    // Register class and create window
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIconW(hInstance, L"IDI_ICON");
    wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"DirectXTKSimpleSampleWindowClass";
    if (!RegisterClassExW(&wcex)) return 1;

    RECT rc = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(0, L"DirectXTKSimpleSampleWindowClass", L"DirectX Game", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) return 1;

    ShowWindow(hwnd, nCmdShow);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(g_game.get()));

    g_game->Initialize(hwnd, width, height);

    // Main loop
    MSG msg = {};
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            g_game->Tick();
        }
    }

    g_game.reset();
    CoUninitialize();

    return static_cast<int>(msg.wParam);
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto game = reinterpret_cast<Game*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;

    switch (message)
    {
    case WM_PAINT:
        if (game) game->Tick();
        else {
            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

        // ... (Keep existing mouse/keyboard handlers, omitted for brevity but should be identical to your input) ...

        // Ensure you handle WM_SIZE, WM_ACTIVATEAPP etc. forwarding to 'game' instance
    case WM_SIZE:
        if (game) game->OnWindowSizeChanged(LOWORD(lParam), HIWORD(lParam));
        break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}