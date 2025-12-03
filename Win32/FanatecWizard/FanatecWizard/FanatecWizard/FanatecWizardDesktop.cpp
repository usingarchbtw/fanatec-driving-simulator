/*
 This ensures application uses wide-character (unicode) apis.
*/
#ifndef UNICODE
#define UNICODE
#endif 
/*----------------------------------
| ControlDesk Setup
| In ControlDesk:
|
| Create new XCP configuration
|
| Set transport to TCP/IP
|
| Set IP to localhost and port to 5555
|
| Load your A2L file
-------------------------------------*/

/*
 windows.h: core win32 api
 gdiplus.h: gdi+ graphics
 cstdio: for swprintf functionality
 #pragma comment: links gdi+ at build time
*/
#define NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#include <cstdio>
#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <algorithm>   // std::max
#include <cstring>     // memcpy
#include "debounce.h"
#include "simplexcp.h"
// #include "xcp_server.h"
#include "a2l_generator.h"
#pragma comment (lib,"Gdiplus.lib")

// UI message for thread -> UI
#define WM_SPEED_UPDATE (WM_APP + 1)

// global vars
enum DriveMode {
    Static,
    Dynamic
};

DriveMode mode = Static;
int pAccelCount = 0;

bool leftPedalPressed = false;    // clutch -> changes modes (static & dynamic)
bool rightPedalPressed = false;    // acceleration 
bool middlePedalPressed = false;    // brake

int middlePedalPressure = 0;
int rightPedalPressure = 0;
int rMiddlePedalPressure = 0;    // the 'r' prefix indicates raw pressure values that are not normalized. 
int rRightPedalPressure = 0;

// debounce globals
Debounce lpDebounce(1000);  // 1 second debounce
Debounce mpDebounce(500);   // 0.5 second debounce
Debounce rpDebounce(1000);  // 1 second debounce for right pedal

// Thread control
static std::atomic<bool> g_speedThreadRunning{ false };
static std::thread g_speedThread;
static std::mutex g_speedMutex;

// speed history
BYTE rawData[8] = { 0 };  // stores the latest 8 bytes of raw HID input
const int SPEED_HISTORY_SIZE = 100;
int speedHistory[SPEED_HISTORY_SIZE] = { 0 };
int speedIndex = 0;

// file-scope constant used by WM_PAINT and other functions
constexpr int maxSpeed = 300;

// edge detection vars
int rpEdge = 0;            // the 'rp' prefix indicates right pedal
int mpEdge = 0;

// gdi globals
Gdiplus::Font* g_pFont = nullptr;
Gdiplus::SolidBrush* g_pBarBg = nullptr;
Gdiplus::SolidBrush* g_pBarFill = nullptr;
Gdiplus::SolidBrush* g_pTextBrush = nullptr;

// Forward declarations for new functions
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void SpeedThreadProc(HWND hwnd);
void StartSpeedThread(HWND hwnd);
void StopSpeedThread();
void InitializeGDIObjects();
void CleanupGDIObjects();
void HandleWMCreate(HWND hwnd);
void HandleWMDestroy();
void HandleWMPaint(HWND hwnd);
void HandleWMInput(HWND hwnd, LPARAM lParam);
void ProcessLeftPedal(const BYTE* data);
void ProcessRightPedal(const BYTE* data);
void ProcessMiddlePedal(const BYTE* data);
void DrawSpeedGauge(Gdiplus::Graphics& g);
void DrawSpeedHistoryGraph(Gdiplus::Graphics& g, int w, int h, Gdiplus::Font* font2, Gdiplus::SolidBrush* wTextBrush);
void DrawOdometer(Gdiplus::Graphics& g, Gdiplus::SolidBrush* wTextBrush);
void DrawRawDataPanel(Gdiplus::Graphics& g);
void DrawPedalBars(Gdiplus::Graphics& g, Gdiplus::Font* font, Gdiplus::SolidBrush* wTextBrush);
void DrawModeAndSpeed(Gdiplus::Graphics& g, Gdiplus::Font* font, Gdiplus::SolidBrush* wTextBrush);

void SpeedThreadProc(HWND hwnd)
{
    g_speedThreadRunning.store(true);
    const DWORD tickMs = 100;        // update every 100 ms
    const int decrementPerTick = 1;  // how much speed falls per tick

    while (g_speedThreadRunning.load()) {
        Sleep(tickMs);

        {
            std::lock_guard<std::mutex> lock(g_speedMutex);
            if (mode == Dynamic) {
                pAccelCount = std::max(0, pAccelCount - decrementPerTick);
            }

            speedIndex = (speedIndex + 1) % SPEED_HISTORY_SIZE;
            speedHistory[speedIndex] = pAccelCount;
        }

        PostMessage(hwnd, WM_SPEED_UPDATE, 0, 0);
    }
}

