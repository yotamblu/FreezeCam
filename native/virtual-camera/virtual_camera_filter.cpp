#include "virtual_camera_filter.h"
#include "virtual_camera_pin.h"
#include "guids.h"
#include "logger.h"
#include <cstring>

// ─── VirtualCameraFilter ───

VirtualCameraFilter::VirtualCameraFilter(IUnknown* pUnk, HRESULT* phr)
    : m_refCount(1)
    , m_state(State_Stopped)
    , m_pGraph(nullptr)
    , m_pClock(nullptr)
    , m_pPin(nullptr) {
    InitializeCriticalSection(&m_cs);
    wcscpy_s(m_filterName, L"FreezeCam Pro Virtual Camera");
    FC_LOG("Constructor called");

    m_pPin = new VirtualCameraPin(this, phr);
    if (!m_pPin) {
        *phr = E_OUTOFMEMORY;
        FC_LOG("ERROR: Failed to create pin");
    } else {
        FC_LOG("Pin created OK");
    }
}

VirtualCameraFilter::~VirtualCameraFilter() {
    FC_LOG("Destructor called");
    if (m_pPin) {
        m_pPin->Release();
        m_pPin = nullptr;
    }
    if (m_pClock) {
        m_pClock->Release();
        m_pClock = nullptr;
    }
    DeleteCriticalSection(&m_cs);
}

static void LogGuid(const char* context, REFIID riid, bool found) {
    WCHAR guidStr[64];
    StringFromGUID2(riid, guidStr, 64);
    char narrow[64];
    WideCharToMultiByte(CP_UTF8, 0, guidStr, -1, narrow, 64, nullptr, nullptr);
    FC_LOG("QI %s: %s -> %s", context, narrow, found ? "OK" : "E_NOINTERFACE");
}

STDMETHODIMP VirtualCameraFilter::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown) *ppv = static_cast<IBaseFilter*>(this);
    else if (riid == IID_IPersist) *ppv = static_cast<IPersist*>(static_cast<IMediaFilter*>(this));
    else if (riid == IID_IPersistStream) *ppv = static_cast<IPersistStream*>(this);
    else if (riid == IID_IPersistPropertyBag) *ppv = static_cast<IPersistPropertyBag*>(this);
    else if (riid == IID_IMediaFilter) *ppv = static_cast<IMediaFilter*>(this);
    else if (riid == IID_IBaseFilter) *ppv = static_cast<IBaseFilter*>(this);
    else if (riid == IID_IAMFilterMiscFlags) *ppv = static_cast<IAMFilterMiscFlags*>(this);
    else if (riid == IID_ISpecifyPropertyPages) *ppv = static_cast<ISpecifyPropertyPages*>(this);
    else if (riid == IID_IAMVideoControl) *ppv = static_cast<IAMVideoControl*>(this);
    else {
        *ppv = nullptr;
        LogGuid("Filter", riid, false);
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) VirtualCameraFilter::AddRef() { return ++m_refCount; }
STDMETHODIMP_(ULONG) VirtualCameraFilter::Release() {
    LONG ref = --m_refCount;
    if (ref == 0) delete this;
    return ref;
}

STDMETHODIMP VirtualCameraFilter::GetClassID(CLSID* pClsID) {
    if (!pClsID) return E_POINTER;
    *pClsID = CLSID_FreezeCamVirtualCamera;
    return S_OK;
}

STDMETHODIMP VirtualCameraFilter::Stop() {
    FC_LOG("Stop() called, current state=%d", m_state);
    EnterCriticalSection(&m_cs);
    if (m_state != State_Stopped) {
        m_pPin->StopStreaming();
        m_state = State_Stopped;
    }
    LeaveCriticalSection(&m_cs);
    FC_LOG("Stop() done");
    return S_OK;
}

STDMETHODIMP VirtualCameraFilter::Pause() {
    FC_LOG("Pause() called, current state=%d", m_state);
    EnterCriticalSection(&m_cs);
    if (m_state == State_Stopped) {
        m_pPin->StartStreaming();
    }
    m_state = State_Paused;
    LeaveCriticalSection(&m_cs);
    FC_LOG("Pause() done");
    return S_OK;
}

