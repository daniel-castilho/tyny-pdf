// DPI module for Tyny PDF - Per-Monitor V2 process awareness and monitor
// scale lookup (story 5.4). No engine include (ADR-0011 R-M10); every
// Windows type stays inside src/os/win32.

#ifndef TYNYPDF_OS_WIN32_DPI_H_
#define TYNYPDF_OS_WIN32_DPI_H_

struct HMONITOR__;

namespace tynypdf {
namespace win32 {

// Declare Per-Monitor V2 awareness for the process. Must run before any
// window is created or WM_DPICHANGED never arrives. Returns false when the
// user32 export is missing (pre-Win10 1703); the caller keeps running
// system-DPI-aware rather than failing the process.
bool set_dpi_awareness_pm2();

// Effective DPI of the monitor as a scale factor (1.0 = 96 DPI).
float get_scale_for_monitor(HMONITOR__* monitor);

// Story 5.4 DPI purity check: renders the test pattern at 1.5x and 2.0x two
// ways each - a scale transform over logical geometry ("rendered at 150%")
// and pre-multiplied geometry at the scaled size ("asked at scaled size") -
// and requires the bytes to match. Writes dpi-150.png / dpi-200.png (the
// rendered-at-scale frame) into out_dir as the approval goldens and prints
// one PASS/FAIL line per scale. Returns false on any mismatch.
bool dpi_selftest(const char* out_dir);

}  // namespace win32
}  // namespace tynypdf

#endif  // TYNYPDF_OS_WIN32_DPI_H_
