#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dshow.h>
#include <dvdmedia.h>
#include <atomic>
#include "virtual_camera_filter.h"

class VirtualCameraPin : public IPin, public IKsPropertySet, public IAMStreamConfig, public IQualityControl {
public:
    VirtualCameraPin(VirtualCameraFilter* pFilter, HRESULT* phr);
    ~VirtualCameraPin();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IPin
    STDMETHODIMP Connect(IPin* pReceivePin, const AM_MEDIA_TYPE* pmt) override;
    STDMETHODIMP ReceiveConnection(IPin* pConnector, const AM_MEDIA_TYPE* pmt) override;
    STDMETHODIMP Disconnect() override;
    STDMETHODIMP ConnectedTo(IPin** ppPin) override;
    STDMETHODIMP ConnectionMediaType(AM_MEDIA_TYPE* pmt) override;
    STDMETHODIMP QueryPinInfo(PIN_INFO* pInfo) override;
    STDMETHODIMP QueryDirection(PIN_DIRECTION* pPinDir) override;
    STDMETHODIMP QueryId(LPWSTR* Id) override;
    STDMETHODIMP QueryAccept(const AM_MEDIA_TYPE* pmt) override;
    STDMETHODIMP EnumMediaTypes(IEnumMediaTypes** ppEnum) override;
    STDMETHODIMP QueryInternalConnections(IPin** apPin, ULONG* nPin) override;
    STDMETHODIMP EndOfStream() override;
    STDMETHODIMP BeginFlush() override;
    STDMETHODIMP EndFlush() override;
    STDMETHODIMP NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate) override;

    // IKsPropertySet
    STDMETHODIMP Set(REFGUID guidPropSet, DWORD dwPropID, LPVOID pInstanceData,
                     DWORD cbInstanceData, LPVOID pPropData, DWORD cbPropData) override;
    STDMETHODIMP Get(REFGUID guidPropSet, DWORD dwPropID, LPVOID pInstanceData,
                     DWORD cbInstanceData, LPVOID pPropData, DWORD cbPropData,
                     DWORD* pcbReturned) override;
    STDMETHODIMP QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD* pTypeSupport) override;

    // IAMStreamConfig
    STDMETHODIMP SetFormat(AM_MEDIA_TYPE* pmt) override;
    STDMETHODIMP GetFormat(AM_MEDIA_TYPE** ppmt) override;
    STDMETHODIMP GetNumberOfCapabilities(int* piCount, int* piSize) override;
    STDMETHODIMP GetStreamCaps(int iIndex, AM_MEDIA_TYPE** ppmt, BYTE* pSCC) override;

    // IQualityControl
    STDMETHODIMP Notify(IBaseFilter* pSelf, Quality q) override;
    STDMETHODIMP SetSink(IQualityControl* piqc) override;

    void StartStreaming();
    void StopStreaming();

private:
    static DWORD WINAPI StreamThreadProc(LPVOID lpParam);
    void StreamLoop();
    bool ReadSharedMemoryFrame(BYTE* pBuffer, int bufferSize, int width, int height, bool isYUY2);
    void GenerateTestPattern(BYTE* pBuffer, int width, int height, bool isYUY2);

    std::atomic<LONG> m_refCount;
    VirtualCameraFilter* m_pFilter;
    IPin* m_pConnectedPin;
    IMemInputPin* m_pInputPin;
    IMemAllocator* m_pAllocator;
    AM_MEDIA_TYPE m_mediaType;
    bool m_connected;
    int m_preferredType;

    HANDLE m_streamThread;
    bool m_streaming;
    CRITICAL_SECTION m_cs;

    // Shared memory
    HANDLE m_hMapFile;
    HANDLE m_hFrameEvent;
    HANDLE m_hMutex;
    void* m_pSharedMem;
};
