// DllMain.cpp — entry point DLL. Windows memuat GuitarwiterTSF.dll dan memanggil fungsi-fungsi
// standar di sini (DllGetClassObject dipanggil TSF runtime saat mengaktifkan keyboard ini;
// DllRegisterServer/DllUnregisterServer dipanggil regsvr32.exe saat install/uninstall).
#include <windows.h>
#include <atomic>
#include "Guids.h"
#include "ClassFactory.h"
#include "Registration.h"

namespace guitarwiter {
std::atomic<long> g_lockCount{0};
}
static std::atomic<long> g_objectCount{0};
static HMODULE g_hModule = nullptr;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    *ppv = nullptr;
    if (rclsid != guitarwiter::CLSID_GuitarwiterTextService) return CLASS_E_CLASSNOTAVAILABLE;
    auto* factory = new guitarwiter::ClassFactory();
    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow() {
    return (guitarwiter::g_lockCount.load() == 0 && g_objectCount.load() == 0) ? S_OK : S_FALSE;
}

// Dipanggil oleh: regsvr32.exe GuitarwiterTSF.dll
STDAPI DllRegisterServer() {
    using namespace guitarwiter;
    HRESULT hr = RegisterComServer(g_hModule);
    if (FAILED(hr)) return hr;
    hr = RegisterCategories();
    if (FAILED(hr)) return hr;
    hr = RegisterTsfProfile();
    if (FAILED(hr)) return hr;
    return S_OK;
}

// Dipanggil oleh: regsvr32.exe /u GuitarwiterTSF.dll
STDAPI DllUnregisterServer() {
    using namespace guitarwiter;
    UnregisterTsfProfile();
    UnregisterCategories();
    UnregisterComServer();
    return S_OK;
}