STDMETHODIMP VirtualCameraFilter::Run(REFERENCE_TIME tStart) {
    FC_LOG("Run() called, tStart=%lld, current state=%d", tStart, m_state);
    EnterCriticalSection(&m_cs);
    if (m_state == State_Stopped) {
        FC_LOG("Run: was stopped, starting streaming first");
        m_pPin->StartStreaming();
    }
    m_state = State_Running;
    LeaveCriticalSection(&m_cs);
    FC_LOG("Run() done");
    return S_OK;
}

STDMETHODIMP VirtualCameraFilter::GetState(DWORD dwMSecs, FILTER_STATE* pState) {
    if (!pState) return E_POINTER;
    *pState = m_state;
    return S_OK;
}

STDMETHODIMP VirtualCameraFilter::SetSyncSource(IReferenceClock* pClock) {
    FC_LOG("SetSyncSource: clock=%p", pClock);
    EnterCriticalSection(&m_cs);
    if (m_pClock) m_pClock->Release();
    m_pClock = pClock;
    if (m_pClock) m_pClock->AddRef();
    LeaveCriticalSection(&m_cs);
    return S_OK;
}

STDMETHODIMP VirtualCameraFilter::GetSyncSource(IReferenceClock** pClock) {
    if (!pClock) return E_POINTER;
    EnterCriticalSection(&m_cs);
    *pClock = m_pClock;
    if (m_pClock) m_pClock->AddRef();
    LeaveCriticalSection(&m_cs);
    return S_OK;
}

STDMETHODIMP VirtualCameraFilter::EnumPins(IEnumPins** ppEnum) {
    FC_LOG("EnumPins called");
    if (!ppEnum) return E_POINTER;
    *ppEnum = new CEnumPins(this, m_pPin);
    return (*ppEnum) ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP VirtualCameraFilter::FindPin(LPCWSTR Id, IPin** ppPin) {
    if (!ppPin) return E_POINTER;
    if (wcscmp(Id, L"Output") == 0 || wcscmp(Id, L"1") == 0) {
        *ppPin = m_pPin;
        m_pPin->AddRef();
        return S_OK;
    }
    *ppPin = nullptr;
    return VFW_E_NOT_FOUND;
}

STDMETHODIMP VirtualCameraFilter::QueryFilterInfo(FILTER_INFO* pInfo) {
    if (!pInfo) return E_POINTER;
    wcscpy_s(pInfo->achName, m_filterName);
    pInfo->pGraph = m_pGraph;
    if (m_pGraph) m_pGraph->AddRef();
    return S_OK;
}

STDMETHODIMP VirtualCameraFilter::JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR pName) {
    FC_LOG("JoinFilterGraph: graph=%p, name=%ls", pGraph, pName ? pName : L"(null)");
    EnterCriticalSection(&m_cs);
    m_pGraph = pGraph;
    if (pName) wcscpy_s(m_filterName, pName);
    LeaveCriticalSection(&m_cs);
    return S_OK;
}

STDMETHODIMP VirtualCameraFilter::QueryVendorInfo(LPWSTR* pVendorInfo) {
    return E_NOTIMPL;
}

STDMETHODIMP_(ULONG) VirtualCameraFilter::GetMiscFlags() {
    return AM_FILTER_MISC_FLAGS_IS_SOURCE;
}

// ISpecifyPropertyPages — empty, no property pages
STDMETHODIMP VirtualCameraFilter::GetPages(CAUUID* pPages) {
    if (!pPages) return E_POINTER;
    pPages->cElems = 0;
    pPages->pElems = nullptr;
    return S_OK;
}

// IPersistStream — no-op, nothing to persist
STDMETHODIMP VirtualCameraFilter::IsDirty() { return S_FALSE; }
STDMETHODIMP VirtualCameraFilter::Load(IStream*) { return S_OK; }
STDMETHODIMP VirtualCameraFilter::Save(IStream*, BOOL) { return S_OK; }
STDMETHODIMP VirtualCameraFilter::GetSizeMax(ULARGE_INTEGER* pcbSize) {
    if (pcbSize) pcbSize->QuadPart = 0;
    return S_OK;
}

// IPersistPropertyBag — accept device moniker properties
STDMETHODIMP VirtualCameraFilter::InitNew() {
    FC_LOG("IPersistPropertyBag::InitNew");
    return S_OK;
}

