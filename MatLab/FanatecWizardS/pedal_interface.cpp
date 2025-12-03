#define S_FUNCTION_NAME pedal_interface
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"
#include <windows.h>
#include <mutex>
#include <algorithm>
#include <cstring>

class FanatecPedals {
private:
    std::mutex dataMutex;
    HWND hwnd;
    
    // Your pedal variables
    int pAccelCount{0};
    bool leftPedalPressed{false};
    bool rightPedalPressed{false};
    bool middlePedalPressed{false};
    int middlePedalPressure{0};
    int rightPedalPressure{0};
    int driveMode{0};
    int rpEdge{0};
    int mpEdge{0};
    BYTE rawData[8] = {0};

    // Static instance pointer for window procedure
    static FanatecPedals* instance;

public:
    FanatecPedals() : hwnd(nullptr) {
        mexPrintf("=== FanatecPedals constructor called ===\n");
        instance = this;
    }
    
    ~FanatecPedals() {
        mexPrintf("=== FanatecPedals destructor called ===\n");
        if (hwnd) {
            DestroyWindow(hwnd);
            mexPrintf("Message window destroyed\n");
        }
        instance = nullptr;
    }
    
    bool initialize() {
        mexPrintf("=== FanatecPedals::initialize() called ===\n");
        
        // Create message window for Raw Input
        WNDCLASS wc = {};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = "FanatecHIDWindow";
        
        mexPrintf(">>> Registering window class...\n");
        if (!RegisterClass(&wc)) {
            DWORD error = GetLastError();
            mexPrintf("!!! ERROR: Failed to register window class (Error: %lu)\n", error);
            
            if (error == ERROR_CLASS_ALREADY_EXISTS) {
                mexPrintf("!!! Window class already exists - this might be OK\n");
            } else {
                return false;
            }
        } else {
            mexPrintf(">>> Window class registered successfully\n");
        }
        
        mexPrintf(">>> Creating message-only window...\n");
        hwnd = CreateWindow(wc.lpszClassName, "Fanatec HID Window", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, wc.hInstance, NULL);
        
        if (!hwnd) {
            DWORD error = GetLastError();
            mexPrintf("!!! ERROR: Failed to create message window (Error: %lu)\n", error);
            return false;
        }
        mexPrintf(">>> Message window created successfully (HWND: %p)\n", hwnd);
        
        // Register for raw input - try multiple HID device types
        RAWINPUTDEVICE rid[3];
        
        // Try Joystick
        rid[0].usUsagePage = 0x01;
        rid[0].usUsage = 0x04;  // Joystick
        rid[0].dwFlags = RIDEV_INPUTSINK;
        rid[0].hwndTarget = hwnd;
        
        // Try Game Pad
        rid[1].usUsagePage = 0x01;
        rid[1].usUsage = 0x05;  // Game Pad
        rid[1].dwFlags = RIDEV_INPUTSINK;
        rid[1].hwndTarget = hwnd;
        
        // Try Generic Desktop Controls
        rid[2].usUsagePage = 0x01;
        rid[2].usUsage = 0x00;  // Wildcard - any generic desktop device
        rid[2].dwFlags = RIDEV_INPUTSINK;
        rid[2].hwndTarget = hwnd;
        
        
        bool registrationSuccess = false;
        int successCount = 0;
        
        // Try registering each device type
        for (int i = 0; i < 3; i++) {
            if (RegisterRawInputDevices(&rid[i], 1, sizeof(RAWINPUTDEVICE))) {
                mexPrintf(">>> SUCCESS: Registered device type %d\n", i);
                successCount++;
                registrationSuccess = true;
            } else {
                DWORD error = GetLastError();
                mexPrintf("!!! Failed to register device type %d (Error: %lu)\n", i, error);
                
                // Print common error meanings
                switch (error) {
                    case ERROR_ACCESS_DENIED:
                        mexPrintf("!!!   Access denied - may need administrator privileges\n");
                        break;
                    case ERROR_INVALID_PARAMETER:
                        mexPrintf("!!!   Invalid parameter - bad usage page/usage values\n");
                        break;
                    case ERROR_NOT_FOUND:
                        mexPrintf("!!!   Device not found - no HID devices of this type\n");
                        break;
                    default:
                        mexPrintf("!!!   Unknown error\n");
                        break;
                }
            }
        }
        
        if (!registrationSuccess) {
            mexPrintf("!!! CRITICAL ERROR: Failed to register ANY HID device types\n");
            mexPrintf("!!! The pedals may not be detected or may need different HID codes\n");
            DestroyWindow(hwnd);
            hwnd = nullptr;
            return false;
        }
        
        mexPrintf(">>> Successfully registered %d HID device type(s)\n", successCount);
        mexPrintf(">>> FanatecPedals initialization COMPLETE - Ready for HID data!\n");
        mexPrintf(">>> Listening window HWND: %p\n", hwnd);
        
        return true;
    }
    
