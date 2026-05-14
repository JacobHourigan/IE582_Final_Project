#define DIRECTINPUT_VERSION 0x0800

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <dinput.h>

#include <chrono>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <string>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "ws2_32.lib")

struct AppState
{
    IDirectInput8* di = nullptr;
    IDirectInputDevice8* device = nullptr;
    HWND hwnd = nullptr;

    DWORD xAxisObjectId = 0;
    bool foundXAxisActuator = false;

    LONG axisMin = -10000;
    LONG axisMax = 10000;
    LONG axisCenter = 0;
};

static AppState g_state;

template <typename T>
static T Clamp(T value, T low, T high)
{
    return (value < low) ? low : (value > high ? high : value);
}

static std::wstring GuidToName(const GUID& guid)
{
    if (guid == GUID_ConstantForce) return L"GUID_ConstantForce";
    if (guid == GUID_RampForce) return L"GUID_RampForce";
    if (guid == GUID_Square) return L"GUID_Square";
    if (guid == GUID_Sine) return L"GUID_Sine";
    if (guid == GUID_Triangle) return L"GUID_Triangle";
    if (guid == GUID_SawtoothUp) return L"GUID_SawtoothUp";
    if (guid == GUID_SawtoothDown) return L"GUID_SawtoothDown";
    if (guid == GUID_Spring) return L"GUID_Spring";
    if (guid == GUID_Damper) return L"GUID_Damper";
    if (guid == GUID_Inertia) return L"GUID_Inertia";
    if (guid == GUID_Friction) return L"GUID_Friction";
    return L"UNKNOWN_GUID";
}

static void PrintHresult(const char* label, HRESULT hr)
{
    std::cout << label << ": 0x"
        << std::hex << std::setw(8) << std::setfill('0') << hr
        << std::dec << std::setfill(' ') << '\n';
}

