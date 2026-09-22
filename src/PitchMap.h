// PitchMap.h — pemetaan nada gitar -> indeks tuts, SINKRON dengan NativeMicPitchDetector.java
// dan index.html (BASE_MIDI, NOTE_COUNT, ambang, dsb). Kalau versi Android diubah, ubah juga di sini.
#pragma once
#include <cmath>
#include <cstdint>

namespace guitarwiter {

// ---- Sinkron dengan NativeMicPitchDetector.java ----
constexpr int kBaseMidi = 40;      // E2, tuts index 0
constexpr int kNoteCount = 44;     // HARUS sama dengan NOTE_COUNT di Java/JS
constexpr double kYinThreshold = 0.15;
constexpr double kYinMinRms = 0.007;
constexpr double kOnsetRms = 0.012;
constexpr double kOnsetRatio = 1.6;
constexpr int kSettleMs = 30;
constexpr int kMaxSampleTries = 5;
constexpr int kBufferSamples = 4096;
constexpr int kFallbackSampleRate = 44100;

inline double MidiToFreq(double midi) { return 440.0 * std::pow(2.0, (midi - 69.0) / 12.0); }
inline double FreqToMidi(double freq) { return 69.0 + 12.0 * std::log2(freq / 440.0); }

inline double MinValidFreq() { return MidiToFreq(kBaseMidi - 3); }
inline double MaxValidFreq() { return MidiToFreq(kBaseMidi + kNoteCount - 1 + 3); }

// Frekuensi -> indeks tuts terdekat (0..kNoteCount-1), atau -1 kalau di luar jangkauan / terlalu jauh dari nada terdekat.
// Toleransi +-45 cent (setengah jarak antar semitone), sinkron dengan indexOfClosest() di Java.
inline int FreqToNoteIndex(double freq) {
    if (freq <= 0) return -1;
    double midiOffset = FreqToMidi(freq) - kBaseMidi;
    int idx = static_cast<int>(std::lround(midiOffset));
    if (idx < 0 || idx >= kNoteCount) return -1;
    double cents = (midiOffset - idx) * 100.0;
    if (std::abs(cents) > 48.0) return -1;   // terlalu jauh dari nada terdekat -> dianggap tidak jelas
    return idx;
}

}  // namespace guitarwiter