    void update() {
        // Process any pending Windows messages
        MSG msg;
        int messageCount = 0;
        while (PeekMessage(&msg, hwnd, 0, 0, PM_REMOVE)) {
            messageCount++;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        static int updateCount = 0;
        updateCount++;
        
        if (updateCount % 100 == 0) {  // Print every 100 updates (1 second)
            mexPrintf("--- Update #%d - Processed %d messages\n", updateCount, messageCount);
        }
    }
    
void getData(double* outputs) {
    std::lock_guard<std::mutex> lock(dataMutex);
    
    outputs[0] = static_cast<double>(pAccelCount) / 300.0;
    outputs[1] = static_cast<double>(driveMode);
    outputs[2] = static_cast<double>(rightPedalPressure) / 255.0;
    outputs[3] = static_cast<double>(middlePedalPressure) / 255.0;
    outputs[4] = leftPedalPressed ? 1.0 : 0.0;
    
    // Debug the actual output values
    static int getDataCount = 0;
    getDataCount++;
    
    if (getDataCount % 20 == 0) {
        mexPrintf(">>> GETDATA OUTPUTS - Speed:%.1fkm/h, Mode:%d, Throttle:%d/255, Brake:%d/255, Clutch:%d\n",
                 outputs[0] * 300, (int)outputs[1], rightPedalPressure, middlePedalPressure, leftPedalPressed);
    }
}

private:
    void processRawInput(LPARAM lParam) {
        mexPrintf(">>> WM_INPUT received! Processing raw input...\n");
        
        UINT dwSize;
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
        
        mexPrintf(">>> Raw input data size: %d bytes\n", dwSize);
        
        if (dwSize == 0) {
            mexPrintf("!!! ERROR: Raw input data size is 0\n");
            return;
        }
        
        BYTE* lpb = new BYTE[dwSize];
        if (!lpb) {
            mexPrintf("!!! ERROR: Failed to allocate buffer for HID data\n");
            return;
        }
        
        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) == dwSize) {
            RAWINPUT* raw = (RAWINPUT*)lpb;
            
            if (raw->header.dwType == RIM_TYPEHID) {
                mexPrintf(">>> HID Data detected! Size: %d\n", raw->data.hid.dwSizeHid);
                
                BYTE* data = raw->data.hid.bRawData;
                UINT size = raw->data.hid.dwSizeHid;
                
                // Print first 8 bytes for debugging
                mexPrintf(">>> HID Bytes: ");
                for (UINT i = 0; i < std::min(size, 8u); i++) {
                    mexPrintf("%02X ", data[i]);
                }
                mexPrintf("\n");
                
                std::lock_guard<std::mutex> lock(dataMutex);
                
                memset(rawData, 0, sizeof(rawData));
                memcpy(rawData, data, std::min(size, 8u));
                processPedalData(data);
            } else {
                mexPrintf("!!! Not HID data, type: %d\n", raw->header.dwType);
            }
        } else {
            mexPrintf("!!! ERROR: Failed to get raw input data\n");
        }
        delete[] lpb;
    }
    
    void processPedalData(const BYTE* data) {
        mexPrintf(">>> Processing pedal data - Throttle[2]: %d, Brake[4]: %d, Clutch[6]: %d\n", 
                 data[2], data[4], data[6]);
        
        static DWORD lastModeChange = 0;
        static DWORD lastBrakeTime = 0;
        DWORD currentTime = GetTickCount();
        
        // LEFT PEDAL LOGIC
        if (data[6] > 0) {
            mexPrintf(">>> Clutch pressed: %d\n", data[6]);
            leftPedalPressed = true;
            if (data[6] > 14) {
                if (currentTime - lastModeChange > 1000) {
                    mexPrintf(">>> Mode change triggered!\n");
                    if (driveMode == 0) {
                        driveMode = 1;
                        mexPrintf(">>> Switching to DYNAMIC mode\n");
                    } else {
                        driveMode = 0;
                        pAccelCount = 0;
                        mexPrintf(">>> Switching to STATIC mode, speed reset to 0\n");
                    }
                    lastModeChange = currentTime;
                }
            }
        } else {
            leftPedalPressed = false;
        }
        
        // RIGHT PEDAL LOGIC
        if (data[2] > 0) {
            mexPrintf(">>> Throttle pressed: %d\n", data[2]);
            rightPedalPressed = true;
            rightPedalPressure = data[2];
            
            if (driveMode == 0) {
                if (data[2] > 13) {
                    rpEdge++;
                    if (rpEdge < 2) {
                        pAccelCount += 20;
                        if (pAccelCount > 300) pAccelCount = 300;
                    }
                }
            } else {
                if (pAccelCount < 300) {
                    pAccelCount = pAccelCount + (rightPedalPressure / 5);
                    if (pAccelCount > 300) pAccelCount = 300;
                }
                if (rightPedalPressure < 2) {
                    pAccelCount += 1;
                    if (pAccelCount > 300) pAccelCount = 300;
                }
            }
        } else {
            rightPedalPressed = false;
            rpEdge = 0;
        }
        
        // MIDDLE PEDAL LOGIC
        if (data[4] > 0) {
            mexPrintf(">>> Brake pressed: %d\n", data[4]);
            middlePedalPressed = true;
            middlePedalPressure = data[4];
            
            if (data[4] > 30) {
                if (currentTime - lastBrakeTime > 500) {
                    if (driveMode == 0) {
                        mpEdge++;
                        if (mpEdge < 2) {
                            pAccelCount -= 20;
                            if (pAccelCount < 0) pAccelCount = 0;
                        }
                    } else {
                        pAccelCount -= (middlePedalPressure * 2);
                        if (pAccelCount < 0) pAccelCount = 0;
                    }
                    lastBrakeTime = currentTime;
                }
            }
        } else {
            middlePedalPressed = false;
            mpEdge = 0;
        }
        
        // Speed decay in dynamic mode
        if (driveMode == 1) {
            static int decayCounter = 0;
            if (++decayCounter >= 10) {
                pAccelCount--;
                if (pAccelCount < 0) pAccelCount = 0;
                decayCounter = 0;
            }
        }
        
        mexPrintf(">>> Final state - Speed: %d, Mode: %d\n", pAccelCount, driveMode);
    }

    // Static window procedure
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (uMsg == WM_INPUT && instance) {
            mexPrintf(">>> WindowProc: WM_INPUT message received\n");
            instance->processRawInput(lParam);
            return 0;
        }
        
        if (uMsg == WM_INPUT && !instance) {
            mexPrintf("!!! WindowProc: WM_INPUT received but no instance!\n");
        }
        
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
};

