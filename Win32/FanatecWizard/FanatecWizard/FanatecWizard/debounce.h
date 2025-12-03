// debounce.h
#pragma once
#include <windows.h>

class Debounce {
public:
    Debounce(int debounceMs = 60);
    void mark();              // call this when the input changes
    bool isReady();           // returns true if debounce time has passed
    void setDebounce(int ms); // optional: change debounce time

private:
    DWORD lastChangeTime = 0;
    int debounceMs;
};