// KeyboardWindow.h — jendela panel keyboard Win32 native (44 tombol persegi, label nada di
// bawah tiap huruf -- setara tampilan index.html tapi digambar langsung pakai GDI, BUKAN WebView,
// supaya tidak menambah dependensi WebView2 Runtime/SDK di atas proyek yang sudah rumit ini).
//
// CATATAN JUJUR: window ini floating & topmost (WS_EX_TOPMOST), mengambang di atas aplikasi lain
// seperti On-Screen Keyboard bawaan Windows -- BUKAN menempel di sistem input method picker
// Windows sungguhan (itu jauh lebih kompleks, butuh ITfLangBarItemButton custom UI). Klik tombol
// di sini memicu callback yang sama dengan jalur deteksi nada gitar.
#pragma once
#include <windows.h>
#include <windowsx.h>
#include <string>
#include <vector>
#include <functional>
#include "NoteKeymap.h"

namespace guitarwiter {

// Callback saat pengguna (via klik mouse ATAU petikan gitar) memicu satu slot nada.
using KeySelectCallback = std::function<void(int noteIndex)>;

class KeyboardWindow {
public:
    bool Create(HINSTANCE hInst, KeySelectCallback onSelect) {
        onSelect_ = std::move(onSelect);
        WNDCLASSEXW wc{ sizeof(wc) };
        wc.lpfnWndProc = &KeyboardWindow::WndProcStatic;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(RGB(0x22, 0x31, 0x34));  // sama dengan --kb-bg di index.html
        wc.lpszClassName = kClassName;
        if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

        hwnd_ = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kClassName, L"Guitarwiter",
            WS_POPUP | WS_VISIBLE, 100, 100, kWinWidth, kWinHeight,
            nullptr, nullptr, hInst, this);
        return hwnd_ != nullptr;
    }

    void Show(bool show) { if (hwnd_) ShowWindow(hwnd_, show ? SW_SHOW : SW_HIDE); }
    HWND Handle() const { return hwnd_; }

    // Dipanggil dari thread audio (lewat PostMessage supaya aman lintas-thread) saat nada terdeteksi,
    // supaya tombolnya "flash" seperti tuts Android saat dipetik.
    void FlashNote(int idx) { if (hwnd_) PostMessageW(hwnd_, WM_APP + 1, static_cast<WPARAM>(idx), 0); }

    // Dipanggil dari THREAD AUDIO (bukan UI thread) saat nada terdeteksi -- PostMessage ke HWND
    // aman dipanggil lintas-thread (beda dengan PostThreadMessage yang butuh message loop
    // khusus yang tidak kita punya). Pesan ini diproses di thread yang MEMBUAT window ini, yaitu
    // thread yang menjalankan message loop aplikasi HOST (mis. notepad.exe, chrome.exe) -- TSF
    // di-load in-process ke situ, jadi message loop-nya numpang pada aplikasi yang sedang dipakai.
    void SelectNoteFromAudioThread(int idx) { if (hwnd_) PostMessageW(hwnd_, WM_APP + 2, static_cast<WPARAM>(idx), 0); }

private:
    static constexpr wchar_t kClassName[] = L"GuitarwiterKeyboardWindow";
    static constexpr int kCols = 10, kKeyW = 46, kKeyH = 46, kGap = 4, kMargin = 8;
    static constexpr int kWinWidth = kMargin * 2 + kCols * (kKeyW + kGap);
    static constexpr int kWinHeight = kMargin * 2 + 5 * (kKeyH + kGap) + 24;  // +24 label judul

    struct Layout { RECT r; int idx; };
    std::vector<Layout> buttons_;
    int flashIdx_ = -1;
    HWND hwnd_ = nullptr;
    KeySelectCallback onSelect_;

    void BuildLayout() {
        buttons_.clear();
        const auto& km = NoteKeymap();
        for (int i = 0; i < static_cast<int>(km.size()); i++) {
            int row = i / kCols, col = i % kCols;
            RECT r{ kMargin + col * (kKeyW + kGap), kMargin + 24 + row * (kKeyH + kGap),
                    kMargin + col * (kKeyW + kGap) + kKeyW, kMargin + 24 + row * (kKeyH + kGap) + kKeyH };
            buttons_.push_back({ r, i });
        }
    }

