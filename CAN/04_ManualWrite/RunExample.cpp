#define NOMINMAX 
#include "04_ManualWrite.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <cstring>

class Debounce {
public:
    Debounce(int debounceMS) { this->debounceMS = debounceMS; }
    void mark() { lastChangeTime = GetTickCount(); }
    bool isReady() { DWORD now = GetTickCount(); return (now - lastChangeTime) >= debounceMS; }
    void setDebounce(int ms) { debounceMS = ms; }

    int debounceMS;
    DWORD lastChangeTime = 0;
};

enum DriveMode {
    Static,
    Dynamic
};

PedalValues pedalValues;
DriveMode mode = Static;

BYTE rawData[8] = { 0 };

int pAccelCount = 0;
int middlePedalPressure = 0;
int rightPedalPressure = 0;
int rMiddlePedalPressure = 0;
int rRightPedalPressure = 0;
int speedIndex = 0;
int rpEdge = 0;
int mpEdge = 0;

const int SPEED_HISTORY_SIZE = 100;

int speedHistory[SPEED_HISTORY_SIZE] = { 0 };

constexpr int maxSpeed = 300;

bool leftPedalPressed = false;
bool middlePedalPressed = false;
bool rightPedalPressed = false;

Debounce lpDebounce(1000);
Debounce mpDebounce(500);
Debounce rpDebounce(1000);

std::atomic<bool> running{ true };
HWND g_hWnd = NULL;

void ProcessValues() {
    pedalValues.accel = pAccelCount;
    pedalValues.drivemode = mode;
    pedalValues.middlePressure = middlePedalPressure;
    pedalValues.rightPressure = rightPedalPressure;
}

void ProcessMiddlePedal(const BYTE* data)
{
    if (data[6] > 0) {
        leftPedalPressed = true;
        if (data[6] > 14 && lpDebounce.isReady()) {
            if (mode == Static) {
                mode = Dynamic;
            }
            else if (mode == Dynamic) {
                mode = Static;
                pAccelCount = 0;
            }
            lpDebounce.mark();
        }
    }
    else {
        leftPedalPressed = false;
    }
}

void ProcessRightPedal(const BYTE* data)
{
    if (data[2] > 0) {
        rightPedalPressed = true;
        if (mode == Static) {
            if (data[2] > 13 && rpEdge < 2) {
                rpEdge++;
                pAccelCount += 10;
            }
        }
        else if (mode == Dynamic) {
                pAccelCount < 300 ? pAccelCount + (rightPedalPressure / 5) : pAccelCount += 0;
                rightPedalPressure < 2 ? pAccelCount += 1 : pAccelCount += 0;
        }
        else {
            rightPedalPressed = false;
            rpEdge = 0;
        }
    }
    else {
        rightPedalPressed = false;
        rpEdge = 0;
    }
}

void ProcessLeftPedal(const BYTE* data)
{
    if (data[4] > 0) {
        middlePedalPressed = true;
        if (data[4] > 30 && mpDebounce.isReady()) {
            if (mode == Static) {
                mpEdge++;
                mpEdge < 2 ? pAccelCount -= 20 : pAccelCount += 0;
            }
            else if (mode == Dynamic) {
                pAccelCount -= (middlePedalPressure * 2);
            }
            mpDebounce.mark();
        }
    }
    else {
        middlePedalPressed = false;
        mpEdge = 0;
    }
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INPUT: {
        UINT dwSize = 0;
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));

        if (dwSize > 0) {
            BYTE* lpb = new BYTE[dwSize];
            if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) == dwSize) {
                RAWINPUT* raw = (RAWINPUT*)lpb;

                if (raw->header.dwType == RIM_TYPEHID) {
                    BYTE* data = raw->data.hid.bRawData;
                    UINT dataSize = raw->data.hid.dwSizeHid;

                    if (dataSize > 2) rightPedalPressure = data[2];
                    if (dataSize > 4) middlePedalPressure = data[4];

                    ProcessRightPedal(data);
                    ProcessMiddlePedal(data);
                    ProcessLeftPedal(data);
                }
            }
            delete[] lpb;
        }
        return 0;
    }

    case WM_DESTROY:
        running = false;
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

DWORD WINAPI MessagePumpThread(LPVOID) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"PedalWindow";

    if (!RegisterClassEx(&wc)) {
        std::cout << "Failed to register window class!" << std::endl;
        return 1;
    }

    g_hWnd = CreateWindowEx(
        0, wc.lpszClassName, L"Pedal Input",
        WS_OVERLAPPEDWINDOW, 0, 0, 100, 100,
        NULL, NULL, wc.hInstance, NULL);

    if (!g_hWnd) {
        std::cout << "Failed to create window!" << std::endl;
        return 1;
    }

    ShowWindow(g_hWnd, SW_SHOWMINIMIZED);

    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x04;
    rid.dwFlags = RIDEV_INPUTSINK;
    rid.hwndTarget = g_hWnd;

    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
        std::cout << "Failed to register HID!" << std::endl;
    }
    else {
        std::cout << "HID registered to window successfully!" << std::endl;
    }

    MSG msg;
    while (running) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            Sleep(1);
        }
    }

    DestroyWindow(g_hWnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

int main() {
    std::cout << "====================================" << std::endl;
    std::cout << "Pedal-to-CAN with Hidden Window" << std::endl;
    std::cout << "Press ESC to exit" << std::endl;
    std::cout << "====================================" << std::endl;

    // Start window thread
    HANDLE hThread = CreateThread(NULL, 0, MessagePumpThread, NULL, 0, NULL);

    Sleep(1000);

    // Initialize CAN - FIXED: No blocking constructor
    std::cout << "Initializing CAN..." << std::endl;

    ManualWrite canWriter;

    std::cout << "Main loop running..." << std::endl;

    while (running) {
        if (_kbhit()) {
            char key = _getch();
            if (key == 27) {
                std::cout << "ESC pressed. Shutting down..." << std::endl;
                running = false;
                break;
            }
        }

        std::cout << "\rAccel: " << pAccelCount
            << " | R: " << rightPedalPressure
            << " | M: " << middlePedalPressure
            << "    " << std::flush;
        
        ProcessValues();
        canWriter.SendAcceleration(pedalValues);

 //       canWriter.SendAcceleration(pAccelCount, rightPedalPressure, middlePedalPressure);
        Sleep(100);
    }

    running = false;
    WaitForSingleObject(hThread, 2000);
    CloseHandle(hThread);

    std::cout << "\nApplication terminated." << std::endl;
    return 0;
}