STDMETHODIMP VirtualCameraFilter::Load(IPropertyBag* pPropBag, IErrorLog* pErrorLog) {
    FC_LOG("IPersistPropertyBag::Load called");
    return S_OK;
}

STDMETHODIMP VirtualCameraFilter::Save(IPropertyBag* pPropBag, BOOL fClearDirty, BOOL fSaveAllProperties) {
    return E_NOTIMPL;
}

// ─── IAMVideoControl ───

STDMETHODIMP VirtualCameraFilter::GetCaps(IPin* pPin, long* pCapsFlags) {
    FC_LOG("IAMVideoControl::GetCaps");
    if (!pCapsFlags) return E_POINTER;
    *pCapsFlags = 0; // no flip/mirror support
    return S_OK;
}

STDMETHODIMP VirtualCameraFilter::SetMode(IPin*, long) { return E_NOTIMPL; }
STDMETHODIMP VirtualCameraFilter::GetMode(IPin* pPin, long* Mode) {
    if (!Mode) return E_POINTER;
    *Mode = 0;
    return S_OK;
}

STDMETHODIMP VirtualCameraFilter::GetCurrentActualFrameRate(IPin* pPin, LONGLONG* ActualFrameRate) {
    if (!ActualFrameRate) return E_POINTER;
    *ActualFrameRate = 333333; // 30fps in 100ns units
    return S_OK;
}

STDMETHODIMP VirtualCameraFilter::GetMaxAvailableFrameRate(IPin*, long, SIZE, LONGLONG* MaxAvailableFrameRate) {
    if (!MaxAvailableFrameRate) return E_POINTER;
    *MaxAvailableFrameRate = 333333;
    return S_OK;
}

STDMETHODIMP VirtualCameraFilter::GetFrameRateList(IPin*, long, SIZE, long* ListSize, LONGLONG** FrameRates) {
    if (!ListSize || !FrameRates) return E_POINTER;
    *ListSize = 1;
    *FrameRates = (LONGLONG*)CoTaskMemAlloc(sizeof(LONGLONG));
    if (!*FrameRates) return E_OUTOFMEMORY;
    (*FrameRates)[0] = 333333;
    return S_OK;
}

// ─── CEnumPins ───

CEnumPins::CEnumPins(VirtualCameraFilter* pFilter, IPin* pPin)
    : m_refCount(1), m_pFilter(pFilter), m_pPin(pPin), m_position(0) {
    if (m_pFilter) m_pFilter->AddRef();
    if (m_pPin) m_pPin->AddRef();
}

CEnumPins::~CEnumPins() {
    if (m_pFilter) m_pFilter->Release();
    if (m_pPin) m_pPin->Release();
}

