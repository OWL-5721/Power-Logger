#include <windows.h>
#include <wtsapi32.h>
#include <fstream>
#include <string>
#include <ctime>

#pragma comment(lib, "Wtsapi32.lib")

SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
SERVICE_STATUS g_ServiceStatus = {};
HWND g_hwnd = NULL;

std::string getTime()
{
    time_t now = time(0);
    tm localTime;
    localtime_s(&localTime, &now);

    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTime);
    return std::string(buffer);
}

void logEvent(const std::string& event)
{
    std::string dir = "C:\\ProgramData\\PowerLogger";
    std::string path = dir + "\\system_event_log.txt";

    if (!CreateDirectoryA(dir.c_str(), NULL))
    {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS)
        {
            OutputDebugStringA("Directory creation failed\n");
        }
    }

    std::ofstream file(path, std::ios::app);

    if (!file.is_open())
    {
        OutputDebugStringA("Log file open failed\n");
        return;
    }

    file << getTime() << " - " << event << std::endl;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_WTSSESSION_CHANGE:
        if (wParam == WTS_SESSION_LOCK)
            logEvent("System Sleeping (Lock)");
        else if (wParam == WTS_SESSION_UNLOCK)
            logEvent("System Resumed (Unlock)");
        break;

    case WM_POWERBROADCAST:
        if (wParam == PBT_APMSUSPEND)
            logEvent("System Suspend");
        else if (wParam == PBT_APMRESUMEAUTOMATIC)
            logEvent("System Resume");
        break;

    case WM_DESTROY:
        WTSUnRegisterSessionNotification(hwnd);
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

DWORD WINAPI ServiceWorkerThread(LPVOID lpParam)
{
    const char CLASS_NAME[] = "PowerLoggerWindow";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClass(&wc))
    {
        logEvent("RegisterClass failed");
        return 1;
    }

    g_hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        "",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    if (!g_hwnd)
    {
        logEvent("Window creation failed");
        return 1;
    }

    // Register session notifications
    WTSRegisterSessionNotification(g_hwnd, NOTIFY_FOR_ALL_SESSIONS);

    logEvent("Service Started");

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DestroyWindow(g_hwnd);
    return 0;
}

void WINAPI ServiceCtrlHandler(DWORD CtrlCode)
{
    switch (CtrlCode)
    {
    case SERVICE_CONTROL_STOP:
        logEvent("Service Stop Requested");

        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

        PostQuitMessage(0);
        break;

    case SERVICE_CONTROL_SHUTDOWN:   
        logEvent("System Shutdown");

        PostQuitMessage(0);
        break;
    }
}

void WINAPI ServiceMain(DWORD argc, LPTSTR* argv)
{
    g_StatusHandle = RegisterServiceCtrlHandler("PowerLoggerService", ServiceCtrlHandler);
    if (!g_StatusHandle)
        return;

    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    // Accept stop + shutdown events
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

    HANDLE thread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);

    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    WaitForSingleObject(thread, INFINITE);

    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

int main()
{
    SERVICE_TABLE_ENTRY ServiceTable[] =
    {
        { (LPSTR)"PowerLoggerService", (LPSERVICE_MAIN_FUNCTION)ServiceMain },
        { NULL, NULL }
    };

    StartServiceCtrlDispatcher(ServiceTable);
    return 0;
}