void StartSpeedThread(HWND hwnd)
{
    if (g_speedThread.joinable()) return;
    g_speedThread = std::thread([hwnd]() { SpeedThreadProc(hwnd); });
}

void StopSpeedThread()
{
    g_speedThreadRunning.store(false);
    if (g_speedThread.joinable()) g_speedThread.join();
}

void InitializeGDIObjects()
{
    g_pFont = new Gdiplus::Font(L"Segoe UI", 16);
    g_pBarBg = new Gdiplus::SolidBrush(Gdiplus::Color(255, 220, 220, 220));
    g_pBarFill = new Gdiplus::SolidBrush(Gdiplus::Color(255, 0, 180, 0));
    g_pTextBrush = new Gdiplus::SolidBrush(Gdiplus::Color(255, 0, 0, 0));
}

void CleanupGDIObjects()
{
    delete g_pFont;      g_pFont = nullptr;
    delete g_pBarBg;     g_pBarBg = nullptr;
    delete g_pBarFill;   g_pBarFill = nullptr;
    delete g_pTextBrush; g_pTextBrush = nullptr;
}

void HandleWMCreate(HWND hwnd)
{
    InitializeGDIObjects();
    StartSpeedThread(hwnd);
}

void HandleWMDestroy()
{
    CleanupGDIObjects();
    StopSpeedThread();
    // g_xcp_server.stop();
    PostQuitMessage(0);
    xcp_cleanup();
}

void DrawPedalBars(Gdiplus::Graphics& g, Gdiplus::Font* font, Gdiplus::SolidBrush* wTextBrush)
{
    auto DrawBar = [&](int x, int y, int bw, int bh, int value, const wchar_t* label) {
        if (value < 0) value = 0;
        if (value > 255) value = 255;

        g.FillRectangle(g_pBarBg, x, y, bw, bh);
        int fillW = (value * bw) / 255;

        Gdiplus::LinearGradientBrush gradientBrush(
            Gdiplus::Point(x, y),
            Gdiplus::Point(x + fillW, y + bh),
            Gdiplus::Color(255, 0, 200, 0),
            Gdiplus::Color(255, 0, 100, 0)
        );

        g.FillRectangle(&gradientBrush, x, y, fillW, bh);

        wchar_t buf[64];
        swprintf_s(buf, sizeof(buf) / sizeof(buf[0]), L"%s: %d", label, value);
        g.DrawString(buf, -1, font, Gdiplus::PointF(static_cast<float>(x + bw + 8), static_cast<float>(y)), wTextBrush);
        };

    DrawBar(40, 40, 300, 22, rightPedalPressure, L"Right (Accel)");
    DrawBar(40, 80, 300, 22, middlePedalPressure, L"Middle (Brake)");

    int lx = 40, ly = 120, lsize = 22;
    Gdiplus::SolidBrush indicatorBrush(leftPedalPressed ? Gdiplus::Color(255, 0, 200, 0) : Gdiplus::Color(255, 200, 0, 0));
    g.FillEllipse(&indicatorBrush, lx, ly, lsize, lsize);
    g.DrawString(leftPedalPressed ? L"Left (Clutch): Pressed" : L"Left (Clutch): Released",
        -1, font, Gdiplus::PointF(static_cast<float>(lx + lsize + 8), static_cast<float>(ly)), wTextBrush);
}

void DrawModeAndSpeed(Gdiplus::Graphics& g, Gdiplus::Font* font, Gdiplus::SolidBrush* wTextBrush)
{
    wchar_t modeBuf[64];
    swprintf_s(modeBuf, sizeof(modeBuf) / sizeof(modeBuf[0]), L"Mode: %s", (mode == Static) ? L"Static" : L"Dynamic");
    g.DrawString(modeBuf, -1, font, Gdiplus::PointF(40.0f, 170.0f), wTextBrush);

    wchar_t speedBuf[64];
    swprintf_s(speedBuf, sizeof(speedBuf) / sizeof(speedBuf[0]), L"Speed: %d", pAccelCount);
    g.DrawString(speedBuf, -1, font, Gdiplus::PointF(40.0f, 200.0f), wTextBrush);
}

