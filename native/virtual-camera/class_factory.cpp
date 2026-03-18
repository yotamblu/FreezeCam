#include "class_factory.h"
#include "virtual_camera_filter.h"
#include "guids.h"

extern std::atomic<LONG> g_dllRefCount;

ClassFactory::ClassFactory() : m_refCount(1) {}
ClassFactory::~ClassFactory() {}

STDMETHODIMP ClassFactory::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IClassFactory) {
        *ppv = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) ClassFactory::AddRef() { return ++m_refCount; }
STDMETHODIMP_(ULONG) ClassFactory::Release() {
    LONG ref = --m_refCount;
    if (ref == 0) delete this;
    return ref;
}

STDMETHODIMP ClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (pUnkOuter) return CLASS_E_NOAGGREGATION;

    HRESULT hr = S_OK;
    auto* filter = new VirtualCameraFilter(nullptr, &hr);
    if (!filter) return E_OUTOFMEMORY;
    if (FAILED(hr)) {
        filter->Release();
        return hr;
    }

    hr = filter->QueryInterface(riid, ppv);
    filter->Release();
    return hr;
}

STDMETHODIMP ClassFactory::LockServer(BOOL fLock) {
    if (fLock) ++g_dllRefCount;
    else --g_dllRefCount;
    return S_OK;
}