// Initialize static member
FanatecPedals* FanatecPedals::instance = nullptr;

// S-function interface
static FanatecPedals* pedalSystem = nullptr;

static void mdlInitializeSizes(SimStruct *S) {
    mexPrintf("=== mdlInitializeSizes called ===\n");
    ssSetNumSFcnParams(S, 0);
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        mexPrintf("!!! Parameter count mismatch\n");
        return;
    }
    
    ssSetNumContStates(S, 0);
    ssSetNumDiscStates(S, 0);
    
    if (!ssSetNumInputPorts(S, 0)) {
        mexPrintf("!!! Failed to set 0 input ports\n");
        return;
    }
    
    if (!ssSetNumOutputPorts(S, 5)) {
        mexPrintf("!!! Failed to set 5 output ports\n");
        return;
    }
    
    ssSetOutputPortWidth(S, 0, 1);
    ssSetOutputPortWidth(S, 1, 1);
    ssSetOutputPortWidth(S, 2, 1);
    ssSetOutputPortWidth(S, 3, 1);
    ssSetOutputPortWidth(S, 4, 1);
    
    ssSetNumSampleTimes(S, 1);
    ssSetNumRWork(S, 0);
    ssSetNumIWork(S, 0);
    ssSetNumPWork(S, 1);
    ssSetNumModes(S, 0);
    ssSetNumNonsampledZCs(S, 0);
    
    ssSetSimStateCompliance(S, USE_DEFAULT_SIM_STATE);
    ssSetOptions(S, SS_OPTION_EXCEPTION_FREE_CODE);
    
    mexPrintf("=== S-function configured with 5 output ports ===\n");
}

