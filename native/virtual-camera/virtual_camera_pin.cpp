#include "virtual_camera_pin.h"
#include "guids.h"
#include "logger.h"
#include "../shared/shared_memory.h"
#include <cstring>
#include <cmath>

// User-mode DirectShow pin property set (NOT the kernel-mode KSPROPSETID_Pin!)
static const GUID AMPROPSETID_Pin_UM =
    { 0x9b00f101, 0x1567, 0x11d1, { 0xb3, 0xf1, 0x00, 0xaa, 0x00, 0x37, 0x61, 0xc5 } };
// Kernel-mode version (some apps use this instead)
static const GUID KSPROPSETID_Pin_KM =
    { 0x8C134960, 0x51AD, 0x11CF, { 0x87, 0x8A, 0x94, 0xF8, 0x01, 0xC1, 0x00, 0x00 } };

static void FreeMediaType(AM_MEDIA_TYPE& mt) {
    if (mt.cbFormat > 0 && mt.pbFormat) {
        CoTaskMemFree(mt.pbFormat);
        mt.pbFormat = nullptr;
        mt.cbFormat = 0;
    }
    if (mt.pUnk) {
        mt.pUnk->Release();
        mt.pUnk = nullptr;
    }
}

static void CopyMediaType(AM_MEDIA_TYPE* dst, const AM_MEDIA_TYPE* src) {
    *dst = *src;
    if (src->cbFormat > 0 && src->pbFormat) {
        dst->pbFormat = (BYTE*)CoTaskMemAlloc(src->cbFormat);
        if (dst->pbFormat) {
            memcpy(dst->pbFormat, src->pbFormat, src->cbFormat);
        }
    }
    if (src->pUnk) {
        dst->pUnk = src->pUnk;
        dst->pUnk->AddRef();
    }
}

static const char* GuidToSubtype(const GUID& g) {
    if (g == MEDIASUBTYPE_YUY2) return "YUY2";
    if (g == MEDIASUBTYPE_RGB24) return "RGB24";
    if (g == MEDIASUBTYPE_RGB32) return "RGB32";
    if (g == MEDIASUBTYPE_NV12) return "NV12";
    if (g == MEDIASUBTYPE_UYVY) return "UYVY";
    if (g == MEDIASUBTYPE_MJPG) return "MJPG";
    // I420 GUID: 30323449-0000-0010-8000-00AA00389B71
    static const GUID MY_MEDIASUBTYPE_I420 =
        { 0x30323449, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };
    if (g == MY_MEDIASUBTYPE_I420) return "I420";
    return "UNKNOWN";
}

VirtualCameraPin::VirtualCameraPin(VirtualCameraFilter* pFilter, HRESULT* phr)
    : m_refCount(1)
    , m_pFilter(pFilter)
    , m_pConnectedPin(nullptr)
    , m_pInputPin(nullptr)
    , m_pAllocator(nullptr)
    , m_connected(false)
    , m_preferredType(0)
    , m_streamThread(nullptr)
    , m_streaming(false)
    , m_hMapFile(nullptr)
    , m_hFrameEvent(nullptr)
    , m_hMutex(nullptr)
    , m_pSharedMem(nullptr) {
    InitializeCriticalSection(&m_cs);
    ZeroMemory(&m_mediaType, sizeof(m_mediaType));
    CEnumMediaTypes::GetMediaType(0, &m_mediaType);
    FC_LOG("Pin constructed, default type: %s", GuidToSubtype(m_mediaType.subtype));
    *phr = S_OK;
}

VirtualCameraPin::~VirtualCameraPin() {
    FC_LOG("Pin destructor");
    StopStreaming();
    FreeMediaType(m_mediaType);
    if (m_pConnectedPin) m_pConnectedPin->Release();
    if (m_pInputPin) m_pInputPin->Release();
    if (m_pAllocator) m_pAllocator->Release();
    DeleteCriticalSection(&m_cs);
}

static void LogPinGuid(const char* context, REFIID riid, bool found) {
    WCHAR guidStr[64];
    StringFromGUID2(riid, guidStr, 64);
    char narrow[64];
    WideCharToMultiByte(CP_UTF8, 0, guidStr, -1, narrow, 64, nullptr, nullptr);
    FC_LOG("QI %s: %s -> %s", context, narrow, found ? "OK" : "E_NOINTERFACE");
}

