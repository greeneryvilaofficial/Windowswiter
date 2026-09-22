// ClassFactory.h — IClassFactory standar untuk mengizinkan Windows meng-instantiate
// GuitarwiterTextService lewat CoCreateInstance(CLSID_GuitarwiterTextService, ...).
#pragma once
#include <unknwn.h>
#include <atomic>
#include "GuitarwiterTextService.h"

namespace guitarwiter {

// Deklarasi di scope namespace (BUKAN di dalam function body) -- supaya pasti terikat ke
// guitarwiter::g_lockCount, bukan ke namespace global. Definisi sebenarnya ada di DllMain.cpp.
extern std::atomic<long> g_lockCount;

class ClassFactory : public IClassFactory {
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IClassFactory) { *ppv = this; AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&ref_); }
    STDMETHODIMP_(ULONG) Release() override { LONG r = InterlockedDecrement(&ref_); if (r == 0) delete this; return r; }

    STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        *ppv = nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;
        auto* svc = new GuitarwiterTextService();
        HRESULT hr = svc->QueryInterface(riid, ppv);
        svc->Release();
        return hr;
    }
    STDMETHODIMP LockServer(BOOL lock) override {
        if (lock) g_lockCount.fetch_add(1);
        else g_lockCount.fetch_sub(1);
        return S_OK;
    }
private:
    LONG ref_ = 1;
};

}  // namespace guitarwiter