static void mdlInitializeSampleTimes(SimStruct *S) {
    mexPrintf("=== mdlInitializeSampleTimes called - Setting 100Hz (0.01s)\n");
    ssSetSampleTime(S, 0, 0.01);
    ssSetOffsetTime(S, 0, 0.0);
}

#define MDL_START
static void mdlStart(SimStruct *S) {
    mexPrintf("🔥 mdlStart CALLED - Creating FanatecPedals instance ===\n");
    pedalSystem = new FanatecPedals();
    void **PWork = ssGetPWork(S);
    PWork[0] = pedalSystem;
    
    mexPrintf(">>> Calling initialize...\n");
    bool success = pedalSystem->initialize();
    mexPrintf(">>> Initialize result: %s\n", success ? "SUCCESS" : "FAILED");
}

static void mdlOutputs(SimStruct *S, int_T tid) {
    static int outputCount = 0;
    outputCount++;
    
    // Reduced debug spam
    if (outputCount % 50 == 0) {
        mexPrintf("--- mdlOutputs #%d\n", outputCount);
    }
    
    FanatecPedals* system = static_cast<FanatecPedals*>(ssGetPWork(S)[0]);
    
    if (system) {
        system->update();  // Process HID messages
        
        double outputs[5];
        system->getData(outputs);
        
        real_T *y0 = ssGetOutputPortRealSignal(S, 0);
        real_T *y1 = ssGetOutputPortRealSignal(S, 1);
        real_T *y2 = ssGetOutputPortRealSignal(S, 2);
        real_T *y3 = ssGetOutputPortRealSignal(S, 3);
        real_T *y4 = ssGetOutputPortRealSignal(S, 4);
        
        // ✅ CRITICAL FIX: Use REAL data, not forced values
        y0[0] = outputs[0];  // Speed (from your pedal logic)
        y1[0] = outputs[1];  // Mode  
        y2[0] = outputs[2];  // Throttle
        y3[0] = outputs[3];  // Brake
        y4[0] = outputs[4];  // Clutch
        
        // Debug what we're actually sending
        if (outputCount % 50 == 0) {
            mexPrintf(">>> SCOPE OUTPUTS - y0:%.3f, y1:%.0f, y2:%.3f, y3:%.3f, y4:%.0f\n",
                     y0[0], y1[0], y2[0], y3[0], y4[0]);
        }
    } else {
        // Zero outputs if no system
        real_T *y0 = ssGetOutputPortRealSignal(S, 0);
        real_T *y1 = ssGetOutputPortRealSignal(S, 1);
        real_T *y2 = ssGetOutputPortRealSignal(S, 2);
        real_T *y3 = ssGetOutputPortRealSignal(S, 3);
        real_T *y4 = ssGetOutputPortRealSignal(S, 4);
        
        if (y0) y0[0] = 0.0;
        if (y1) y1[0] = 0.0;
        if (y2) y2[0] = 0.0;
        if (y3) y3[0] = 0.0;
        if (y4) y4[0] = 0.0;
    }
}

static void mdlTerminate(SimStruct *S) {
    mexPrintf("=== mdlTerminate called - Cleaning up ===\n");
    FanatecPedals* system = static_cast<FanatecPedals*>(ssGetPWork(S)[0]);
    if (system) {
        mexPrintf(">>> Deleting FanatecPedals instance\n");
        delete system;
        pedalSystem = nullptr;
        mexPrintf(">>> Cleanup complete\n");
    } else {
        mexPrintf("!!! No system instance to delete\n");
    }
}

#ifdef MATLAB_MEX_FILE
#include "simulink.c"
#else
#include "cg_sfun.h"
#endif