STDMETHODIMP VirtualCameraPin::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown) *ppv = static_cast<IPin*>(this);
    else if (riid == IID_IPin) *ppv = static_cast<IPin*>(this);
    else if (riid == IID_IKsPropertySet) *ppv = static_cast<IKsPropertySet*>(this);
    else if (riid == IID_IAMStreamConfig) *ppv = static_cast<IAMStreamConfig*>(this);
    else if (riid == IID_IQualityControl) *ppv = static_cast<IQualityControl*>(this);
    else {
        *ppv = nullptr;
        LogPinGuid("Pin", riid, false);
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) VirtualCameraPin::AddRef() { return ++m_refCount; }
STDMETHODIMP_(ULONG) VirtualCameraPin::Release() {
    LONG ref = --m_refCount;
    if (ref == 0) delete this;
    return ref;
}

// ─── IPin ───

STDMETHODIMP VirtualCameraPin::Connect(IPin* pReceivePin, const AM_MEDIA_TYPE* pmt) {
    if (!pReceivePin) return E_POINTER;
    if (m_connected) {
        FC_LOG("Connect: ALREADY CONNECTED");
        return VFW_E_ALREADY_CONNECTED;
    }
    if (m_pFilter->GetFilterState() != State_Stopped) {
        FC_LOG("Connect: NOT STOPPED");
        return VFW_E_NOT_STOPPED;
    }

    FC_LOG("Connect() called, pmt=%p", pmt);
    if (pmt && pmt->majortype != GUID_NULL) {
        FC_LOG("Connect: caller suggests type %s, %dx?", GuidToSubtype(pmt->subtype),
            pmt->pbFormat ? ((VIDEOINFOHEADER*)pmt->pbFormat)->bmiHeader.biWidth : 0);
    }

    auto tryConnect = [&](const AM_MEDIA_TYPE& mt, const char* label) -> HRESULT {
        VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)mt.pbFormat;
        FC_LOG("Trying %s: %s %dx%d sampleSize=%lu",
            label, GuidToSubtype(mt.subtype),
            vih ? vih->bmiHeader.biWidth : 0,
            vih ? abs(vih->bmiHeader.biHeight) : 0,
            mt.lSampleSize);

        HRESULT hr = pReceivePin->ReceiveConnection(this, &mt);
        FC_LOG("  ReceiveConnection returned 0x%08lx (%s)", hr, SUCCEEDED(hr) ? "OK" : "FAILED");
        if (FAILED(hr)) return hr;

        // Connection accepted — set up
        FreeMediaType(m_mediaType);
        CopyMediaType(&m_mediaType, &mt);
        m_pConnectedPin = pReceivePin;
        m_pConnectedPin->AddRef();

        hr = pReceivePin->QueryInterface(IID_IMemInputPin, (void**)&m_pInputPin);
        FC_LOG("  QueryInterface IMemInputPin: 0x%08lx, ptr=%p", hr, m_pInputPin);

        m_connected = true;

        if (m_pInputPin) {
            ALLOCATOR_PROPERTIES requested, actual;
            ZeroMemory(&requested, sizeof(requested));
            requested.cBuffers = 2;
            requested.cbBuffer = m_mediaType.lSampleSize;
            requested.cbAlign = 1;
            requested.cbPrefix = 0;

            FC_LOG("  Requesting allocator: cBuffers=%ld cbBuffer=%ld", requested.cBuffers, requested.cbBuffer);

            hr = m_pInputPin->GetAllocator(&m_pAllocator);
            FC_LOG("  GetAllocator: 0x%08lx, ptr=%p", hr, m_pAllocator);

            if (FAILED(hr) || !m_pAllocator) {
                FC_LOG("  Downstream allocator failed, creating our own");
                hr = CoCreateInstance(CLSID_MemoryAllocator, nullptr, CLSCTX_INPROC_SERVER,
                    IID_IMemAllocator, (void**)&m_pAllocator);
                FC_LOG("  CoCreateInstance MemoryAllocator: 0x%08lx, ptr=%p", hr, m_pAllocator);
            }

            if (m_pAllocator) {
                hr = m_pAllocator->SetProperties(&requested, &actual);
                FC_LOG("  SetProperties: 0x%08lx, actual: cBuffers=%ld cbBuffer=%ld cbAlign=%ld",
                    hr, actual.cBuffers, actual.cbBuffer, actual.cbAlign);

                hr = m_pInputPin->NotifyAllocator(m_pAllocator, FALSE);
                FC_LOG("  NotifyAllocator: 0x%08lx", hr);
            } else {
                FC_LOG("  ERROR: No allocator available!");
            }
        }

        FC_LOG("Connect SUCCEEDED: %s %dx%d", GuidToSubtype(m_mediaType.subtype),
            vih ? vih->bmiHeader.biWidth : 0, vih ? abs(vih->bmiHeader.biHeight) : 0);
        return S_OK;
    };

    // Try caller-proposed type first
    if (pmt && pmt->majortype != GUID_NULL) {
        AM_MEDIA_TYPE proposedType;
        CopyMediaType(&proposedType, pmt);
        HRESULT hr = tryConnect(proposedType, "proposed");
        FreeMediaType(proposedType);
        if (SUCCEEDED(hr)) return S_OK;
    }

    // Try our default type
    {
        HRESULT hr = tryConnect(m_mediaType, "default");
        if (SUCCEEDED(hr)) return S_OK;
    }

    // Try all supported types
    int count = CEnumMediaTypes::GetMediaTypeCount();
    FC_LOG("Trying all %d media types...", count);
    for (int i = 0; i < count; i++) {
        AM_MEDIA_TYPE tryType;
        CEnumMediaTypes::GetMediaType(i, &tryType);
        HRESULT hr = tryConnect(tryType, "enumerated");
        if (SUCCEEDED(hr)) {
            FreeMediaType(tryType);
            return S_OK;
        }
        FreeMediaType(tryType);
    }

    FC_LOG("Connect FAILED: no acceptable types");
    return VFW_E_NO_ACCEPTABLE_TYPES;
}

