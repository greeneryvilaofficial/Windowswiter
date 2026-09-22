// GuitarwiterTextService.h — kelas COM utama yang Windows panggil untuk mengaktifkan/menonaktifkan
// text service ini. Setara HtmlKeyboardService (InputMethodService) di Android: Activate() di sini
// ~= onCreateInputView(), Deactivate() ~= onDestroy().
#pragma once
#include <msctf.h>
#include <wrl/client.h>
#include <atomic>
#include "AudioCapture.h"
#include "YinPitchDetector.h"
#include "PitchMap.h"
#include "NoteKeymap.h"
#include "TextInsertion.h"
#include "KeyboardWindow.h"

namespace guitarwiter {
using Microsoft::WRL::ComPtr;

class GuitarwiterTextService : public ITfTextInputProcessor, public ITfThreadMgrEventSink {
public:
    GuitarwiterTextService() = default;
    ~GuitarwiterTextService() { StopAudio(); }

    // ---- IUnknown ----
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_INVALIDARG;
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == IID_ITfTextInputProcessor) *ppv = static_cast<ITfTextInputProcessor*>(this);
        else if (riid == IID_ITfThreadMgrEventSink) *ppv = static_cast<ITfThreadMgrEventSink*>(this);
        if (!*ppv) return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refCount_); }
    STDMETHODIMP_(ULONG) Release() override {
        LONG r = InterlockedDecrement(&refCount_);
        if (r == 0) delete this;
        return r;
    }

    // ---- ITfTextInputProcessor ----
    // Dipanggil Windows saat pengguna MEMILIH Guitarwiter sebagai input method aktif
    // (setara onCreateInputView() Android: di sinilah "keyboard" mulai hidup).
    STDMETHODIMP Activate(ITfThreadMgr* threadMgr, TfClientId clientId) override {
        threadMgr_ = threadMgr;
        clientId_ = clientId;

        HRESULT hr = threadMgr_->QueryInterface(IID_ITfSource, &source_);
        if (SUCCEEDED(hr) && source_) {
            source_->AdviseSink(IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink*>(this), &threadMgrEventCookie_);
        }

        keyboardWindow_ = std::make_unique<KeyboardWindow>();
        keyboardWindow_->Create(GetModuleHandleW(nullptr), [this](int idx) { OnNoteSelected(idx); });

        StartAudio();
        return S_OK;
    }

    // Dipanggil Windows saat pengguna beralih ke input method LAIN atau menutup aplikasi
    // (setara onDestroy() Android: WAJIB melepas semua resource, terutama mic).
    STDMETHODIMP Deactivate() override {
        StopAudio();
        if (source_ && threadMgrEventCookie_ != TF_INVALID_COOKIE) {
            source_->UnadviseSink(threadMgrEventCookie_);
            threadMgrEventCookie_ = TF_INVALID_COOKIE;
        }
        keyboardWindow_.reset();
        threadMgr_.Reset();
        source_.Reset();
        return S_OK;
    }

    // ---- ITfThreadMgrEventSink: melacak kolom teks mana yang sedang fokus ----
    // Setara getCurrentInputConnection() Android -- context_ di bawah itulah "kolom aktif".
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }
    STDMETHODIMP OnSetFocus(ITfDocumentMgr* docMgrFocus, ITfDocumentMgr*) override {
        context_.Reset();
        if (docMgrFocus) docMgrFocus->GetTop(&context_);
        return S_OK;
    }
    STDMETHODIMP OnPushContext(ITfContext*) override { return S_OK; }
    STDMETHODIMP OnPopContext(ITfContext*) override { return S_OK; }

