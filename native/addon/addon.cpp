#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <napi.h>
#include <cstdint>
#include <cstring>
#include "../shared/shared_memory.h"

static HANDLE g_hMapFile = nullptr;
static HANDLE g_hFrameEvent = nullptr;
static HANDLE g_hMutex = nullptr;
static void* g_pSharedMem = nullptr;
static bool g_initialized = false;

Napi::Value Init(const Napi::CallbackInfo& info) {
    if (g_initialized) return Napi::Boolean::New(info.Env(), true);

    g_hMutex = CreateMutexA(nullptr, FALSE, FREEZECAM_MUTEX_NAME);
    if (!g_hMutex) {
        Napi::Error::New(info.Env(), "Failed to create mutex").ThrowAsJavaScriptException();
        return info.Env().Undefined();
    }

    g_hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
        0, FREEZECAM_SHM_SIZE, FREEZECAM_SHM_NAME);
    if (!g_hMapFile) {
        CloseHandle(g_hMutex);
        g_hMutex = nullptr;
        Napi::Error::New(info.Env(), "Failed to create shared memory").ThrowAsJavaScriptException();
        return info.Env().Undefined();
    }

    g_pSharedMem = MapViewOfFile(g_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, FREEZECAM_SHM_SIZE);
    if (!g_pSharedMem) {
        CloseHandle(g_hMapFile);
        CloseHandle(g_hMutex);
        g_hMapFile = nullptr;
        g_hMutex = nullptr;
        Napi::Error::New(info.Env(), "Failed to map shared memory").ThrowAsJavaScriptException();
        return info.Env().Undefined();
    }

    g_hFrameEvent = CreateEventA(nullptr, FALSE, FALSE, FREEZECAM_EVENT_NAME);
    if (!g_hFrameEvent) {
        UnmapViewOfFile(g_pSharedMem);
        CloseHandle(g_hMapFile);
        CloseHandle(g_hMutex);
        g_pSharedMem = nullptr;
        g_hMapFile = nullptr;
        g_hMutex = nullptr;
        Napi::Error::New(info.Env(), "Failed to create event").ThrowAsJavaScriptException();
        return info.Env().Undefined();
    }

    ZeroMemory(g_pSharedMem, FREEZECAM_SHM_SIZE);
    g_initialized = true;
    return Napi::Boolean::New(info.Env(), true);
}

Napi::Value SendFrame(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (!g_initialized || !g_pSharedMem) {
        return Napi::Boolean::New(env, false);
    }

    if (info.Length() < 3) {
        Napi::TypeError::New(env, "Expected (buffer, width, height)").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    Napi::Buffer<uint8_t> buffer = info[0].As<Napi::Buffer<uint8_t>>();
    uint32_t width = info[1].As<Napi::Number>().Uint32Value();
    uint32_t height = info[2].As<Napi::Number>().Uint32Value();

    if (width == 0 || height == 0 || width > FREEZECAM_MAX_WIDTH || height > FREEZECAM_MAX_HEIGHT) {
        return Napi::Boolean::New(env, false);
    }

    uint32_t frameSize = width * height * 4; // RGBA
    if (buffer.Length() < frameSize) {
        return Napi::Boolean::New(env, false);
    }

    if (frameSize > FREEZECAM_MAX_FRAME_SIZE) {
        return Napi::Boolean::New(env, false);
    }

    WaitForSingleObject(g_hMutex, 16);

    auto* header = static_cast<FreezeCamFrameHeader*>(g_pSharedMem);
    header->width = width;
    header->height = height;
    header->stride = width * 4;
    header->format = 0; // RGBA
    header->frameNumber++;
    header->isActive = 1;

    uint8_t* dst = static_cast<uint8_t*>(g_pSharedMem) + sizeof(FreezeCamFrameHeader);
    memcpy(dst, buffer.Data(), frameSize);

    ReleaseMutex(g_hMutex);
    SetEvent(g_hFrameEvent);

    return Napi::Boolean::New(env, true);
}

Napi::Value Shutdown(const Napi::CallbackInfo& info) {
    if (!g_initialized) return Napi::Boolean::New(info.Env(), true);

    if (g_pSharedMem) {
        auto* header = static_cast<FreezeCamFrameHeader*>(g_pSharedMem);
        header->isActive = 0;
    }

    if (g_pSharedMem) { UnmapViewOfFile(g_pSharedMem); g_pSharedMem = nullptr; }
    if (g_hFrameEvent) { CloseHandle(g_hFrameEvent); g_hFrameEvent = nullptr; }
    if (g_hMapFile) { CloseHandle(g_hMapFile); g_hMapFile = nullptr; }
    if (g_hMutex) { CloseHandle(g_hMutex); g_hMutex = nullptr; }

    g_initialized = false;
    return Napi::Boolean::New(info.Env(), true);
}

Napi::Object InitModule(Napi::Env env, Napi::Object exports) {
    exports.Set("init", Napi::Function::New(env, Init));
    exports.Set("sendFrame", Napi::Function::New(env, SendFrame));
    exports.Set("shutdown", Napi::Function::New(env, Shutdown));
    return exports;
}

NODE_API_MODULE(freezecam_addon, InitModule)
