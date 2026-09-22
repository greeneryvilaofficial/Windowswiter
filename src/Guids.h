// Guids.h — identitas unik Text Service ini di Windows. JANGAN diubah setelah dirilis:
// Windows memakai CLSID & GUID profile ini untuk mengenali "Guitarwiter" sebagai text service
// yang sama antar update. Kalau berubah, Windows akan menganggapnya text service baru dan
// pengguna lama harus menambahkannya ulang dari Pengaturan.
//
// GUID di bawah ini di-generate acak (bukan dari `uuidgen` Windows karena tidak tersedia di
// sandbox ini) -- SEBELUM rilis pertama, generate ulang pakai `guidgen.exe` (masuk Visual Studio)
// atau powershell `[guid]::NewGuid()` di Windows asli, lalu ganti nilai di bawah. Yang penting
// hanya SEKALI ganti sebelum rilis pertama, sesudah itu jangan diubah lagi.
#pragma once
#include <initguid.h>

// CLSID kelas COM utama (ITfTextInputProcessor)
// {2F1A9E3C-8B6D-4C7A-9E2F-6A1D3B5C7E9F}
DEFINE_GUID(CLSID_GuitarwiterTextService,
    0x2f1a9e3c, 0x8b6d, 0x4c7a, 0x9e, 0x2f, 0x6a, 0x1d, 0x3b, 0x5c, 0x7e, 0x9f);

// GUID profile bahasa/input (dipakai ITfInputProcessorProfiles::Register)
// {6D4E8F1B-3A7C-4E9D-8F2A-1C6B4D8E3F7A}
DEFINE_GUID(GUID_GuitarwiterProfile,
    0x6d4e8f1b, 0x3a7c, 0x4e9d, 0x8f, 0x2a, 0x1c, 0x6b, 0x4d, 0x8e, 0x3f, 0x7a);

// LangBar item (tombol di language bar untuk buka/tutup panel)
// {9A3C5E7D-1B4F-4A6E-9D3C-5E7A1B4F6A9D}
DEFINE_GUID(GUID_GuitarwiterLangBarItem,
    0x9a3c5e7d, 0x1b4f, 0x4a6e, 0x9d, 0x3c, 0x5e, 0x7a, 0x1b, 0x4f, 0x6a, 0x9d);