void DrawSpeedGauge(Gdiplus::Graphics& g)
{
    int gaugeX = 200;
    int gaugeY = 200;
    int gaugeSize = 135;

    Gdiplus::Pen gaugeBgPen(Gdiplus::Color(255, 80, 80, 80), 8);
    g.DrawArc(&gaugeBgPen, gaugeX, gaugeY, gaugeSize, gaugeSize, 0, 360);

    float sweepAngle = (static_cast<float>(pAccelCount) / maxSpeed) * 360.0f;
    Gdiplus::Pen gaugePen(Gdiplus::Color(255, 0, 200, 0), 8);
    g.DrawArc(&gaugePen, gaugeX, gaugeY, gaugeSize, gaugeSize, -90, sweepAngle);

    wchar_t gaugeText[32];
    swprintf_s(gaugeText, 32, L"%d", pAccelCount);

    Gdiplus::Font gaugeFont(L"Segoe UI", 18, Gdiplus::FontStyleBold);
    Gdiplus::SolidBrush gaugeTextBrush(Gdiplus::Color(255, 255, 255, 255));

    Gdiplus::StringFormat format;
    format.SetAlignment(Gdiplus::StringAlignmentCenter);
    format.SetLineAlignment(Gdiplus::StringAlignmentCenter);

    Gdiplus::RectF textRect(
        gaugeX,
        gaugeY,
        static_cast<Gdiplus::REAL>(gaugeSize),
        static_cast<Gdiplus::REAL>(gaugeSize)
    );

    g.DrawString(gaugeText, -1, &gaugeFont, textRect, &format, &gaugeTextBrush);
}

void DrawSpeedHistoryGraph(Gdiplus::Graphics& g, int w, int h, Gdiplus::Font* font2, Gdiplus::SolidBrush* wTextBrush)
{
    int uiLeftMargin = 40;
    int labelAreaWidth = 120;
    int bgX = uiLeftMargin + labelAreaWidth;
    int bgWidth = 1000;
    int bgHeight = 200;
    int bgY = h - bgHeight - 40;

    float desiredBarWidth = 5.0f;
    float desiredBarSpacing = 7.5f;
    float availableWidth = static_cast<float>(bgWidth);

    float totalDesired = SPEED_HISTORY_SIZE * desiredBarWidth + (SPEED_HISTORY_SIZE - 1) * desiredBarSpacing;
    float barWidth = desiredBarWidth;
    float barSpacing = desiredBarSpacing;
    if (totalDesired > availableWidth) {
        float scale = availableWidth / totalDesired;
        barWidth *= scale;
        barSpacing *= scale;
    }

    float innerX = static_cast<float>(bgX);
    float innerWidth = static_cast<float>(bgWidth);
    float innerY = static_cast<float>(bgY);
    float innerHeight = static_cast<float>(bgHeight);

    Gdiplus::SolidBrush graphBgBrush2(Gdiplus::Color(255, 30, 30, 30));
    g.FillRectangle(&graphBgBrush2, innerX - 100, innerY - 75, innerWidth + 200, innerHeight + 150);

    Gdiplus::SolidBrush graphBgBrush(Gdiplus::Color(255, 240, 240, 240));
    g.FillRectangle(&graphBgBrush, innerX, innerY, innerWidth, innerHeight);

    Gdiplus::Pen gridPen(Gdiplus::Color(255, 180, 180, 180), 1.0f);
    int gridStep = 50;
    for (int s = 0; s <= maxSpeed; s += gridStep) {
        float y = innerY + innerHeight - (s * innerHeight / static_cast<float>(maxSpeed));
        Gdiplus::PointF p1(innerX, y);
        Gdiplus::PointF p2(innerX + innerWidth, y);
        g.DrawLine(&gridPen, p1, p2);

        wchar_t label[16];
        swprintf_s(label, 16, L"%d", s);
        g.DrawString(label, -1, font2, Gdiplus::PointF(innerX - 60.0f, y - 8.0f), wTextBrush);
    }

    Gdiplus::SolidBrush speedBarBrush(Gdiplus::Color(255, 0, 180, 0));
    Gdiplus::Pen speedBarOutline(Gdiplus::Color(255, 0, 120, 200), 1.0f);
    float maxRight = innerX + innerWidth;

    for (int i = 0; i < SPEED_HISTORY_SIZE; ++i) {
        int histIndex = (speedIndex + i) % SPEED_HISTORY_SIZE;
        float barX = innerX + i * (barWidth + barSpacing);
        float barH = (speedHistory[histIndex] * innerHeight) / static_cast<float>(maxSpeed);
        float barY = innerY + innerHeight - barH;

        float drawW = barWidth;
        if (barX + drawW > maxRight) drawW = maxRight - barX;
        if (drawW <= 0.0f) break;

        g.FillRectangle(&speedBarBrush, barX, barY, drawW, barH);
        g.DrawRectangle(&speedBarOutline, barX, barY, drawW, barH);
    }

    g.DrawString(L"Speed History", -1, g_pFont, Gdiplus::PointF(innerX, innerY - 48.0f), wTextBrush);
}

