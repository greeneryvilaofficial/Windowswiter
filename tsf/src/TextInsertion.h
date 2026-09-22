// TextInsertion.h — menyisipkan teks ke dokumen yang sedang fokus lewat TSF, setara
// InputConnection.commitText() di Android. Dipanggil dari UI thread (bukan thread audio).
#pragma once
#include <msctf.h>
#include <wrl/client.h>
#include <string>

namespace guitarwiter {
using Microsoft::WRL::ComPtr;

// Callback yang TSF berikan lewat ITfEditSession::DoEditSession -- WAJIB dijalankan di dalam
// edit session (tidak bisa memanggil ITfRange langsung dari luar), mengikuti aturan TSF.
class InsertTextEditSession : public ITfEditSession {
public:
    InsertTextEditSession(ComPtr<ITfContext> context, std::wstring text)
        : context_(std::move(context)), text_(std::move(text)) {}

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_ITfEditSession) { *ppv = this; AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&ref_); }
    STDMETHODIMP_(ULONG) Release() override {
        LONG r = InterlockedDecrement(&ref_);
        if (r == 0) delete this;
        return r;
    }

    // Dipanggil TSF di dalam edit session yang sah -- di sinilah teks benar-benar disisipkan.
    STDMETHODIMP DoEditSession(TfEditCookie cookie) override {
        ComPtr<ITfInsertAtSelection> insertAtSelection;
        HRESULT hr = context_.As(&insertAtSelection);
        if (FAILED(hr)) return hr;

        ComPtr<ITfRange> range;
        hr = insertAtSelection->InsertTextAtSelection(cookie, TF_IAS_QUERYONLY, nullptr, 0, &range);
        if (FAILED(hr) || !range) return hr;

        hr = range->SetText(cookie, 0, text_.c_str(), static_cast<LONG>(text_.size()));
        if (FAILED(hr)) return hr;

        // Pindahkan kursor ke akhir teks yang baru disisipkan (persis commitText Android).
        ComPtr<ITfRange> collapsed;
        range->Clone(&collapsed);
        if (collapsed) {
            collapsed->Collapse(cookie, TF_ANCHOR_END);
            TF_SELECTION sel;
            sel.range = collapsed.Get();
            sel.style.ase = TF_AE_NONE;
            sel.style.fInterimChar = FALSE;
            context_->SetSelection(cookie, 1, &sel);
        }
        return S_OK;
    }

private:
    LONG ref_ = 1;
    ComPtr<ITfContext> context_;
    std::wstring text_;
};

// Sisipkan teks ke context yang sedang aktif. clientId WAJIB TfClientId asli milik text
// service ini (didapat dari ITfThreadMgr::Activate saat Activate() dipanggil Windows -- lihat
// GuitarwiterTextService::clientId_), BUKAN TF_CLIENTID_NULL. Mengembalikan false kalau gagal
// (mis. kolom yang di-protect / tidak menerima edit session -- ini BISA terjadi di beberapa
// aplikasi Win32 lawas yang tidak mendukung TSF penuh, mirip keterbatasan InputConnection Android).
inline bool InsertText(ComPtr<ITfContext> context, TfClientId clientId, const std::wstring& text) {
    if (!context || text.empty()) return false;
    ComPtr<InsertTextEditSession> session = new InsertTextEditSession(context, text);
    HRESULT hrSession = S_OK;
    HRESULT hr = context->RequestEditSession(clientId, session.Get(), TF_ES_SYNC | TF_ES_READWRITE, &hrSession);
    return SUCCEEDED(hr) && SUCCEEDED(hrSession);
}

// Hapus n karakter SEBELUM kursor (setara deleteSurroundingText Android). Dipakai untuk fitur Hapus.
class DeleteBackEditSession : public ITfEditSession {
public:
    DeleteBackEditSession(ComPtr<ITfContext> context, int count) : context_(std::move(context)), count_(count) {}
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_ITfEditSession) { *ppv = this; AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&ref_); }
    STDMETHODIMP_(ULONG) Release() override { LONG r = InterlockedDecrement(&ref_); if (r == 0) delete this; return r; }

    STDMETHODIMP DoEditSession(TfEditCookie cookie) override {
        ComPtr<ITfRange> sel;
        TF_SELECTION selection;
        ULONG fetched = 0;
        HRESULT hr = context_->GetSelection(cookie, TF_DEFAULT_SELECTION, 1, &selection, &fetched);
        if (FAILED(hr) || fetched == 0) return hr;
        sel.Attach(selection.range);

        LONG moved = 0;
        sel->ShiftStart(cookie, -count_, &moved, nullptr);
        sel->SetText(cookie, 0, L"", 0);
        return S_OK;
    }
private:
    LONG ref_ = 1;
    ComPtr<ITfContext> context_;
    int count_;
};


inline bool DeleteBack(ComPtr<ITfContext> context, TfClientId clientId, int count) {
    if (!context || count <= 0) return false;
    ComPtr<DeleteBackEditSession> session = new DeleteBackEditSession(context, count);
    HRESULT hrSession = S_OK;
    HRESULT hr = context->RequestEditSession(clientId, session.Get(), TF_ES_SYNC | TF_ES_READWRITE, &hrSession);
    return SUCCEEDED(hr) && SUCCEEDED(hrSession);
}

}  // namespace guitarwiter
