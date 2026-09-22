# Guitarwiter TSF — versi Windows (Text Services Framework)

## STATUS: kerangka awal, BELUM PERNAH DIKOMPILASI

Kode ini ditulis tanpa akses ke Windows/MSVC sama sekali — tidak ada satu baris pun yang sudah
diverifikasi lewat kompilasi nyata sebelum kamu jalankan sendiri. Semua pemeriksaan yang bisa
dilakukan tanpa Windows sudah dijalankan (keseimbangan kurung, kecocokan pemanggilan fungsi antar
file, include yang lengkap, dan satu bug nyata soal lintas-thread messaging sudah ditemukan &
diperbaiki lewat review manual) — tapi itu bukan pengganti kompilasi sungguhan.

**Cara verifikasi sebenarnya:** push folder `tsf/` ini ke repo GitHub yang sudah ada workflow
`.github/workflows/windows-build.yml`. GitHub Actions akan menjalankan compiler MSVC asli di
runner `windows-latest`. Kalau build gagal, log error compiler itulah kebenaran yang sesungguhnya
— jauh lebih bisa dipercaya daripada dugaan siapa pun tanpa kompilasi.

## Apa yang SUDAH ada
- Text service TSF terdaftar (`ITfTextInputProcessor`) — Activate/Deactivate mengikuti kontrak resmi Microsoft
- Registrasi COM + TSF profile (`DllRegisterServer`/`DllUnregisterServer`, dipanggil lewat `regsvr32.exe`)
- Deteksi nada: algoritma YIN + verifikasi oktaf Goertzel — **porting baris demi baris** dari
  `NativeMicPitchDetector.java` (Android), termasuk semua konstanta (`YIN_THRESHOLD`, `SETTLE_MS`, dst)
- Tangkap audio lewat WASAPI (setara `AudioRecord` Android)
- 44 slot nada → huruf, **identik** dengan `KEY_DEFS` di `index.html`
- Penyisipan/penghapusan teks ke kolom aktif lewat `ITfInsertAtSelection` / `ITfRange`
- Panel keyboard mengambang (Win32 native, bukan WebView — supaya tidak menambah dependensi WebView2)

## Apa yang BELUM ada (dibanding versi Android)
- **Mode Shift / Simbol / Emoji** — tombolnya ada di layout tapi belum diimplementasikan (lihat `TODO` di `GuitarwiterTextService::OnNoteSelected`)
- **State machine onset/settle/konfirmasi** seperti di `NativeMicPitchDetector.java` — versi ini cuma jalankan YIN tiap ~16ms tanpa fase "tunggu petikan reda dulu", jadi kemungkinan salah-deteksi lebih tinggi daripada Android
- Saran kata, kamus pribadi, bahasa non-Latin, papan klip, dll — semua fitur besar di `index.html` belum di-porting
- Tampilan panel masih kotak abu-abu sederhana, bukan replika visual Gboard seperti Android
- **Panel ini floating, BUKAN muncul di Windows Input Method picker** sebagai keyboard yang bisa
  "dipilih" seperti keyboard Cina/Jepang biasa — dia terdaftar sebagai TIP tapi UI-nya jendela
  terpisah, bukan `ITfLangBarItemButton` custom candidate window

## Cara build manual (kalau tidak lewat GitHub Actions)
Butuh Windows + Visual Studio 2022 (atau Build Tools) dengan workload "Desktop development with C++":
```
cmake -B build -A x64
cmake --build build --config Release
```
Hasilnya: `build/Release/GuitarwiterTSF.dll`

## Cara install (WAJIB run as Administrator)
```
regsvr32 GuitarwiterTSF.dll
```
Lalu buka **Settings → Time & language → Language & region → [English/bahasa apa pun] → Language
options → Add a keyboard**, dan cari "Guitarwiter" di daftar.

## Cara uninstall
```
regsvr32 /u GuitarwiterTSF.dll
```

## SEBELUM rilis pertama — WAJIB dilakukan
GUID di `src/Guids.h` di-generate acak oleh AI (bukan lewat `guidgen.exe`/`New-Guid` Windows asli
karena saya tidak punya akses Windows). **Generate ulang GUID-nya dulu** pakai PowerShell di
Windows asli:
```powershell
[guid]::NewGuid()
```
Ganti tiga nilai `DEFINE_GUID` di `Guids.h`, build ulang, baru rilis ke pengguna. Setelah rilis
pertama, GUID ini TIDAK BOLEH diubah lagi (sama seperti keystore Android — kalau berubah, Windows
menganggapnya text service baru).

## Risiko yang perlu kamu tahu
- **Antivirus mungkin menandai ini mencurigakan.** Text service yang menyisipkan teks ke aplikasi
  lain + mengakses mikrofon adalah pola yang mirip malware, meski maksudnya legit. Kemungkinan
  perlu code-signing certificate resmi (berbayar) supaya SmartScreen/Defender tidak memblokir.
- **Uninstall paksa via registry manual** mungkin diperlukan kalau `regsvr32 /u` gagal karena DLL
  sedang dipakai proses lain — ini risiko umum semua TSF, bukan spesifik ke kode ini.
- **Ini kerangka, bukan produk jadi.** Wajar kalau perlu beberapa putaran debug sebelum benar-benar
  jalan mulus — TSF terkenal sebagai salah satu API Windows paling sulit di-debug pertama kali.