STDMETHODIMP VirtualCameraPin::ReceiveConnection(IPin*, const AM_MEDIA_TYPE*) {
    return E_UNEXPECTED;
}

STDMETHODIMP VirtualCameraPin::Disconnect() {
    FC_LOG("Disconnect() called");
    EnterCriticalSection(&m_cs);
    if (!m_connected) { LeaveCriticalSection(&m_cs); return S_FALSE; }
    StopStreaming();
    if (m_pAllocator) { m_pAllocator->Release(); m_pAllocator = nullptr; }
    if (m_pInputPin) { m_pInputPin->Release(); m_pInputPin = nullptr; }
    if (m_pConnectedPin) { m_pConnectedPin->Release(); m_pConnectedPin = nullptr; }
    m_connected = false;
    LeaveCriticalSection(&m_cs);
    FC_LOG("Disconnect done");
    return S_OK;
}

STDMETHODIMP VirtualCameraPin::ConnectedTo(IPin** ppPin) {
    if (!ppPin) return E_POINTER;
    if (!m_connected) { *ppPin = nullptr; return VFW_E_NOT_CONNECTED; }
    *ppPin = m_pConnectedPin;
    m_pConnectedPin->AddRef();
    return S_OK;
}

STDMETHODIMP VirtualCameraPin::ConnectionMediaType(AM_MEDIA_TYPE* pmt) {
    if (!pmt) return E_POINTER;
    if (!m_connected) { ZeroMemory(pmt, sizeof(*pmt)); return VFW_E_NOT_CONNECTED; }
    CopyMediaType(pmt, &m_mediaType);
    return S_OK;
}

STDMETHODIMP VirtualCameraPin::QueryPinInfo(PIN_INFO* pInfo) {
    if (!pInfo) return E_POINTER;
    pInfo->pFilter = m_pFilter;
    if (m_pFilter) m_pFilter->AddRef();
    pInfo->dir = PINDIR_OUTPUT;
    wcscpy_s(pInfo->achName, L"Output");
    return S_OK;
}

STDMETHODIMP VirtualCameraPin::QueryDirection(PIN_DIRECTION* pPinDir) {
    if (!pPinDir) return E_POINTER;
    *pPinDir = PINDIR_OUTPUT;
    return S_OK;
}

STDMETHODIMP VirtualCameraPin::QueryId(LPWSTR* Id) {
    if (!Id) return E_POINTER;
    *Id = (LPWSTR)CoTaskMemAlloc(sizeof(WCHAR) * 8);
    if (!*Id) return E_OUTOFMEMORY;
    wcscpy_s(*Id, 8, L"Output");
    return S_OK;
}

