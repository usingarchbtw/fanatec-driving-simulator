// debounce.cpp
#include "debounce.h"

Debounce::Debounce(int debounceMs) {
    this->debounceMs = debounceMs;
}

void Debounce::mark() {
    lastChangeTime = GetTickCount();
}

bool Debounce::isReady() {
    DWORD now = GetTickCount();
    return (now - lastChangeTime) >= debounceMs;
}

void Debounce::setDebounce(int ms) {
    debounceMs = ms;
}