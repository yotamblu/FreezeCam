#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <ctime>

#define FC_LOG(fmt, ...) FreezeCamLog(__FUNCTION__, fmt, ##__VA_ARGS__)

inline void FreezeCamLog(const char* func, const char* fmt, ...) {
    static FILE* logFile = nullptr;
    static CRITICAL_SECTION cs;
    static bool csInit = false;

    if (!csInit) {
        InitializeCriticalSection(&cs);
        csInit = true;
    }

    EnterCriticalSection(&cs);

    if (!logFile) {
        logFile = fopen("C:\\ProgramData\\FreezeCam_virtual_camera.log", "a");
        if (!logFile) logFile = fopen("C:\\FreezeCam\\virtual_camera.log", "a");
        if (logFile) {
            fprintf(logFile, "\n========== FreezeCam Virtual Camera DLL loaded (PID %lu) ==========\n", GetCurrentProcessId());
            fflush(logFile);
        }
    }

    if (logFile) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(logFile, "[%02d:%02d:%02d.%03d][%s] ",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, func);

        va_list args;
        va_start(args, fmt);
        vfprintf(logFile, fmt, args);
        va_end(args);

        fprintf(logFile, "\n");
        fflush(logFile);
    }

    LeaveCriticalSection(&cs);
}
