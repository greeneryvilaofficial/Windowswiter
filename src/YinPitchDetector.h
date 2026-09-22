// YinPitchDetector.h — deteksi pitch algoritma YIN + verifikasi oktaf Goertzel.
// PORTING LANGSUNG dari yinDetect()/goertzelMag() di NativeMicPitchDetector.java (Android).
// Logikanya sengaja dibuat SAMA PERSIS (bukan "versi C++ yang lebih efisien") supaya perilaku
// deteksi nada di Windows konsisten dengan versi Android yang sudah diuji.
#pragma once
#include <vector>
#include <optional>
#include <cmath>
#include <algorithm>
#include "PitchMap.h"

namespace guitarwiter {

constexpr double kPi = 3.14159265358979323846;

struct PitchReading {
    double freq;
    double probability;
};

inline double GoertzelMag(const float* buf, int size, double targetFreq, int sampleRate) {
    int k = static_cast<int>(std::lround(size * targetFreq / sampleRate));
    double w = 2.0 * kPi * k / size;
    double cosine = std::cos(w), sine = std::sin(w), coeff = 2.0 * cosine;
    double q0 = 0, q1 = 0, q2 = 0;
    for (int n = 0; n < size; n++) {
        q0 = coeff * q1 - q2 + buf[n];
        q2 = q1;
        q1 = q0;
    }
    double real = q1 - q2 * cosine;
    double imag = q2 * sine;
    return std::sqrt(real * real + imag * imag) / size;
}

// buf: sampel PCM float [-1,1], size == kBufferSamples. Return nullopt kalau tidak ada nada jelas.
inline std::optional<PitchReading> YinDetect(const float* buf, int size, int sampleRate) {
    double rms = 0;
    for (int i = 0; i < size; i++) rms += static_cast<double>(buf[i]) * buf[i];
    rms = std::sqrt(rms / size);
    if (rms < kYinMinRms) return std::nullopt;

    int minTau = std::max(2, static_cast<int>(std::floor(sampleRate / MaxValidFreq())));
    int maxTau = std::min(size / 2 - 1, static_cast<int>(std::ceil(sampleRate / MinValidFreq())));
    if (maxTau <= minTau) return std::nullopt;

    // Langkah 1: fungsi selisih d(tau)
    std::vector<double> diff(maxTau + 1, 0.0);
    for (int tau = minTau; tau <= maxTau; tau++) {
        double sum = 0;
        for (int j = 0; j < size - maxTau; j++) {
            double d = buf[j] - buf[j + tau];
            sum += d * d;
        }
        diff[tau] = sum;
    }

    // Langkah 2: cumulative mean normalized difference function (CMNDF)
    std::vector<double> cmnd(maxTau + 1, 0.0);
    double runningSum = 0;
    cmnd[minTau] = 1;
    for (int tau = minTau + 1; tau <= maxTau; tau++) {
        runningSum += diff[tau];
        cmnd[tau] = diff[tau] * (tau - minTau) / (runningSum != 0 ? runningSum : 1e-9);
    }

    // Langkah 3: cari tau terkecil di bawah ambang (menghindari salah pilih oktaf ke bawah)
    int tauEstimate = -1;
    for (int tau = minTau + 1; tau <= maxTau; tau++) {
        if (cmnd[tau] < kYinThreshold) {
            while (tau + 1 <= maxTau && cmnd[tau + 1] < cmnd[tau]) tau++;
            tauEstimate = tau;
            break;
        }
    }
    if (tauEstimate == -1) return std::nullopt;

    // Langkah 4: interpolasi parabola
    int x0 = tauEstimate > minTau ? tauEstimate - 1 : tauEstimate;
    int x2 = tauEstimate < maxTau ? tauEstimate + 1 : tauEstimate;
    double betterTau = tauEstimate;
    if (x0 != tauEstimate && x2 != tauEstimate) {
        double s0 = cmnd[x0], s1 = cmnd[tauEstimate], s2 = cmnd[x2];
        double denom = 2 * (2 * s1 - s2 - s0);
        if (denom != 0) betterTau = tauEstimate + (s2 - s0) / denom;
    }
    if (betterTau <= 0) return std::nullopt;

    double rawFreq = sampleRate / betterTau;
    double probability = 1 - cmnd[tauEstimate];

    // Langkah 5: verifikasi oktaf lewat energi spektral Goertzel (lihat komentar panjang
    // di NativeMicPitchDetector.java untuk alasan lengkapnya -- disingkat di sini).
    double finalFreq = rawFreq;
    double magAtFreq = GoertzelMag(buf, size, rawFreq, sampleRate);
    double halfFreq = rawFreq / 2;
    if (halfFreq >= MinValidFreq()) {
        double magAtHalf = GoertzelMag(buf, size, halfFreq, sampleRate);
        if (magAtHalf >= magAtFreq * 0.6) finalFreq = halfFreq;
    }

    return PitchReading{finalFreq, probability};
}

}  // namespace guitarwiter
