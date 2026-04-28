#define DIRECTINPUT_VERSION 0x0800

#include <windows.h>
#include <dinput.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

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
};

static AppState g_state;

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

static HWND CreateDummyWindow(HINSTANCE hInstance)
{
    const wchar_t CLASS_NAME[] = L"MozaFFBTestWindow";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = DummyWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"MOZA FFB UDP Steering",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 240,
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
        DISCL_FOREGROUND | DISCL_EXCLUSIVE
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

    Sleep(300);

    hr = g_state.device->Acquire();
    if (FAILED(hr))
    {
        PrintHresult("Acquire failed", hr);
        return false;
    }
    std::cout << "[Init] Acquire ok.\n";

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

void RunLiveSteeringTest()
{
    std::cout << "\n[Test] UDP steering mode\n";
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
    cf.lMagnitude = 1;

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

    std::cout << "[Test] ConstantForce created.\n";

    hr = fx->Start(1, 0);
    if (FAILED(hr))
    {
        PrintHresult("Start failed", hr);
        fx->Release();
        closesocket(udpSocket);
        WSACleanup();
        return;
    }

    std::cout << "[Test] ConstantForce started.\n";

    const LONG MAX_FORCE = DI_FFNOMINALMAX / 2;

    // Your previous calibration:
    // positive signed force = left
    // negative signed force = right
    //
    // Python convention:
    // positive steering = yellow line is to the right
    //
    // Therefore invert steering before applying force.
    const int FORCE_SIGN = -1;

    float latestSteering = 0.0f;

    auto lastPacketTime = std::chrono::steady_clock::now();
    auto lastPrintTime = std::chrono::steady_clock::now();

    while (!(GetAsyncKeyState(VK_ESCAPE) & 0x8000))
    {
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

                if (latestSteering > 1.0f)
                    latestSteering = 1.0f;
                else if (latestSteering < -1.0f)
                    latestSteering = -1.0f;

                lastPacketTime = std::chrono::steady_clock::now();
            }
            catch (...)
            {
                std::cout << "[UDP] Invalid steering packet: " << buffer << "\n";
            }
        }

        auto now = std::chrono::steady_clock::now();

        auto msSinceLastPacket =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastPacketTime
            ).count();

        // Safety fallback:
        // If steering packets stop, remove force from the wheel.
        if (msSinceLastPacket > 500)
        {
            latestSteering = 0.0f;
        }

        LONG signedForce = static_cast<LONG>(
            latestSteering * MAX_FORCE * FORCE_SIGN
            );

        cf.lMagnitude = signedForce;

        hr = fx->SetParameters(&effect, DIEP_TYPESPECIFICPARAMS);
        if (FAILED(hr))
        {
            PrintHresult("SetParameters failed", hr);
            break;
        }

        auto msSinceLastPrint =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastPrintTime
            ).count();

        if (msSinceLastPrint > 250)
        {
            std::cout << "[UDP] steering=" << latestSteering
                << " signedForce=" << signedForce << "\n";
            lastPrintTime = now;
        }

        Sleep(20);
    }

    std::cout << "[Test] Stopping force.\n";

    cf.lMagnitude = 0;
    fx->SetParameters(&effect, DIEP_TYPESPECIFICPARAMS);

    fx->Stop();
    fx->Release();

    closesocket(udpSocket);
    WSACleanup();
}

static bool RunSpringTest()
{
    std::cout << "\n[Test] Spring effect\n";

    DWORD axes[1] = { g_state.xAxisObjectId };
    LONG direction[1] = { 0 };

    DICONDITION cond = {};
    cond.lOffset = 5000;
    cond.lPositiveCoefficient = 10000;
    cond.lNegativeCoefficient = 10000;
    cond.dwPositiveSaturation = DI_FFNOMINALMAX;
    cond.dwNegativeSaturation = DI_FFNOMINALMAX;
    cond.lDeadBand = 0;

    DIEFFECT effect = {};
    effect.dwSize = sizeof(effect);
    effect.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTIDS;
    effect.dwDuration = 3 * DI_SECONDS;
    effect.dwSamplePeriod = 0;
    effect.dwGain = DI_FFNOMINALMAX;
    effect.dwTriggerButton = DIEB_NOTRIGGER;
    effect.cAxes = 1;
    effect.rgdwAxes = axes;
    effect.rglDirection = direction;
    effect.cbTypeSpecificParams = sizeof(cond);
    effect.lpvTypeSpecificParams = &cond;

    IDirectInputEffect* fx = nullptr;

    HRESULT hr = g_state.device->CreateEffect(GUID_Spring, &effect, &fx, nullptr);
    if (FAILED(hr))
    {
        PrintHresult("CreateEffect(Spring) failed", hr);
        return false;
    }

    std::cout << "[Test] Spring CreateEffect ok.\n";

    hr = fx->Start(1, 0);
    if (FAILED(hr))
    {
        PrintHresult("Start(Spring) failed", hr);
        fx->Release();
        return false;
    }

    std::cout << "[Test] Spring started for 3s.\n";

    Sleep(3000);

    hr = fx->Stop();
    if (FAILED(hr))
    {
        PrintHresult("Stop(Spring) failed", hr);
    }
    else
    {
        std::cout << "[Test] Spring stopped.\n";
    }

    fx->Release();
    return true;
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
    std::cout << "=== MOZA FFB UDP Steering Test ===\n";
    std::cout << "Close Pit House before running.\n";
    std::cout << "Click the test window if it appears.\n";
    std::cout << "Start yellow_line_tracking.py from WSL after this is listening.\n\n";

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

    EnumerateAxesAndEffects();
    TryDisableAutocenter();

    RunLiveSteeringTest();

    // Disabled during UDP steering testing so it does not unexpectedly move the wheel afterward.
    // RunSpringTest();

    Cleanup();

    std::cout << "\nDone.\n";
    return 0;
}