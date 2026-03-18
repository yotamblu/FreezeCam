#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>
#include <atomic>

class ClassFactory : public IClassFactory {
public:
    ClassFactory();
    ~ClassFactory();

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override;
    STDMETHODIMP LockServer(BOOL fLock) override;

private:
    std::atomic<LONG> m_refCount;
};