STDMETHODIMP VirtualCameraPin::QueryAccept(const AM_MEDIA_TYPE* pmt) {
    if (!pmt) return E_POINTER;
    if (pmt->majortype != MEDIATYPE_Video) return S_FALSE;
    if (pmt->subtype != MEDIASUBTYPE_YUY2 && pmt->subtype != MEDIASUBTYPE_RGB24) return S_FALSE;
    if (pmt->formattype != FORMAT_VideoInfo) return S_FALSE;
    FC_LOG("QueryAccept: %s -> S_OK", GuidToSubtype(pmt->subtype));
    return S_OK;
}

STDMETHODIMP VirtualCameraPin::EnumMediaTypes(IEnumMediaTypes** ppEnum) {
    if (!ppEnum) return E_POINTER;
    *ppEnum = new CEnumMediaTypes(0);
    return (*ppEnum) ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP VirtualCameraPin::QueryInternalConnections(IPin**, ULONG*) { return E_NOTIMPL; }
STDMETHODIMP VirtualCameraPin::EndOfStream() { return S_OK; }
STDMETHODIMP VirtualCameraPin::BeginFlush() { return S_OK; }
STDMETHODIMP VirtualCameraPin::EndFlush() { return S_OK; }
STDMETHODIMP VirtualCameraPin::NewSegment(REFERENCE_TIME, REFERENCE_TIME, double) { return S_OK; }

// ─── IKsPropertySet ───

STDMETHODIMP VirtualCameraPin::Set(REFGUID, DWORD, LPVOID, DWORD, LPVOID, DWORD) {
    return E_NOTIMPL;
}

STDMETHODIMP VirtualCameraPin::Get(REFGUID guidPropSet, DWORD dwPropID,
    LPVOID, DWORD, LPVOID pPropData, DWORD cbPropData, DWORD* pcbReturned) {
    if (guidPropSet == AMPROPSETID_Pin_UM || guidPropSet == KSPROPSETID_Pin_KM) {
        if (dwPropID == 0) { // KSPROPERTY_PIN_CATEGORY
            FC_LOG("IKsPropertySet::Get PIN_CATEGORY_CAPTURE");
            if (cbPropData < sizeof(GUID)) return E_UNEXPECTED;
            *(GUID*)pPropData = PIN_CATEGORY_CAPTURE;
            if (pcbReturned) *pcbReturned = sizeof(GUID);
            return S_OK;
        }
    }
    WCHAR gs[64]; StringFromGUID2(guidPropSet, gs, 64);
    char narrow[64]; WideCharToMultiByte(CP_UTF8, 0, gs, -1, narrow, 64, nullptr, nullptr);
    FC_LOG("IKsPropertySet::Get unknown propSet=%s propID=%lu -> E_NOTIMPL", narrow, dwPropID);
    return E_NOTIMPL;
}

STDMETHODIMP VirtualCameraPin::QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD* pTypeSupport) {
    if ((guidPropSet == AMPROPSETID_Pin_UM || guidPropSet == KSPROPSETID_Pin_KM) && dwPropID == 0) {
        if (pTypeSupport) *pTypeSupport = KSPROPERTY_SUPPORT_GET;
        return S_OK;
    }
    return E_NOTIMPL;
}

// ─── IAMStreamConfig ───

STDMETHODIMP VirtualCameraPin::SetFormat(AM_MEDIA_TYPE* pmt) {
    if (!pmt) return E_POINTER;
    FC_LOG("SetFormat: %s", GuidToSubtype(pmt->subtype));
    if (pmt->majortype != MEDIATYPE_Video) return VFW_E_INVALIDMEDIATYPE;
    if (pmt->subtype != MEDIASUBTYPE_YUY2 && pmt->subtype != MEDIASUBTYPE_RGB24)
        return VFW_E_INVALIDMEDIATYPE;
    EnterCriticalSection(&m_cs);
    FreeMediaType(m_mediaType);
    CopyMediaType(&m_mediaType, pmt);
    LeaveCriticalSection(&m_cs);
    return S_OK;
}

STDMETHODIMP VirtualCameraPin::GetFormat(AM_MEDIA_TYPE** ppmt) {
    if (!ppmt) return E_POINTER;
    *ppmt = (AM_MEDIA_TYPE*)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
    if (!*ppmt) return E_OUTOFMEMORY;
    CopyMediaType(*ppmt, &m_mediaType);
    return S_OK;
}