static LRESULT CALLBACK DummyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void PumpWindowMessages()
{
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

static HWND CreateDummyWindow(HINSTANCE hInstance)
{
    const wchar_t CLASS_NAME[] = L"MozaFFBTargetAngleWindow";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = DummyWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"MOZA FFB Target Angle",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 360, 220,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    return hwnd;
}

static BOOL CALLBACK EnumDeviceCallback(const DIDEVICEINSTANCE* instance, VOID* context)
{
    std::wcout << L"[Device] Product: " << instance->tszProductName << L'\n';
    std::wcout << L"[Device] Instance: " << instance->tszInstanceName << L'\n';

    HRESULT hr = g_state.di->CreateDevice(instance->guidInstance, &g_state.device, nullptr);
    if (FAILED(hr))
    {
        PrintHresult("CreateDevice failed", hr);
        return DIENUM_CONTINUE;
    }

    DIDEVCAPS caps = {};
    caps.dwSize = sizeof(caps);

    hr = g_state.device->GetCapabilities(&caps);
    if (FAILED(hr))
    {
        PrintHresult("GetCapabilities failed", hr);
        g_state.device->Release();
        g_state.device = nullptr;
        return DIENUM_CONTINUE;
    }

    std::cout << "[Device] Caps flags: 0x" << std::hex << caps.dwFlags << std::dec << '\n';

    if (!(caps.dwFlags & DIDC_FORCEFEEDBACK))
    {
        std::cout << "[Device] No force feedback support.\n";
        g_state.device->Release();
        g_state.device = nullptr;
        return DIENUM_CONTINUE;
    }

    std::cout << "[Device] Force feedback supported.\n";
    return DIENUM_STOP;
}

static BOOL CALLBACK EnumObjectsCallback(const DIDEVICEOBJECTINSTANCE* obj, VOID* context)
{
    std::wcout << L"[Axis] " << obj->tszName
        << L" dwType=0x" << std::hex << obj->dwType
        << L" guid=" << GuidToName(obj->guidType)
        << std::dec << L'\n';

    std::wstring name = obj->tszName;
    if (!g_state.foundXAxisActuator && name.find(L"X Axis") != std::wstring::npos)
    {
        g_state.xAxisObjectId = obj->dwType;
        g_state.foundXAxisActuator = true;
    }

    return DIENUM_CONTINUE;
}

static BOOL CALLBACK EnumEffectsCallback(const DIEFFECTINFO* info, VOID* context)
{
    std::wcout << L"[Effect] " << info->tszName
        << L" guid=" << GuidToName(info->guid)
        << L" staticParams=0x" << std::hex << info->dwStaticParams
        << L" dynamicParams=0x" << info->dwDynamicParams
        << std::dec << L'\n';
    return DIENUM_CONTINUE;
}

static bool InitDirectInput()
{
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    g_state.hwnd = CreateDummyWindow(hInstance);

    if (!g_state.hwnd)
    {
        std::cout << "Failed to create dummy window.\n";
        return false;
    }

    ShowWindow(g_state.hwnd, SW_SHOW);
    SetForegroundWindow(g_state.hwnd);
    SetActiveWindow(g_state.hwnd);
    SetFocus(g_state.hwnd);

    std::cout << "[Init] HWND: " << g_state.hwnd << '\n';

    HRESULT hr = DirectInput8Create(
        hInstance,
        DIRECTINPUT_VERSION,
        IID_IDirectInput8,
        reinterpret_cast<void**>(&g_state.di),
        nullptr
    );

    if (FAILED(hr))
    {
        PrintHresult("DirectInput8Create failed", hr);
        return false;
    }

    std::cout << "[Init] DirectInput initialized.\n";
    return true;
}

static bool AcquireDeviceWithRetry(int attempts = 50, int sleepMs = 250)
{
    HRESULT hr = E_FAIL;

    std::cout << "[Init] Attempting to acquire device...\n";
    std::cout << "[Init] Click the MOZA FFB Target Angle window if needed.\n";

    for (int attempt = 1; attempt <= attempts; ++attempt)
    {
        PumpWindowMessages();

        ShowWindow(g_state.hwnd, SW_SHOW);
        SetForegroundWindow(g_state.hwnd);
        SetActiveWindow(g_state.hwnd);
        SetFocus(g_state.hwnd);

        hr = g_state.device->Acquire();
        if (SUCCEEDED(hr))
        {
            std::cout << "[Init] Acquire ok on attempt " << attempt << ".\n";
            return true;
        }

        std::cout << "[Init] Acquire attempt " << attempt << " failed: 0x"
            << std::hex << std::setw(8) << std::setfill('0') << hr
            << std::dec << std::setfill(' ') << "\n";

        Sleep(sleepMs);
    }

    PrintHresult("Acquire failed after retries", hr);
    return false;
}

static bool OpenFFBDevice()
{
    HRESULT hr = g_state.di->EnumDevices(
        DI8DEVCLASS_GAMECTRL,
        EnumDeviceCallback,
        nullptr,
        DIEDFL_ATTACHEDONLY | DIEDFL_FORCEFEEDBACK
    );

    if (FAILED(hr))
    {
        PrintHresult("EnumDevices failed", hr);
        return false;
    }

    if (!g_state.device)
    {
        std::cout << "[Init] No FFB device selected.\n";
        return false;
    }

    hr = g_state.device->SetDataFormat(&c_dfDIJoystick);
    if (FAILED(hr))
    {
        PrintHresult("SetDataFormat failed", hr);
        return false;
    }
    std::cout << "[Init] SetDataFormat ok.\n";

    hr = g_state.device->SetCooperativeLevel(
        g_state.hwnd,
        DISCL_BACKGROUND | DISCL_EXCLUSIVE
    );
    if (FAILED(hr))
    {
        PrintHresult("SetCooperativeLevel failed", hr);
        return false;
    }
    std::cout << "[Init] SetCooperativeLevel ok.\n";

    DIPROPDWORD axisMode = {};
    axisMode.diph.dwSize = sizeof(DIPROPDWORD);
    axisMode.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    axisMode.diph.dwObj = 0;
    axisMode.diph.dwHow = DIPH_DEVICE;
    axisMode.dwData = DIPROPAXISMODE_ABS;

    hr = g_state.device->SetProperty(DIPROP_AXISMODE, &axisMode.diph);
    if (FAILED(hr))
    {
        PrintHresult("SetProperty(DIPROP_AXISMODE) failed", hr);
        return false;
    }
    std::cout << "[Init] Axis mode ABS ok.\n";

    if (!AcquireDeviceWithRetry())
        return false;

    return true;
}

static bool EnumerateAxesAndEffects()
{
    HRESULT hr = g_state.device->EnumObjects(
        EnumObjectsCallback,
        nullptr,
        DIDFT_AXIS | DIDFT_FFACTUATOR
    );

    if (FAILED(hr))
    {
        PrintHresult("EnumObjects failed", hr);
        return false;
    }

    if (!g_state.foundXAxisActuator)
    {
        std::cout << "[Init] No X-axis actuator found.\n";
        return false;
    }

    std::cout << "[Init] Using X-axis actuator object id: 0x"
        << std::hex << g_state.xAxisObjectId << std::dec << '\n';

    std::cout << "[Init] Enumerating supported effects...\n";
    hr = g_state.device->EnumEffects(
        EnumEffectsCallback,
        nullptr,
        DIEFT_ALL
    );

    if (FAILED(hr))
    {
        PrintHresult("EnumEffects failed", hr);
        return false;
    }

    return true;
}

static bool ConfigureXAxisRange()
{
    DIPROPRANGE rangeProp = {};
    rangeProp.diph.dwSize = sizeof(DIPROPRANGE);
    rangeProp.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    rangeProp.diph.dwObj = g_state.xAxisObjectId;
    rangeProp.diph.dwHow = DIPH_BYID;
    rangeProp.lMin = g_state.axisMin;
    rangeProp.lMax = g_state.axisMax;

    HRESULT hr = g_state.device->SetProperty(DIPROP_RANGE, &rangeProp.diph);
    if (FAILED(hr))
    {
        PrintHresult("SetProperty(DIPROP_RANGE) failed", hr);
        return false;
    }

    std::cout << "[Init] X axis range set to [" << g_state.axisMin
        << ", " << g_state.axisMax << "]\n";
    return true;
}

static bool TryDisableAutocenter()
{
    DIPROPDWORD ac = {};
    ac.diph.dwSize = sizeof(DIPROPDWORD);
    ac.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    ac.diph.dwObj = 0;
    ac.diph.dwHow = DIPH_DEVICE;
    ac.dwData = FALSE;

    HRESULT hr = g_state.device->SetProperty(DIPROP_AUTOCENTER, &ac.diph);
    if (FAILED(hr))
    {
        PrintHresult("SetProperty(DIPROP_AUTOCENTER) failed", hr);
        return false;
    }

    std::cout << "[Init] Auto-center disabled.\n";
    return true;
}

static bool ReadJoystickState(DIJOYSTATE& js)
{
    HRESULT hr = g_state.device->Poll();

    if (FAILED(hr))
    {
        g_state.device->Acquire();
    }

    hr = g_state.device->GetDeviceState(sizeof(js), &js);
    if (FAILED(hr))
    {
        g_state.device->Acquire();
        hr = g_state.device->GetDeviceState(sizeof(js), &js);
        if (FAILED(hr))
        {
            PrintHresult("GetDeviceState failed", hr);
            return false;
        }
    }

    return true;
}

static bool CalibrateWheelCenter()
{
    std::cout << "[Cal] Centering calibration starting.\n";
    std::cout << "[Cal] Please let go of the wheel and do not touch it for ~1 second.\n";

    long long sum = 0;
    int count = 0;

    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count() < 1000)
    {
        DIJOYSTATE js = {};
        if (ReadJoystickState(js))
        {
            sum += js.lX;
            ++count;
        }

        PumpWindowMessages();
        Sleep(20);
    }

    if (count == 0)
    {
        std::cout << "[Cal] Failed to read joystick state during calibration.\n";
        return false;
    }

    g_state.axisCenter = static_cast<LONG>(sum / count);

    std::cout << "[Cal] axisCenter = " << g_state.axisCenter << "\n";
    return true;
}