void DrawOdometer(Gdiplus::Graphics& g, Gdiplus::SolidBrush* wTextBrush)
{
    Gdiplus::SolidBrush odometerBrush(Gdiplus::Color(255, 0, 0, 0));
    Gdiplus::Font odometerFont(L"Courier New", 24);
    wchar_t odoBuf[64];
    swprintf_s(odoBuf, 64, L"%05d", pAccelCount);
    g.DrawString(L"Odometer", -1, g_pFont, Gdiplus::PointF(800.0f, 40.0f), wTextBrush);
    g.DrawString(odoBuf, -1, &odometerFont, Gdiplus::PointF(800.0f, 70.0f), &odometerBrush);
}

void DrawRawDataPanel(Gdiplus::Graphics& g)
{
    const int boxX = 1000;
    const int boxY = 10;
    const int padding = 12;
    const int lines = 8;
    const int lineHeight = 20;
    const int boxW = 260;
    const int boxH = padding * 2 + lines * lineHeight;

    Gdiplus::SolidBrush shadowBrush(Gdiplus::Color(80, 0, 0, 0));
    g.FillRectangle(&shadowBrush, static_cast<Gdiplus::REAL>(boxX + 4), static_cast<Gdiplus::REAL>(boxY + 4),
        static_cast<Gdiplus::REAL>(boxW), static_cast<Gdiplus::REAL>(boxH));

    Gdiplus::SolidBrush boxBrush(Gdiplus::Color(255, 30, 30, 30));
    Gdiplus::Pen boxPen(Gdiplus::Color(255, 80, 80, 80), 1.0f);
    g.FillRectangle(&boxBrush, static_cast<Gdiplus::REAL>(boxX), static_cast<Gdiplus::REAL>(boxY),
        static_cast<Gdiplus::REAL>(boxW), static_cast<Gdiplus::REAL>(boxH));
    g.DrawRectangle(&boxPen, boxX, boxY, boxW, boxH);

    Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(255, 255, 255, 255));
    Gdiplus::Font rawFont(L"Courier New", 14);

    int rx = boxX + padding;
    int ry = boxY + padding;
    for (int i = 0; i < lines; ++i) {
        wchar_t rawBuf[32];
        swprintf_s(rawBuf, 32, L"data[%d]: 0x%02X", i, rawData[i]);
        g.DrawString(rawBuf, -1, &rawFont, Gdiplus::PointF(static_cast<Gdiplus::REAL>(rx),
            static_cast<Gdiplus::REAL>(ry + i * lineHeight)), &whiteBrush);
    }
}

void ProcessLeftPedal(const BYTE* data)
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
                speedHistory[speedIndex] = pAccelCount;
                speedIndex = (speedIndex + 1) % SPEED_HISTORY_SIZE;
            }
        }
        else if (mode == Dynamic) {
            if (pAccelCount < 300) {
                pAccelCount = pAccelCount + (rightPedalPressure / 5);
            }
            if (rightPedalPressure < 2) {
                pAccelCount = pAccelCount + 1;
            }
            speedHistory[speedIndex] = pAccelCount;
            speedIndex = (speedIndex + 1) % SPEED_HISTORY_SIZE;
        }
    }
    else {
        rightPedalPressed = false;
        rpEdge = 0;
    }
}

void ProcessMiddlePedal(const BYTE* data)
{
    if (data[4] > 0) {
        middlePedalPressed = true;
        if (data[4] > 30 && mpDebounce.isReady()) {
            if (mode == Static) {
                mpEdge++;
                if (mpEdge < 2) {
                    pAccelCount -= 20;
                    speedHistory[speedIndex] = pAccelCount;
                    speedIndex = (speedIndex + 1) % SPEED_HISTORY_SIZE;
                }
            }
            else if (mode == Dynamic) {
                pAccelCount -= (middlePedalPressure * 2);
                speedHistory[speedIndex] = pAccelCount;
                speedIndex = (speedIndex + 1) % SPEED_HISTORY_SIZE;
            }
            mpDebounce.mark();
        }
    }
    else {
        middlePedalPressed = false;
        mpEdge = 0;
    }
}