STDMETHODIMP VirtualCameraPin::GetNumberOfCapabilities(int* piCount, int* piSize) {
    if (!piCount || !piSize) return E_POINTER;
    *piCount = CEnumMediaTypes::GetMediaTypeCount();
    *piSize = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    FC_LOG("GetNumberOfCapabilities: count=%d", *piCount);
    return S_OK;
}

STDMETHODIMP VirtualCameraPin::GetStreamCaps(int iIndex, AM_MEDIA_TYPE** ppmt, BYTE* pSCC) {
    if (!ppmt || !pSCC) return E_POINTER;
    if (iIndex < 0 || iIndex >= CEnumMediaTypes::GetMediaTypeCount()) return S_FALSE;

    *ppmt = (AM_MEDIA_TYPE*)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
    if (!*ppmt) return E_OUTOFMEMORY;
    CEnumMediaTypes::GetMediaType(iIndex, *ppmt);

    VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)((*ppmt)->pbFormat);
    VIDEO_STREAM_CONFIG_CAPS* caps = (VIDEO_STREAM_CONFIG_CAPS*)pSCC;
    ZeroMemory(caps, sizeof(VIDEO_STREAM_CONFIG_CAPS));
    caps->guid = FORMAT_VideoInfo;
    caps->VideoStandard = 0;
    caps->InputSize.cx = vih->bmiHeader.biWidth;
    caps->InputSize.cy = abs(vih->bmiHeader.biHeight);
    caps->MinCroppingSize = caps->InputSize;
    caps->MaxCroppingSize = caps->InputSize;
    caps->CropGranularityX = 1;
    caps->CropGranularityY = 1;
    caps->MinOutputSize = caps->InputSize;
    caps->MaxOutputSize = caps->InputSize;
    caps->OutputGranularityX = 1;
    caps->OutputGranularityY = 1;
    caps->MinFrameInterval = vih->AvgTimePerFrame;
    caps->MaxFrameInterval = vih->AvgTimePerFrame;
    caps->MinBitsPerSecond = vih->bmiHeader.biSizeImage * (10000000LL / vih->AvgTimePerFrame) * 8;
    caps->MaxBitsPerSecond = caps->MinBitsPerSecond;

    return S_OK;
}

// ─── IQualityControl ───

STDMETHODIMP VirtualCameraPin::Notify(IBaseFilter*, Quality) { return S_OK; }
STDMETHODIMP VirtualCameraPin::SetSink(IQualityControl*) { return S_OK; }

// ─── Streaming ───

void VirtualCameraPin::StartStreaming() {
    EnterCriticalSection(&m_cs);
    if (m_streaming) {
        FC_LOG("StartStreaming: already streaming");
        LeaveCriticalSection(&m_cs);
        return;
    }

    FC_LOG("StartStreaming: connected=%d, inputPin=%p, allocator=%p",
        m_connected, m_pInputPin, m_pAllocator);

    // Open shared memory
    m_hMutex = OpenMutexA(SYNCHRONIZE, FALSE, FREEZECAM_MUTEX_NAME);
    m_hMapFile = OpenFileMappingA(FILE_MAP_READ, FALSE, FREEZECAM_SHM_NAME);
    m_hFrameEvent = OpenEventA(SYNCHRONIZE, FALSE, FREEZECAM_EVENT_NAME);

    FC_LOG("Shared mem: mutex=%p, mapFile=%p, event=%p", m_hMutex, m_hMapFile, m_hFrameEvent);

    if (m_hMapFile) {
        m_pSharedMem = MapViewOfFile(m_hMapFile, FILE_MAP_READ, 0, 0, 0);
        FC_LOG("MapViewOfFile: %p", m_pSharedMem);
    }

    if (m_pAllocator) {
        HRESULT hr = m_pAllocator->Commit();
        FC_LOG("Allocator Commit: 0x%08lx", hr);
    } else {
        FC_LOG("WARNING: No allocator to commit!");
    }

    m_streaming = true;
    m_streamThread = CreateThread(nullptr, 0, StreamThreadProc, this, 0, nullptr);
    FC_LOG("Stream thread created: handle=%p", m_streamThread);
    LeaveCriticalSection(&m_cs);
}

