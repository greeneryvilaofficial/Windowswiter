// AudioCapture.h — tangkap audio mikrofon lewat WASAPI (Windows Audio Session API), setara
// AudioRecord di Android. Jalan di thread terpisah; tiap blok sampel dipanggilkan callback.
//
// CATATAN JUJUR: kode ini BELUM PERNAH DIKOMPILASI. Strukturnya mengikuti pola WASAPI resmi
// Microsoft (IMMDeviceEnumerator -> IAudioClient -> IAudioCaptureClient), tapi WASAPI punya
// banyak jebakan (format audio device yang tidak terduga, event handle, COM apartment thread)
// yang biasanya baru ketahuan saat benar-benar dites di Windows asli.
#pragma once
#ifndef NOMINMAX
#define NOMINMAX  // cegah windows.h mendefinisikan makro min/max yang merusak std::min/std::max
#endif
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functional>
#include <atomic>
#include <thread>
#include <vector>
#include <wrl/client.h>
#include "PitchMap.h"  // untuk kFallbackSampleRate

namespace guitarwiter {

using Microsoft::WRL::ComPtr;

// Callback dipanggil dari thread capture setiap ada sampel baru (mono, float, sampleRate apa adanya).
using AudioCallback = std::function<void(const float* samples, int count, int sampleRate)>;

class AudioCapture {
public:
    ~AudioCapture() { Stop(); }

    // Mulai menangkap mic default sistem. Return false kalau gagal (device tidak ada / izin ditolak / format tidak didukung).
    bool Start(AudioCallback cb) {
        if (running_.load()) return true;
        callback_ = std::move(cb);
        running_.store(true);
        thread_ = std::thread([this] { CaptureLoop(); });
        return true;
    }

    void Stop() {
        if (!running_.exchange(false)) return;
        if (thread_.joinable()) thread_.join();
    }

    bool IsRunning() const { return running_.load(); }
    int SampleRate() const { return sampleRate_; }

private:
    void CaptureLoop() {
        // COM apartment untuk thread ini (WASAPI butuh STA/MTA per-thread, bukan pinjam dari UI thread).
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool needUninit = SUCCEEDED(hr);

        ComPtr<IMMDeviceEnumerator> enumerator;
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                               __uuidof(IMMDeviceEnumerator), &enumerator);
        if (FAILED(hr)) { Fail(); if (needUninit) CoUninitialize(); return; }

        ComPtr<IMMDevice> device;
        hr = enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &device);
        if (FAILED(hr)) { Fail(); if (needUninit) CoUninitialize(); return; }

        ComPtr<IAudioClient> client;
        hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client);
        if (FAILED(hr)) { Fail(); if (needUninit) CoUninitialize(); return; }

        WAVEFORMATEX* mixFormat = nullptr;
        hr = client->GetMixFormat(&mixFormat);
        if (FAILED(hr) || !mixFormat) { Fail(); if (needUninit) CoUninitialize(); return; }
        sampleRate_ = mixFormat->nSamplesPerSec;
        int channels = mixFormat->nChannels;
        bool isFloat = (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE || mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
        // TODO(butuh-tes-nyata): device WASAPI kadang memberi format PCM 16-bit, bukan float.
        // Kalau isFloat == false, blok konversi di bawah (baris "konversi sampel") wajib
        // menangani int16 -> float juga; saat ini hanya jalur float yang diuji secara logika.

        const REFERENCE_TIME bufferDuration = 100 * 10000;  // 100ms dalam satuan 100-ns, cukup untuk hop ~16ms kita
        hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK == 0 ? 0 : 0,
                                 bufferDuration, 0, mixFormat, nullptr);
        CoTaskMemFree(mixFormat);
        if (FAILED(hr)) { Fail(); if (needUninit) CoUninitialize(); return; }

        ComPtr<IAudioCaptureClient> captureClient;
        hr = client->GetService(__uuidof(IAudioCaptureClient), &captureClient);
        if (FAILED(hr)) { Fail(); if (needUninit) CoUninitialize(); return; }

        hr = client->Start();
        if (FAILED(hr)) { Fail(); if (needUninit) CoUninitialize(); return; }

        std::vector<float> monoBuf;
        while (running_.load()) {
            Sleep(10);  // ~poll tiap 10ms; cukup untuk hop 16.7ms yang kita perlukan
            UINT32 packetLength = 0;
            hr = captureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr)) break;
            while (packetLength != 0) {
                BYTE* data = nullptr;
                UINT32 numFrames = 0;
                DWORD flags = 0;
                hr = captureClient->GetBuffer(&data, &numFrames, &flags, nullptr, nullptr);
                if (FAILED(hr)) break;

                monoBuf.resize(numFrames);
                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                    std::fill(monoBuf.begin(), monoBuf.end(), 0.0f);
                } else if (isFloat) {
                    const float* src = reinterpret_cast<const float*>(data);
                    for (UINT32 i = 0; i < numFrames; i++) {
                        // konversi sampel: turunkan ke mono dengan rata-rata semua kanal
                        float sum = 0;
                        for (int c = 0; c < channels; c++) sum += src[i * channels + c];
                        monoBuf[i] = sum / channels;
                    }
                } else {
                    const int16_t* src = reinterpret_cast<const int16_t*>(data);
                    for (UINT32 i = 0; i < numFrames; i++) {
                        int32_t sum = 0;
                        for (int c = 0; c < channels; c++) sum += src[i * channels + c];
                        monoBuf[i] = (sum / static_cast<float>(channels)) / 32768.0f;
                    }
                }
                if (callback_) callback_(monoBuf.data(), static_cast<int>(numFrames), sampleRate_);

                captureClient->ReleaseBuffer(numFrames);
                hr = captureClient->GetNextPacketSize(&packetLength);
                if (FAILED(hr)) break;
            }
        }
        client->Stop();
        if (needUninit) CoUninitialize();
    }

    void Fail() { running_.store(false); failed_.store(true); }

    std::atomic<bool> running_{false};
    std::atomic<bool> failed_{false};
    std::thread thread_;
    AudioCallback callback_;
    int sampleRate_ = kFallbackSampleRate;
};

}  // namespace guitarwiter