    static const wchar_t* NoteLabel(int idx) {
        // E2..(E2+43): label sederhana "idx" cukup untuk debug awal; peta nama nada penuh
        // (E2, F2, F#2, ...) ada di PitchMap.h kalau mau ditambahkan tampilannya nanti.
        static wchar_t buf[8];
        swprintf_s(buf, L"%d", idx);
        return buf;
    }

    void OnPaint(HDC hdcWin) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd_, &ps);
        RECT client; GetClientRect(hwnd_, &client);
        HBRUSH bg = CreateSolidBrush(RGB(0x22, 0x31, 0x34));
        FillRect(hdc, &client, bg);
        DeleteObject(bg);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0xFD, 0xFF, 0xFC));
        RECT title{ kMargin, 2, client.right - kMargin, 22 };
        DrawTextW(hdc, L"Guitarwiter", -1, &title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        HFONT font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                  DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HFONT old = static_cast<HFONT>(SelectObject(hdc, font));

        const auto& km = NoteKeymap();
        for (const auto& b : buttons_) {
            bool flashed = (b.idx == flashIdx_);
            HBRUSH keyBrush = CreateSolidBrush(flashed ? RGB(0x56, 0x64, 0x68) : RGB(0x3D, 0x49, 0x4D));
            RoundRect(hdc, b.r.left, b.r.top, b.r.right, b.r.bottom, 8, 8);
            FillRect(hdc, &b.r, keyBrush);
            DeleteObject(keyBrush);
            FrameRect(hdc, &b.r, static_cast<HBRUSH>(GetStockObject(NULL_BRUSH)));

            std::wstring label;
            const KeyDef& kd = km[b.idx];
            switch (kd.action) {
                case KeyAction::Letter: case KeyAction::Comma: case KeyAction::Period: label = kd.ch; break;
                case KeyAction::Shift: label = L"^"; break;
                case KeyAction::Backspace: label = L"<-"; break;
                case KeyAction::Space: label = L"___"; break;
                case KeyAction::Enter: label = L"OK"; break;
                default: label = L"#"; break;
            }
            RECT tr = b.r;
            DrawTextW(hdc, label.c_str(), -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        SelectObject(hdc, old);
        DeleteObject(font);
        EndPaint(hwnd_, &ps);
    }

    int HitTest(int x, int y) const {
        for (const auto& b : buttons_) {
            if (x >= b.r.left && x < b.r.right && y >= b.r.top && y < b.r.bottom) return b.idx;
        }
        return -1;
    }

    LRESULT WndProc(UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
            case WM_CREATE: BuildLayout(); return 0;
            case WM_PAINT: OnPaint(nullptr); return 0;
            case WM_LBUTTONDOWN: {
                int idx = HitTest(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
                if (idx >= 0 && onSelect_) onSelect_(idx);
                return 0;
            }
            case WM_APP + 1:  // FlashNote, dikirim dari thread audio lewat PostMessage (aman lintas-thread)
                flashIdx_ = static_cast<int>(wp);
                InvalidateRect(hwnd_, nullptr, FALSE);
                SetTimer(hwnd_, 1, 90, nullptr);
                return 0;
            case WM_TIMER:
                KillTimer(hwnd_, 1);
                flashIdx_ = -1;
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            case WM_APP + 2: {   // nada dari thread audio -> proses di thread UI (jalur sama dengan klik mouse)
                int idx = static_cast<int>(wp);
                if (onSelect_) onSelect_(idx);
                return 0;
            }
            case WM_DESTROY: hwnd_ = nullptr; return 0;
        }
        return DefWindowProcW(hwnd_, msg, wp, lp);
    }

    static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        KeyboardWindow* self;
        if (msg == WM_NCCREATE) {
            auto cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            self = static_cast<KeyboardWindow*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->hwnd_ = hwnd;
        } else {
            self = reinterpret_cast<KeyboardWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }
        return self ? self->WndProc(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
    }
};

}  // namespace guitarwiter