void VirtualCameraPin::StopStreaming() {
    FC_LOG("StopStreaming called, streaming=%d", m_streaming);
    EnterCriticalSection(&m_cs);
    m_streaming = false;
    LeaveCriticalSection(&m_cs);

    if (m_streamThread) {
        DWORD waitResult = WaitForSingleObject(m_streamThread, 3000);
        FC_LOG("Thread wait result: %lu", waitResult);
        CloseHandle(m_streamThread);
        m_streamThread = nullptr;
    }

    if (m_pAllocator) {
        m_pAllocator->Decommit();
    }

    if (m_pSharedMem) { UnmapViewOfFile(m_pSharedMem); m_pSharedMem = nullptr; }
    if (m_hMapFile) { CloseHandle(m_hMapFile); m_hMapFile = nullptr; }
    if (m_hFrameEvent) { CloseHandle(m_hFrameEvent); m_hFrameEvent = nullptr; }
    if (m_hMutex) { CloseHandle(m_hMutex); m_hMutex = nullptr; }
    FC_LOG("StopStreaming done");
}

DWORD WINAPI VirtualCameraPin::StreamThreadProc(LPVOID lpParam) {
    // Must init COM on this thread
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    auto* pin = static_cast<VirtualCameraPin*>(lpParam);
    pin->StreamLoop();
    CoUninitialize();
    return 0;
}

static inline BYTE clamp(int v) { return (BYTE)(v < 0 ? 0 : (v > 255 ? 255 : v)); }

void VirtualCameraPin::StreamLoop() {
    FC_LOG("StreamLoop started on thread %lu", GetCurrentThreadId());

    VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)m_mediaType.pbFormat;
    if (!vih) {
        FC_LOG("ERROR: no format on media type, aborting stream");
        return;
    }

    int width = vih->bmiHeader.biWidth;
    int height = abs(vih->bmiHeader.biHeight);
    REFERENCE_TIME frameDuration = vih->AvgTimePerFrame;
    bool isYUY2 = (m_mediaType.subtype == MEDIASUBTYPE_YUY2);

    FC_LOG("Stream format: %s %dx%d, frameDuration=%lld, sampleSize=%lu",
        isYUY2 ? "YUY2" : "RGB24", width, height, frameDuration, m_mediaType.lSampleSize);

    REFERENCE_TIME startTime = 0;
    int frameCount = 0;
    int errorCount = 0;

    // Wait for allocator and input pin
    for (int i = 0; i < 100 && m_streaming; i++) {
        if (m_pInputPin && m_pAllocator) break;
        FC_LOG("Waiting for inputPin=%p allocator=%p (attempt %d)", m_pInputPin, m_pAllocator, i);
        Sleep(50);
    }

    if (!m_pInputPin || !m_pAllocator) {
        FC_LOG("ERROR: inputPin=%p, allocator=%p - cannot stream!", m_pInputPin, m_pAllocator);
        return;
    }

    FC_LOG("Starting frame delivery loop");

    while (m_streaming) {
        // Try to (re)connect to shared memory periodically
        if (!m_hMapFile) {
            m_hMapFile = OpenFileMappingA(FILE_MAP_READ, FALSE, FREEZECAM_SHM_NAME);
            if (m_hMapFile) {
                m_pSharedMem = MapViewOfFile(m_hMapFile, FILE_MAP_READ, 0, 0, 0);
                FC_LOG("Reconnected shared memory: map=%p, view=%p", m_hMapFile, m_pSharedMem);
            }
        }
        if (!m_hFrameEvent) {
            m_hFrameEvent = OpenEventA(SYNCHRONIZE, FALSE, FREEZECAM_EVENT_NAME);
        }
        if (!m_hMutex) {
            m_hMutex = OpenMutexA(SYNCHRONIZE, FALSE, FREEZECAM_MUTEX_NAME);
        }

        // Pace ourselves
        if (m_hFrameEvent) {
            WaitForSingleObject(m_hFrameEvent, 33);
        } else {
            Sleep(33);
        }

        if (!m_streaming) break;

        IMediaSample* pSample = nullptr;
        HRESULT hr = m_pAllocator->GetBuffer(&pSample, nullptr, nullptr, 0);
        if (FAILED(hr) || !pSample) {
            if (errorCount < 5) {
                FC_LOG("GetBuffer FAILED: 0x%08lx (frame %d)", hr, frameCount);
            }
            errorCount++;
            Sleep(10);
            continue;
        }

        BYTE* pBuffer = nullptr;
        pSample->GetPointer(&pBuffer);
        long bufSize = pSample->GetSize();

        if (!pBuffer) {
            FC_LOG("GetPointer returned null!");
            pSample->Release();
            continue;
        }

        bool gotFrame = ReadSharedMemoryFrame(pBuffer, bufSize, width, height, isYUY2);
        if (!gotFrame) {
            GenerateTestPattern(pBuffer, width, height, isYUY2);
        }

        pSample->SetActualDataLength(m_mediaType.lSampleSize);

        REFERENCE_TIME end = startTime + frameDuration;
        pSample->SetTime(&startTime, &end);
        pSample->SetSyncPoint(TRUE);

        hr = m_pInputPin->Receive(pSample);
        pSample->Release();

        if (FAILED(hr)) {
            if (errorCount < 5) {
                FC_LOG("Receive FAILED: 0x%08lx (frame %d)", hr, frameCount);
            }
            errorCount++;
            if (hr == VFW_E_NOT_RUNNING) {
                FC_LOG("Downstream not running, sleeping...");
                Sleep(100);
            }
        }

        startTime = end;
        frameCount++;

        if (frameCount == 1 || frameCount == 10 || frameCount == 100 || frameCount % 500 == 0) {
            FC_LOG("Frame %d delivered OK (gotSharedMem=%d, errors=%d)", frameCount, gotFrame, errorCount);
        }
    }

    FC_LOG("StreamLoop ended, total frames=%d, errors=%d", frameCount, errorCount);
}

