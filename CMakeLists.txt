# CMakeLists.txt — proyek Guitarwiter TSF (Windows Text Service).
# Build lewat: cmake -B build -A x64  &&  cmake --build build --config Release
# HANYA jalan di Windows (COM/TSF adalah API Windows, tidak ada di Linux/macOS).
cmake_minimum_required(VERSION 3.20)
project(GuitarwiterTSF LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "Proyek ini cuma bisa di-build di Windows (butuh COM/TSF/WASAPI).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(GuitarwiterTSF SHARED
  src/DllMain.cpp
)
target_include_directories(GuitarwiterTSF PRIVATE src)

# Header-only implementation classes tetap perlu didaftarkan supaya muncul di IDE/dependency
# tracking, meskipun isinya inline semua.
target_sources(GuitarwiterTSF PRIVATE
  src/Guids.h
  src/PitchMap.h
  src/YinPitchDetector.h
  src/AudioCapture.h
  src/NoteKeymap.h
  src/TextInsertion.h
  src/KeyboardWindow.h
  src/GuitarwiterTextService.h
  src/ClassFactory.h
  src/Registration.h
)

target_link_libraries(GuitarwiterTSF PRIVATE
  ole32 oleaut32 uuid    # COM dasar
  mmdevapi               # WASAPI (enumerasi device audio)
  msctf                  # Text Services Framework
)

set_target_properties(GuitarwiterTSF PROPERTIES
  OUTPUT_NAME "GuitarwiterTSF"
  SUFFIX ".dll"
)

# Definisi ekspor DLL (DllGetClassObject dkk WAJIB nama persis ini, tanpa name-mangling C++)
target_sources(GuitarwiterTSF PRIVATE src/GuitarwiterTSF.def)