STDMETHODIMP CEnumPins::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IEnumPins) {
        *ppv = static_cast<IEnumPins*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CEnumPins::AddRef() { return ++m_refCount; }
STDMETHODIMP_(ULONG) CEnumPins::Release() {
    LONG ref = --m_refCount;
    if (ref == 0) delete this;
    return ref;
}

STDMETHODIMP CEnumPins::Next(ULONG cPins, IPin** ppPins, ULONG* pcFetched) {
    if (!ppPins) return E_POINTER;
    ULONG fetched = 0;
    while (fetched < cPins && m_position < 1) {
        ppPins[fetched] = m_pPin;
        m_pPin->AddRef();
        m_position++;
        fetched++;
    }
    if (pcFetched) *pcFetched = fetched;
    return (fetched == cPins) ? S_OK : S_FALSE;
}

STDMETHODIMP CEnumPins::Skip(ULONG cPins) {
    m_position += cPins;
    return (m_position <= 1) ? S_OK : S_FALSE;
}

STDMETHODIMP CEnumPins::Reset() { m_position = 0; return S_OK; }

STDMETHODIMP CEnumPins::Clone(IEnumPins** ppEnum) {
    if (!ppEnum) return E_POINTER;
    auto* p = new CEnumPins(m_pFilter, m_pPin);
    p->m_position = m_position;
    *ppEnum = p;
    return S_OK;
}

// ─── CEnumMediaTypes ───

struct MediaTypeEntry {
    int width;
    int height;
    int fps;
    bool isYUY2;
};

static const MediaTypeEntry g_mediaTypes[] = {
    { 1280, 720, 30, true },
    { 640, 480, 30, true },
    { 1920, 1080, 30, true },
    { 1280, 720, 15, true },
    { 1280, 720, 30, false },
    { 640, 480, 30, false },
    { 1920, 1080, 30, false },
};

int CEnumMediaTypes::GetMediaTypeCount() {
    return _countof(g_mediaTypes);
}

void CEnumMediaTypes::GetMediaType(int index, AM_MEDIA_TYPE* pmt) {
    if (index < 0 || index >= GetMediaTypeCount()) return;

    const auto& entry = g_mediaTypes[index];

    ZeroMemory(pmt, sizeof(AM_MEDIA_TYPE));
    pmt->majortype = MEDIATYPE_Video;
    pmt->bFixedSizeSamples = TRUE;
    pmt->bTemporalCompression = FALSE;
    pmt->formattype = FORMAT_VideoInfo;

    VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)CoTaskMemAlloc(sizeof(VIDEOINFOHEADER));
    ZeroMemory(vih, sizeof(VIDEOINFOHEADER));
    vih->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    vih->bmiHeader.biWidth = entry.width;
    vih->bmiHeader.biPlanes = 1;
    vih->AvgTimePerFrame = 10000000LL / entry.fps;

    if (entry.isYUY2) {
        pmt->subtype = MEDIASUBTYPE_YUY2;
        int sampleSize = entry.width * entry.height * 2;
        pmt->lSampleSize = sampleSize;
        vih->bmiHeader.biHeight = entry.height;
        vih->bmiHeader.biBitCount = 16;
        vih->bmiHeader.biCompression = MAKEFOURCC('Y','U','Y','2');
        vih->bmiHeader.biSizeImage = sampleSize;
    } else {
        pmt->subtype = MEDIASUBTYPE_RGB24;
        int stride = ((entry.width * 3 + 3) & ~3);
        int sampleSize = stride * entry.height;
        pmt->lSampleSize = sampleSize;
        vih->bmiHeader.biHeight = entry.height;
        vih->bmiHeader.biBitCount = 24;
        vih->bmiHeader.biCompression = BI_RGB;
        vih->bmiHeader.biSizeImage = sampleSize;
    }

    pmt->pbFormat = (BYTE*)vih;
    pmt->cbFormat = sizeof(VIDEOINFOHEADER);
}

CEnumMediaTypes::CEnumMediaTypes(int index)
    : m_refCount(1), m_position(index) {}

CEnumMediaTypes::~CEnumMediaTypes() {}

STDMETHODIMP CEnumMediaTypes::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IEnumMediaTypes) {
        *ppv = static_cast<IEnumMediaTypes*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CEnumMediaTypes::AddRef() { return ++m_refCount; }
STDMETHODIMP_(ULONG) CEnumMediaTypes::Release() {
    LONG ref = --m_refCount;
    if (ref == 0) delete this;
    return ref;
}

STDMETHODIMP CEnumMediaTypes::Next(ULONG cMediaTypes, AM_MEDIA_TYPE** ppMediaTypes, ULONG* pcFetched) {
    if (!ppMediaTypes) return E_POINTER;
    ULONG fetched = 0;
    int total = GetMediaTypeCount();
    while (fetched < cMediaTypes && m_position < total) {
        AM_MEDIA_TYPE* pmt = (AM_MEDIA_TYPE*)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
        if (!pmt) return E_OUTOFMEMORY;
        GetMediaType(m_position, pmt);
        ppMediaTypes[fetched] = pmt;
        m_position++;
        fetched++;
    }
    if (pcFetched) *pcFetched = fetched;
    return (fetched == cMediaTypes) ? S_OK : S_FALSE;
}

STDMETHODIMP CEnumMediaTypes::Skip(ULONG cMediaTypes) {
    m_position += cMediaTypes;
    return (m_position <= GetMediaTypeCount()) ? S_OK : S_FALSE;
}

STDMETHODIMP CEnumMediaTypes::Reset() { m_position = 0; return S_OK; }

STDMETHODIMP CEnumMediaTypes::Clone(IEnumMediaTypes** ppEnum) {
    if (!ppEnum) return E_POINTER;
    *ppEnum = new CEnumMediaTypes(m_position);
    return S_OK;
}