// Sample a source pixel with bilinear interpolation, returns RGBA
static void SampleBilinear(const BYTE* src, int srcW, int srcH, float fx, float fy,
                           int& outR, int& outG, int& outB) {
    int x0 = (int)fx, y0 = (int)fy;
    int x1 = x0 + 1, y1 = y0 + 1;
    if (x1 >= srcW) x1 = srcW - 1;
    if (y1 >= srcH) y1 = srcH - 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;

    float dx = fx - x0, dy = fy - y0;
    float w00 = (1 - dx) * (1 - dy), w10 = dx * (1 - dy);
    float w01 = (1 - dx) * dy,       w11 = dx * dy;

    const BYTE* p00 = src + (y0 * srcW + x0) * 4;
    const BYTE* p10 = src + (y0 * srcW + x1) * 4;
    const BYTE* p01 = src + (y1 * srcW + x0) * 4;
    const BYTE* p11 = src + (y1 * srcW + x1) * 4;

    outR = (int)(p00[0] * w00 + p10[0] * w10 + p01[0] * w01 + p11[0] * w11 + 0.5f);
    outG = (int)(p00[1] * w00 + p10[1] * w10 + p01[1] * w01 + p11[1] * w11 + 0.5f);
    outB = (int)(p00[2] * w00 + p10[2] * w10 + p01[2] * w01 + p11[2] * w11 + 0.5f);
}