void HandleWMPaint(HWND hwnd)
{
    int localSpeed = 0;
    int localIndex = 0;
    int localHistory[SPEED_HISTORY_SIZE];
    {
        std::lock_guard<std::mutex> lock(g_speedMutex);
        localSpeed = pAccelCount;
        localIndex = speedIndex;
        std::memcpy(localHistory, speedHistory, sizeof(speedHistory));
    }

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP hBmp = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(memDC, hBmp);

    Gdiplus::Graphics g(memDC);
    g.Clear(Gdiplus::Color(150, 100, 100, 100));

    Gdiplus::Font* font = g_pFont ? g_pFont : new Gdiplus::Font(L"Segoe UI", 16);
    Gdiplus::Font* font2 = g_pFont ? g_pFont : new Gdiplus::Font(L"Segoe UI", 10);
    Gdiplus::SolidBrush* wTextBrush = new Gdiplus::SolidBrush(Gdiplus::Color(255, 255, 255, 255));

    DrawPedalBars(g, font, wTextBrush);
    DrawModeAndSpeed(g, font, wTextBrush);
    DrawSpeedGauge(g);
    DrawSpeedHistoryGraph(g, w, h, font2, wTextBrush);
    DrawOdometer(g, wTextBrush);
    DrawRawDataPanel(g);

    BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, hOldBmp);
    DeleteObject(hBmp);
    DeleteDC(memDC);

    if (!g_pFont) delete font;
    if (!g_pBarBg) delete g_pBarBg;
    if (!g_pBarFill) delete g_pBarFill;
    if (!g_pTextBrush) delete g_pTextBrush;
    delete wTextBrush;

    EndPaint(hwnd, &ps);
}

void HandleWMInput(HWND hwnd, LPARAM lParam)
{
    UINT dwSize;
    GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
    BYTE* lpb = new BYTE[dwSize];
    if (lpb == NULL) return;

    if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) == dwSize) {
        RAWINPUT* raw = (RAWINPUT*)lpb;

        if (raw->header.dwType == RIM_TYPEHID) {
            BYTE* data = raw->data.hid.bRawData;
            UINT size = raw->data.hid.dwSizeHid;

            memset(rawData, 0, sizeof(rawData));
            memcpy(rawData, data, std::min(size, 8u));

            ProcessLeftPedal(data);
            ProcessRightPedal(data);
            ProcessMiddlePedal(data);

            rightPedalPressure = data[2];
            middlePedalPressure = data[4];

            InvalidateRect(hwnd, NULL, TRUE);

            xcp_update_variables(middlePedalPressure, rightPedalPressure, pAccelCount, (mode == Static) ? 0 : 1);

            for (UINT i = 0; i < size; i++) {
                wchar_t byteStr[16];
                swprintf(byteStr, 16, L"%d ", data[i]);
                OutputDebugString(byteStr);
            }
            OutputDebugString(L"\n");
        }
    }
    delete[] lpb;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        HandleWMCreate(hwnd);
        return 0;
    case WM_DESTROY:
        HandleWMDestroy();
        return 0;
    case WM_SPEED_UPDATE:
        InvalidateRect(hwnd, NULL, false);
        return 0;
    case WM_PAINT:
        HandleWMPaint(hwnd);
        return 0;
    case WM_INPUT:
        HandleWMInput(hwnd, lParam);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"Sample Window Class";

    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Fanatec Wizard",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, NULL
    );

    xcp_init();

    A2LGenerator a2l_gen;
    a2l_gen.add_variable("brake_raw", "Brake Pedal Raw Value", "UBYTE");
    a2l_gen.add_variable("throttle_raw", "Throttle Pedal Raw Value", "UBYTE");
    a2l_gen.add_variable("vehicle_speed", "Vehicle Speed", "UWORD");
    a2l_gen.add_variable("drive_mode", "Drive Mode", "UBYTE");
    a2l_gen.generate("fanatec_pedals.a2l");

    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x04;
    rid.dwFlags = RIDEV_INPUTSINK;
    rid.hwndTarget = hwnd;
    RegisterRawInputDevices(&rid, 1, sizeof(rid));

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return 0;
}