static double RawAxisToAngleDeg(LONG rawAxis, double maxWheelAngleDeg)
{
    const double halfRange = (g_state.axisMax - g_state.axisMin) / 2.0;
    if (halfRange <= 0.0)
        return 0.0;

    double normalized = static_cast<double>(rawAxis - g_state.axisCenter) / halfRange;
    normalized = Clamp(normalized, -1.0, 1.0);

    return normalized * maxWheelAngleDeg;
}

void RunLiveSteeringTest()
{
    std::cout << "\n[Test] UDP steering target-angle mode\n";
    std::cout << "[Test] Listening for steering values on UDP port 5005.\n";
    std::cout << "[Test] Expected values: -1.0 to +1.0\n";
    std::cout << "[Test] Press ESC to quit.\n";

    WSADATA wsaData;
    int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaResult != 0)
    {
        std::cout << "[UDP] WSAStartup failed: " << wsaResult << "\n";
        return;
    }

    SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET)
    {
        std::cout << "[UDP] socket() failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return;
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(5005);

    if (bind(
        udpSocket,
        reinterpret_cast<sockaddr*>(&serverAddr),
        sizeof(serverAddr)
    ) == SOCKET_ERROR)
    {
        std::cout << "[UDP] bind() failed: " << WSAGetLastError() << "\n";
        closesocket(udpSocket);
        WSACleanup();
        return;
    }

    u_long nonBlocking = 1;
    ioctlsocket(udpSocket, FIONBIO, &nonBlocking);

    DWORD axes[1] = { g_state.xAxisObjectId };
    LONG direction[1] = { 0 };

    DICONSTANTFORCE cf = {};
    cf.lMagnitude = 0;

    DIEFFECT effect = {};
    effect.dwSize = sizeof(effect);
    effect.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTIDS;
    effect.dwDuration = INFINITE;
    effect.dwSamplePeriod = 0;
    effect.dwGain = DI_FFNOMINALMAX;
    effect.dwTriggerButton = DIEB_NOTRIGGER;
    effect.dwTriggerRepeatInterval = 0;
    effect.cAxes = 1;
    effect.rgdwAxes = axes;
    effect.rglDirection = direction;
    effect.lpEnvelope = nullptr;
    effect.cbTypeSpecificParams = sizeof(cf);
    effect.lpvTypeSpecificParams = &cf;
    effect.dwStartDelay = 0;

    IDirectInputEffect* fx = nullptr;

    HRESULT hr = g_state.device->CreateEffect(
        GUID_ConstantForce,
        &effect,
        &fx,
        nullptr
    );

    if (FAILED(hr))
    {
        PrintHresult("CreateEffect failed", hr);
        closesocket(udpSocket);
        WSACleanup();
        return;
    }

    hr = fx->Start(1, 0);
    if (FAILED(hr))
    {
        PrintHresult("Start failed", hr);
        fx->Release();
        closesocket(udpSocket);
        WSACleanup();
        return;
    }

    std::cout << "[Test] ConstantForce effect started.\n";

    // ------------------------------------------------------------
    // TUNING CONSTANTS
    // ------------------------------------------------------------

    // Keep this modest. The Python steering value does not need to map
    // to full real wheel lock.
    constexpr double MAX_WHEEL_ANGLE_DEG = 120.0;

    // Start gentle. Increase KP only after direction is correct.
    constexpr double KP = 18.0;

    // Damping. Increase if the wheel oscillates.
    constexpr double KD = 0.45;

    // Force clamp. Start lower than full force for safety.
    constexpr LONG MAX_FORCE = DI_FFNOMINALMAX / 3;

    // Logical convention used in this controller:
    // positive logical steering/angle/force = right
    //
    // DirectInput signed force convention from your earlier test:
    // positive signed force = left
    // negative signed force = right
    //
    // Therefore logical right force must become negative DirectInput force.
    constexpr int DIRECTINPUT_FORCE_SIGN = -1;

    // This is the important one to flip if the wheel runs away to full lock.
    //
    // If physically turning the wheel right makes currentLogicalDeg increase,
    // leave this as +1.
    //
    // If physically turning the wheel right makes currentLogicalDeg decrease,
    // set this to -1.
    constexpr int WHEEL_AXIS_SIGN = 1;

    constexpr int PACKET_TIMEOUT_MS = 500;

    // Small deadband so tiny steering corrections do not constantly push the wheel.
    constexpr double STEERING_DEADBAND = 0.03;

    // ------------------------------------------------------------

    float latestSteering = 0.0f;
    double smoothedSteering = 0.0;
    double previousLogicalDeg = 0.0;

    auto lastPacketTime = std::chrono::steady_clock::now();
    auto lastLoopTime = std::chrono::steady_clock::now();
    auto lastPrintTime = std::chrono::steady_clock::now();

    while (!(GetAsyncKeyState(VK_ESCAPE) & 0x8000))
    {
        PumpWindowMessages();

        char buffer[128] = {};
        sockaddr_in senderAddr = {};
        int senderAddrSize = sizeof(senderAddr);

        int bytesReceived = recvfrom(
            udpSocket,
            buffer,
            sizeof(buffer) - 1,
            0,
            reinterpret_cast<sockaddr*>(&senderAddr),
            &senderAddrSize
        );

        if (bytesReceived > 0)
        {
            buffer[bytesReceived] = '\0';

            try
            {
                latestSteering = std::stof(buffer);
                latestSteering = Clamp(latestSteering, -1.0f, 1.0f);
                lastPacketTime = std::chrono::steady_clock::now();
            }
            catch (...)
            {
                std::cout << "[UDP] Invalid steering packet: " << buffer << "\n";
            }
        }

        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - lastLoopTime).count();
        lastLoopTime = now;

        if (dt <= 0.000001)
            dt = 0.01;

        const auto msSinceLastPacket =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastPacketTime
            ).count();

        if (msSinceLastPacket > PACKET_TIMEOUT_MS)
        {
            latestSteering = 0.0f;
        }

        // Smooth the incoming steering command.
        smoothedSteering = 0.85 * smoothedSteering + 0.15 * latestSteering;

        if (std::abs(smoothedSteering) < STEERING_DEADBAND)
        {
            smoothedSteering = 0.0;
        }

        DIJOYSTATE js = {};
        if (!ReadJoystickState(js))
        {
            std::cout << "[Wheel] Failed reading joystick state, skipping loop.\n";
            Sleep(20);
            continue;
        }

        const LONG currentRawAxis = js.lX;

        // RawAxisToAngleDeg returns raw device angle.
        // WHEEL_AXIS_SIGN converts it to our logical convention:
        // positive logical angle = right.
        const double currentRawDeg =
            RawAxisToAngleDeg(currentRawAxis, MAX_WHEEL_ANGLE_DEG);

        const double currentLogicalDeg =
            WHEEL_AXIS_SIGN * currentRawDeg;

        const double targetLogicalDeg =
            smoothedSteering * MAX_WHEEL_ANGLE_DEG;

        const double errorDeg =
            targetLogicalDeg - currentLogicalDeg;

        const double velocityDegPerSec =
            (currentLogicalDeg - previousLogicalDeg) / dt;

        previousLogicalDeg = currentLogicalDeg;

        // Logical force:
        // positive = push right
        // negative = push left
        double logicalForce =
            KP * errorDeg - KD * velocityDegPerSec;

        logicalForce = Clamp(
            logicalForce,
            static_cast<double>(-MAX_FORCE),
            static_cast<double>(MAX_FORCE)
        );

        // Convert logical force to DirectInput signed magnitude.
        LONG signedDirectInputForce =
            static_cast<LONG>(logicalForce * DIRECTINPUT_FORCE_SIGN);

        signedDirectInputForce = Clamp(
            signedDirectInputForce,
            -MAX_FORCE,
            MAX_FORCE
        );

        cf.lMagnitude = signedDirectInputForce;

        hr = fx->SetParameters(&effect, DIEP_TYPESPECIFICPARAMS);
        if (FAILED(hr))
        {
            PrintHresult("SetParameters failed", hr);
            break;
        }

        const auto msSinceLastPrint =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastPrintTime
            ).count();

        if (msSinceLastPrint > 250)
        {
            std::cout << std::fixed << std::setprecision(2)
                << "[CTRL] rawSteer=" << latestSteering
                << " smoothSteer=" << smoothedSteering
                << " targetDeg=" << targetLogicalDeg
                << " currentRaw=" << currentRawAxis
                << " rawDeg=" << currentRawDeg
                << " logicalDeg=" << currentLogicalDeg
                << " errorDeg=" << errorDeg
                << " logicalForce=" << logicalForce
                << " DIForce=" << signedDirectInputForce
                << "\n";

            lastPrintTime = now;
        }

        Sleep(10);
    }

    std::cout << "[Test] Stopping force.\n";

    cf.lMagnitude = 0;
    fx->SetParameters(&effect, DIEP_TYPESPECIFICPARAMS);

    fx->Stop();
    fx->Release();

    closesocket(udpSocket);
    WSACleanup();
}

