// NoteKeymap.h — 44 slot nada -> aksi, SALINAN PERSIS dari KEY_DEFS di index.html (urutan dan
// isinya harus tetap sama supaya nada yang sama menghasilkan huruf yang sama di Android & Windows).
#pragma once
#include <vector>
#include <string>

namespace guitarwiter {

enum class KeyAction { Letter, Shift, Backspace, NumToggle, Comma, EmojiToggle, Space, Period, Enter };

struct KeyDef {
    KeyAction action;
    wchar_t ch = 0;   // hanya untuk Letter/Comma/Period
};

// HARUS sinkron dengan KEY_DEFS di index.html -- lihat komentar di sana kalau perlu diubah.
inline const std::vector<KeyDef>& NoteKeymap() {
    static const std::vector<KeyDef> kMap = [] {
        std::vector<KeyDef> v;
        for (wchar_t c : std::wstring(L"1234567890")) v.push_back({KeyAction::Letter, c});
        for (wchar_t c : std::wstring(L"qwertyuiop")) v.push_back({KeyAction::Letter, c});
        for (wchar_t c : std::wstring(L"asdfghjkl")) v.push_back({KeyAction::Letter, c});
        v.push_back({KeyAction::Shift});
        for (wchar_t c : std::wstring(L"zxcvbnm")) v.push_back({KeyAction::Letter, c});
        v.push_back({KeyAction::Backspace});
        v.push_back({KeyAction::NumToggle});
        v.push_back({KeyAction::Comma, L','});
        v.push_back({KeyAction::EmojiToggle});
        v.push_back({KeyAction::Space});
        v.push_back({KeyAction::Period, L'.'});
        v.push_back({KeyAction::Enter});
        return v;
    }();
    return kMap;
}
constexpr int kNoteKeymapSize = 44;  // HARUS sama dengan NoteKeymap().size() -- dicek statis_assert di .cpp

}  // namespace guitarwiter
