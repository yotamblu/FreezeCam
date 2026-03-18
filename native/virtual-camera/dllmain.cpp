#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dshow.h>
#include <olectl.h>
#include <initguid.h>
#include <atomic>

#include "guids.h"
#include "class_factory.h"

std::atomic<LONG> g_dllRefCount(0);
HMODULE g_hModule = nullptr;

static const wchar_t* FILTER_NAME = L"FreezeCam Pro Virtual Camera";
static const wchar_t* VENDOR_NAME = L"FreezeCam Pro";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    if (!ppv) return E_POINTER;
    if (rclsid != CLSID_FreezeCamVirtualCamera)
        return CLASS_E_CLASSNOTAVAILABLE;

    auto* factory = new ClassFactory();
    if (!factory) return E_OUTOFMEMORY;

    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow() {
    return (g_dllRefCount == 0) ? S_OK : S_FALSE;
}

static HRESULT RegisterFilter(BOOL bRegister) {
    HRESULT hr;
    IFilterMapper2* pFM2 = nullptr;

    hr = CoCreateInstance(CLSID_FilterMapper2, nullptr, CLSCTX_INPROC_SERVER,
        IID_IFilterMapper2, (void**)&pFM2);
    if (FAILED(hr)) return hr;

    if (bRegister) {
        // Register the filter
        REGFILTER2 rf2;
        rf2.dwVersion = 1;
        rf2.dwMerit = MERIT_DO_NOT_USE;
        rf2.cPins = 1;

        REGFILTERPINS rfp;
        ZeroMemory(&rfp, sizeof(rfp));
        rfp.strName = const_cast<LPWSTR>(L"Output");
        rfp.bRendered = FALSE;
        rfp.bOutput = TRUE;
        rfp.bZero = FALSE;
        rfp.bMany = FALSE;
        rfp.nMediaTypes = 1;

        REGPINTYPES rpt;
        rpt.clsMajorType = &MEDIATYPE_Video;
        rpt.clsMinorType = &MEDIASUBTYPE_RGB24;
        rfp.lpMediaType = &rpt;

        rf2.rgPins = &rfp;

        hr = pFM2->RegisterFilter(
            CLSID_FreezeCamVirtualCamera,
            FILTER_NAME,
            nullptr,
            &CLSID_VideoInputDeviceCategory,
            FILTER_NAME,
            &rf2);
    } else {
        hr = pFM2->UnregisterFilter(
            &CLSID_VideoInputDeviceCategory,
            FILTER_NAME,
            CLSID_FreezeCamVirtualCamera);
    }

    pFM2->Release();
    return hr;
}

STDAPI DllRegisterServer() {
    // Register COM class
    WCHAR dllPath[MAX_PATH];
    GetModuleFileNameW(g_hModule, dllPath, MAX_PATH);

    WCHAR clsidStr[64];
    StringFromGUID2(CLSID_FreezeCamVirtualCamera, clsidStr, 64);

    // HKCR\CLSID\{...}
    WCHAR keyPath[256];
    swprintf_s(keyPath, L"CLSID\\%s", clsidStr);

    HKEY hKey = nullptr;
    RegCreateKeyExW(HKEY_CLASSES_ROOT, keyPath, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr);
    if (hKey) {
        RegSetValueExW(hKey, nullptr, 0, REG_SZ, (BYTE*)FILTER_NAME,
            (DWORD)(wcslen(FILTER_NAME) + 1) * sizeof(WCHAR));
        RegCloseKey(hKey);
    }

    // InprocServer32
    WCHAR inprocPath[256];
    swprintf_s(inprocPath, L"%s\\InprocServer32", keyPath);
    RegCreateKeyExW(HKEY_CLASSES_ROOT, inprocPath, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr);
    if (hKey) {
        RegSetValueExW(hKey, nullptr, 0, REG_SZ, (BYTE*)dllPath,
            (DWORD)(wcslen(dllPath) + 1) * sizeof(WCHAR));
        const WCHAR* threadingModel = L"Both";
        RegSetValueExW(hKey, L"ThreadingModel", 0, REG_SZ, (BYTE*)threadingModel,
            (DWORD)(wcslen(threadingModel) + 1) * sizeof(WCHAR));
        RegCloseKey(hKey);
    }

    // Register with DirectShow filter mapper
    CoInitialize(nullptr);
    HRESULT hr = RegisterFilter(TRUE);
    CoUninitialize();

    return hr;
}

STDAPI DllUnregisterServer() {
    CoInitialize(nullptr);
    RegisterFilter(FALSE);
    CoUninitialize();

    WCHAR clsidStr[64];
    StringFromGUID2(CLSID_FreezeCamVirtualCamera, clsidStr, 64);

    WCHAR keyPath[256];
    swprintf_s(keyPath, L"CLSID\\%s", clsidStr);

    // Delete InprocServer32 first, then the CLSID key
    WCHAR inprocPath[256];
    swprintf_s(inprocPath, L"%s\\InprocServer32", keyPath);
    RegDeleteKeyW(HKEY_CLASSES_ROOT, inprocPath);
    RegDeleteKeyW(HKEY_CLASSES_ROOT, keyPath);

    return S_OK;
}
