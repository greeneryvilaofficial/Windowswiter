// Registration.h — daftar/lepas Text Service ini ke Windows: entri registry COM standar
// (HKCR\CLSID\...) DITAMBAH pendaftaran khusus TSF lewat ITfInputProcessorProfiles supaya
// muncul di Pengaturan > Waktu & Bahasa > Bahasa > Tambahkan keyboard.
//
// CATATAN JUJUR: ini bagian paling sering salah di implementasi TSF pertama kali (path
// registry, urutan panggilan Register vs RegisterCategory, dsb.) -- belum pernah diuji Windows
// asli. Kalau registrasi gagal, keyboard TIDAK akan muncul di daftar bahasa sama sekali (gagal
// senyap secara UI, biasanya perlu Event Viewer atau tool seperti Ftfmon untuk debug).
#pragma once
#include <msctf.h>
#include <wrl/client.h>
#include <string>
#include "Guids.h"

namespace guitarwiter {
using Microsoft::WRL::ComPtr;

inline std::wstring GetModulePath(HMODULE hModule) {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(hModule, path, MAX_PATH);
    return path;
}

// Tulis entri registry standar COM (HKCR\CLSID\{guid}\InprocServer32).
inline HRESULT RegisterComServer(HMODULE hModule) {
    wchar_t clsidStr[64];
    StringFromGUID2(CLSID_GuitarwiterTextService, clsidStr, 64);

    std::wstring keyPath = std::wstring(L"CLSID\\") + clsidStr;
    HKEY key;
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, keyPath.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return E_FAIL;
    const wchar_t* name = L"Guitarwiter Text Service";
    RegSetValueExW(key, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(name), (wcslen(name) + 1) * sizeof(wchar_t));
    RegCloseKey(key);

    std::wstring inprocPath = keyPath + L"\\InprocServer32";
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, inprocPath.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return E_FAIL;
    std::wstring modulePath = GetModulePath(hModule);
    RegSetValueExW(key, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(modulePath.c_str()), (modulePath.size() + 1) * sizeof(wchar_t));
    const wchar_t* model = L"Apartment";
    RegSetValueExW(key, L"ThreadingModel", 0, REG_SZ, reinterpret_cast<const BYTE*>(model), (wcslen(model) + 1) * sizeof(wchar_t));
    RegCloseKey(key);
    return S_OK;
}

inline HRESULT UnregisterComServer() {
    wchar_t clsidStr[64];
    StringFromGUID2(CLSID_GuitarwiterTextService, clsidStr, 64);
    std::wstring keyPath = std::wstring(L"CLSID\\") + clsidStr;
    // Hapus subkey InprocServer32 dulu, baru key utamanya (RegDeleteTree lebih simpel tapi butuh
    // Shlwapi/Advapi versi baru -- dipisah manual di sini supaya dependensinya minimal).
    std::wstring inprocPath = keyPath + L"\\InprocServer32";
    RegDeleteKeyW(HKEY_CLASSES_ROOT, inprocPath.c_str());
    RegDeleteKeyW(HKEY_CLASSES_ROOT, keyPath.c_str());
    return S_OK;
}

// Daftarkan sebagai TSF Text Input Processor supaya muncul di Pengaturan Bahasa Windows.
// langId: gunakan 0x0409 (English US) sebagai default netral -- keyboard ini tidak benar-benar
// tergantung bahasa (nada -> huruf latin), tapi TSF mewajibkan satu langid per profile.
inline HRESULT RegisterTsfProfile(LANGID langId = 0x0409) {
    ComPtr<ITfInputProcessorProfiles> profiles;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_ITfInputProcessorProfiles, &profiles);
    if (FAILED(hr)) return hr;

    hr = profiles->Register(CLSID_GuitarwiterTextService);
    if (FAILED(hr)) return hr;

    hr = profiles->AddLanguageProfile(
        CLSID_GuitarwiterTextService, langId, GUID_GuitarwiterProfile,
        L"Guitarwiter", static_cast<ULONG>(wcslen(L"Guitarwiter")),
        nullptr, 0, 0);
    return hr;
}

inline HRESULT UnregisterTsfProfile(LANGID langId = 0x0409) {
    ComPtr<ITfInputProcessorProfiles> profiles;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_ITfInputProcessorProfiles, &profiles);
    if (FAILED(hr)) return hr;
    profiles->RemoveLanguageProfile(CLSID_GuitarwiterTextService, langId, GUID_GuitarwiterProfile);
    return profiles->Unregister(CLSID_GuitarwiterTextService);
}

// Tandai kategori supaya Windows tahu ini text service jenis "keyboard" (bukan speech/handwriting).
inline HRESULT RegisterCategories() {
    ComPtr<ITfCategoryMgr> catMgr;
    HRESULT hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, &catMgr);
    if (FAILED(hr)) return hr;
    return catMgr->RegisterCategory(CLSID_GuitarwiterTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_GuitarwiterTextService);
}

inline HRESULT UnregisterCategories() {
    ComPtr<ITfCategoryMgr> catMgr;
    HRESULT hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, &catMgr);
    if (FAILED(hr)) return hr;
    return catMgr->UnregisterCategory(CLSID_GuitarwiterTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_GuitarwiterTextService);
}

}  // namespace guitarwiter