private:
    void StartAudio() {
        audio_.Start([this](const float* samples, int count, int sampleRate) {
            OnAudioSamples(samples, count, sampleRate);
        });
    }
    void StopAudio() { audio_.Stop(); }

    // Dipanggil dari THREAD AUDIO (bukan UI thread) -- kumpulkan sampel ke jendela geser lalu
    // jalankan YIN, mirip recordLoop() di NativeMicPitchDetector.java. Diringkas di sini (tanpa
    // fase onset/settle/konfirmasi penuh seperti versi Android) untuk kerangka awal; port
    // lengkapnya mengikuti pola state machine yang sama seperti di Java.
    void OnAudioSamples(const float* samples, int count, int sampleRate) {
        for (int i = 0; i < count; i++) {
            ringBuf_[ringPos_] = samples[i];
            ringPos_ = (ringPos_ + 1) % kBufferSamples;
            filled_ = std::min(filled_ + 1, kBufferSamples);
        }
        if (filled_ < kBufferSamples) return;

        int64_t now = GetTickCount64();
        if (now - lastCheck_ < 16) return;  // ~1 hop 60fps, sinkron kasar dengan hop Android
        lastCheck_ = now;

        float linear[kBufferSamples];
        for (int i = 0; i < kBufferSamples; i++) linear[i] = ringBuf_[(ringPos_ + i) % kBufferSamples];

        auto reading = YinDetect(linear, kBufferSamples, sampleRate);
        if (!reading) return;
        int idx = FreqToNoteIndex(reading->freq);
        if (idx < 0) return;

        int64_t nowMs = now;
        if (nowMs - lastNoteAt_ < 110) return;  // batas kecepatan antar nada, sinkron dengan Android
        lastNoteAt_ = nowMs;

        if (keyboardWindow_) {
            keyboardWindow_->FlashNote(idx);
            // Penyisipan teks WAJIB dari thread yang membuat window ini (TSF tidak boleh dipanggil
            // dari sembarang thread) -- PostMessage ke HWND, diproses balik lewat OnNoteSelected()
            // di WndProc KeyboardWindow, jalur yang SAMA dengan klik mouse (lihat WM_APP+2 di sana).
            keyboardWindow_->SelectNoteFromAudioThread(idx);
        }
    }

    // Dipanggil dari thread UI (lewat klik mouse ATAU pesan WM_APP+2 dari thread audio).
    void OnNoteSelected(int idx) {
        const auto& km = NoteKeymap();
        if (idx < 0 || idx >= static_cast<int>(km.size()) || !context_) return;
        const KeyDef& kd = km[idx];
        switch (kd.action) {
            case KeyAction::Letter:
            case KeyAction::Comma:
            case KeyAction::Period:
                InsertText(context_, clientId_, std::wstring(1, kd.ch));
                break;
            case KeyAction::Space:
                InsertText(context_, clientId_, L" ");
                break;
            case KeyAction::Enter:
                InsertText(context_, clientId_, L"\r");
                break;
            case KeyAction::Backspace:
                DeleteBack(context_, clientId_, 1);
                break;
            case KeyAction::Shift:
            case KeyAction::NumToggle:
            case KeyAction::EmojiToggle:
                // TODO: mode shift/simbol/emoji belum diimplementasikan di kerangka awal ini --
                // di Android ini mengubah tampilan tuts (lihat setMode() di index.html); versi
                // Windows perlu state serupa di KeyboardWindow sebelum tombol ini berfungsi penuh.
                break;
        }
    }

    LONG refCount_ = 1;
    ComPtr<ITfThreadMgr> threadMgr_;
    ComPtr<ITfSource> source_;
    ComPtr<ITfContext> context_;
    TfClientId clientId_ = TF_CLIENTID_NULL;
    DWORD threadMgrEventCookie_ = TF_INVALID_COOKIE;

    std::unique_ptr<KeyboardWindow> keyboardWindow_;
    AudioCapture audio_;
    float ringBuf_[kBufferSamples] = {};
    int ringPos_ = 0, filled_ = 0;
    int64_t lastCheck_ = 0, lastNoteAt_ = 0;
};

}  // namespace guitarwiter
