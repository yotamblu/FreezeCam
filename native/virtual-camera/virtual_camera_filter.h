#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dshow.h>
#include <dvdmedia.h>
#include <ks.h>
#include <ksmedia.h>
#include <ocidl.h>
#include <olectl.h>
#include <atomic>

class VirtualCameraPin;

class VirtualCameraFilter : public IBaseFilter, public IAMFilterMiscFlags,
                            public ISpecifyPropertyPages, public IPersistStream,
                            public IPersistPropertyBag, public IAMVideoControl {
public:
    VirtualCameraFilter(IUnknown* pUnk, HRESULT* phr);
    ~VirtualCameraFilter();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IPersist
    STDMETHODIMP GetClassID(CLSID* pClsID) override;

    // IMediaFilter
    STDMETHODIMP Stop() override;
    STDMETHODIMP Pause() override;
    STDMETHODIMP Run(REFERENCE_TIME tStart) override;
    STDMETHODIMP GetState(DWORD dwMSecs, FILTER_STATE* pState) override;
    STDMETHODIMP SetSyncSource(IReferenceClock* pClock) override;
    STDMETHODIMP GetSyncSource(IReferenceClock** pClock) override;

    // IBaseFilter
    STDMETHODIMP EnumPins(IEnumPins** ppEnum) override;
    STDMETHODIMP FindPin(LPCWSTR Id, IPin** ppPin) override;
    STDMETHODIMP QueryFilterInfo(FILTER_INFO* pInfo) override;
    STDMETHODIMP JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR pName) override;
    STDMETHODIMP QueryVendorInfo(LPWSTR* pVendorInfo) override;

    // IAMFilterMiscFlags
    STDMETHODIMP_(ULONG) GetMiscFlags() override;

    // ISpecifyPropertyPages
    STDMETHODIMP GetPages(CAUUID* pPages) override;

    // IPersistStream
    STDMETHODIMP IsDirty() override;
    STDMETHODIMP Load(IStream* pStm) override;
    STDMETHODIMP Save(IStream* pStm, BOOL fClearDirty) override;
    STDMETHODIMP GetSizeMax(ULARGE_INTEGER* pcbSize) override;

    // IPersistPropertyBag
    STDMETHODIMP InitNew() override;
    STDMETHODIMP Load(IPropertyBag* pPropBag, IErrorLog* pErrorLog) override;
    STDMETHODIMP Save(IPropertyBag* pPropBag, BOOL fClearDirty, BOOL fSaveAllProperties) override;

    // IAMVideoControl
    STDMETHODIMP GetCaps(IPin* pPin, long* pCapsFlags) override;
    STDMETHODIMP SetMode(IPin* pPin, long Mode) override;
    STDMETHODIMP GetMode(IPin* pPin, long* Mode) override;
    STDMETHODIMP GetCurrentActualFrameRate(IPin* pPin, LONGLONG* ActualFrameRate) override;
    STDMETHODIMP GetMaxAvailableFrameRate(IPin* pPin, long iIndex, SIZE Dimensions, LONGLONG* MaxAvailableFrameRate) override;
    STDMETHODIMP GetFrameRateList(IPin* pPin, long iIndex, SIZE Dimensions, long* ListSize, LONGLONG** FrameRates) override;

    FILTER_STATE GetFilterState() const { return m_state; }

private:
    std::atomic<LONG> m_refCount;
    FILTER_STATE m_state;
    IFilterGraph* m_pGraph;
    IReferenceClock* m_pClock;
    VirtualCameraPin* m_pPin;
    WCHAR m_filterName[128];
    CRITICAL_SECTION m_cs;
};

// Enum pins helper
class CEnumPins : public IEnumPins {
public:
    CEnumPins(VirtualCameraFilter* pFilter, IPin* pPin);
    ~CEnumPins();

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    STDMETHODIMP Next(ULONG cPins, IPin** ppPins, ULONG* pcFetched) override;
    STDMETHODIMP Skip(ULONG cPins) override;
    STDMETHODIMP Reset() override;
    STDMETHODIMP Clone(IEnumPins** ppEnum) override;

private:
    std::atomic<LONG> m_refCount;
    VirtualCameraFilter* m_pFilter;
    IPin* m_pPin;
    int m_position;
};

// Enum media types helper
class CEnumMediaTypes : public IEnumMediaTypes {
public:
    CEnumMediaTypes(int index = 0);
    ~CEnumMediaTypes();

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    STDMETHODIMP Next(ULONG cMediaTypes, AM_MEDIA_TYPE** ppMediaTypes, ULONG* pcFetched) override;
    STDMETHODIMP Skip(ULONG cMediaTypes) override;
    STDMETHODIMP Reset() override;
    STDMETHODIMP Clone(IEnumMediaTypes** ppEnum) override;

    static void GetMediaType(int index, AM_MEDIA_TYPE* pmt);
    static int GetMediaTypeCount();

private:
    std::atomic<LONG> m_refCount;
    int m_position;
};