bool VirtualCameraPin::ReadSharedMemoryFrame(BYTE* pBuffer, int bufferSize, int width, int height, bool isYUY2) {
    if (!m_pSharedMem) return false;

    if (m_hMutex) {
        WaitForSingleObject(m_hMutex, 16);
    }

    auto* header = static_cast<const FreezeCamFrameHeader*>(m_pSharedMem);
    if (!header->isActive || header->width == 0 || header->height == 0) {
        if (m_hMutex) ReleaseMutex(m_hMutex);
        return false;
    }

    const BYTE* srcPixels = (const BYTE*)m_pSharedMem + sizeof(FreezeCamFrameHeader);
    int srcW = header->width;
    int srcH = header->height;

    // Aspect-ratio-preserving fit (letterbox/pillarbox)
    float srcAspect = (float)srcW / srcH;
    float dstAspect = (float)width / height;

    int drawW, drawH, offsetX, offsetY;
    if (srcAspect > dstAspect) {
        // Source is wider — fit to width, letterbox top/bottom
        drawW = width;
        drawH = (int)(width / srcAspect + 0.5f);
        offsetX = 0;
        offsetY = (height - drawH) / 2;
    } else {
        // Source is taller — fit to height, pillarbox left/right
        drawH = height;
        drawW = (int)(height * srcAspect + 0.5f);
        offsetX = (width - drawW) / 2;
        offsetY = 0;
    }
    // Ensure even offsets for YUY2 (pairs of pixels)
    offsetX &= ~1;
    drawW &= ~1;

    float scaleX = (float)srcW / drawW;
    float scaleY = (float)srcH / drawH;

    if (isYUY2) {
        // Fill entire buffer with black (Y=16, U=128, V=128)
        int dstStride = width * 2;
        for (int y = 0; y < height; y++) {
            BYTE* row = pBuffer + y * dstStride;
            for (int x = 0; x < width; x += 2) {
                row[x * 2 + 0] = 16;
                row[x * 2 + 1] = 128;
                row[x * 2 + 2] = 16;
                row[x * 2 + 3] = 128;
            }
        }
        // Draw scaled image into the centered rect
        for (int dy = 0; dy < drawH; dy++) {
            int y = dy + offsetY;
            if (y < 0 || y >= height) continue;
            float srcY = dy * scaleY;
            BYTE* dstLine = pBuffer + y * dstStride;

            for (int dx = 0; dx < drawW - 1; dx += 2) {
                int x = dx + offsetX;
                if (x < 0 || x + 1 >= width) continue;

                int r0, g0, b0, r1, g1, b1;
                SampleBilinear(srcPixels, srcW, srcH, dx * scaleX, srcY, r0, g0, b0);
                SampleBilinear(srcPixels, srcW, srcH, (dx + 1) * scaleX, srcY, r1, g1, b1);

                BYTE y0 = clamp(((66 * r0 + 129 * g0 + 25 * b0 + 128) >> 8) + 16);
                BYTE y1v = clamp(((66 * r1 + 129 * g1 + 25 * b1 + 128) >> 8) + 16);
                int avgR = (r0 + r1) >> 1, avgG = (g0 + g1) >> 1, avgB = (b0 + b1) >> 1;
                BYTE u = clamp(((-38 * avgR - 74 * avgG + 112 * avgB + 128) >> 8) + 128);
                BYTE v = clamp(((112 * avgR - 94 * avgG - 18 * avgB + 128) >> 8) + 128);

                dstLine[x * 2 + 0] = y0;
                dstLine[x * 2 + 1] = u;
                dstLine[x * 2 + 2] = y1v;
                dstLine[x * 2 + 3] = v;
            }
        }
    } else {
        int dstStride = ((width * 3 + 3) & ~3);
        // Fill with black
        ZeroMemory(pBuffer, dstStride * height);
        for (int dy = 0; dy < drawH; dy++) {
            int y = dy + offsetY;
            if (y < 0 || y >= height) continue;
            int dstRow = (height - 1 - y);
            float srcY = dy * scaleY;
            BYTE* dstLine = pBuffer + dstRow * dstStride;

            for (int dx = 0; dx < drawW; dx++) {
                int x = dx + offsetX;
                if (x < 0 || x >= width) continue;
                int r, g, b;
                SampleBilinear(srcPixels, srcW, srcH, dx * scaleX, srcY, r, g, b);
                dstLine[x * 3 + 0] = (BYTE)b;
                dstLine[x * 3 + 1] = (BYTE)g;
                dstLine[x * 3 + 2] = (BYTE)r;
            }
        }
    }

    if (m_hMutex) ReleaseMutex(m_hMutex);
    return true;
}

void VirtualCameraPin::GenerateTestPattern(BYTE* pBuffer, int width, int height, bool isYUY2) {
    static int frameCount = 0;
    frameCount++;

    if (isYUY2) {
        int stride = width * 2;
        for (int y = 0; y < height; y++) {
            BYTE* row = pBuffer + y * stride;
            for (int x = 0; x < width; x += 2) {
                float fx = (float)x / width;
                float wave = sinf(fx * 6.28f + frameCount * 0.05f) * 0.5f + 0.5f;
                BYTE luma = (BYTE)(16 + wave * 40);
                row[x * 2 + 0] = luma;
                row[x * 2 + 1] = 140;
                row[x * 2 + 2] = (BYTE)(16 + wave * 35);
                row[x * 2 + 3] = 110;
            }
        }
    } else {
        int stride = ((width * 3 + 3) & ~3);
        for (int y = 0; y < height; y++) {
            BYTE* row = pBuffer + y * stride;
            for (int x = 0; x < width; x++) {
                float fx = (float)x / width;
                float wave = sinf(fx * 6.28f + frameCount * 0.05f) * 0.5f + 0.5f;
                row[x * 3 + 0] = (BYTE)(20 + wave * 30);
                row[x * 3 + 1] = (BYTE)(15 + ((float)y / height) * 20);
                row[x * 3 + 2] = (BYTE)(10 + fx * 15);
            }
        }
    }
}