static void Cleanup()
{
    if (g_state.device)
    {
        g_state.device->Unacquire();
        g_state.device->Release();
        g_state.device = nullptr;
    }

    if (g_state.di)
    {
        g_state.di->Release();
        g_state.di = nullptr;
    }

    if (g_state.hwnd)
    {
        DestroyWindow(g_state.hwnd);
        g_state.hwnd = nullptr;
    }
}

int main()
{
    std::cout << "=== MOZA FFB UDP Steering Test (Target Angle PD) ===\n";
    std::cout << "Close MOZA Pit House before running.\n";
    std::cout << "Run this program as Administrator.\n";
    std::cout << "Start yellow_line_tracking.py after this program is listening.\n\n";

    if (!InitDirectInput())
    {
        Cleanup();
        return 1;
    }

    if (!OpenFFBDevice())
    {
        Cleanup();
        return 1;
    }

    if (!EnumerateAxesAndEffects())
    {
        Cleanup();
        return 1;
    }

    if (!ConfigureXAxisRange())
    {
        Cleanup();
        return 1;
    }

    TryDisableAutocenter();

    if (!CalibrateWheelCenter())
    {
        Cleanup();
        return 1;
    }

    RunLiveSteeringTest();

    Cleanup();

    std::cout << "\nDone.\n";
    return 0;
}