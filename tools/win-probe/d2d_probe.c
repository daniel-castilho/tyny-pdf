// d2d_probe.c - probe #1 of ADR-0010 section 2: the Direct2D/DirectWrite/D3D11/DXGI
// development surface compiles AND links against the MinGW import libraries, and the
// factories are creatable through the WSL interop path.
//
// Pass/fail semantics: the probe exits 0 when the Direct2D and DirectWrite factories
// create successfully, which is the clause that needed measuring ("development headers
// available for the cross target", "import libraries link"). D3D11/DXGI specifics are
// printed for the record but their outcome is gpu_probe's, not this probe's.
//
// This file is Windows-only; it is compiled by win-cross-x64 and windows-msvc only.

#include <d2d1_3.h>
#include <d3d11.h>
#include <dwrite.h>
#include <dwrite_1.h>
#include <dwrite_2.h>
#include <dwrite_3.h>
#include <dxgi1_6.h>
#include <stdio.h>
#include <windows.h>

#ifndef PC_FAILED
#define PC_FAILED(hr) ((HRESULT)(hr) < 0)
#endif

#ifndef PC_SUCCEEDED
#define PC_SUCCEEDED(hr) ((HRESULT)(hr) >= 0)
#endif

static void print_hr(const char* step, HRESULT hr) {
  printf("d2d_probe: %-28s 0x%08lX%s\n", step, (unsigned long)hr,
         PC_FAILED(hr) ? "  FAIL" : "  ok");
}

int main(void) {
  HRESULT hr;

  ID2D1Factory7* d2d = NULL;
  hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory7), NULL,
                         (void**)&d2d);
  print_hr("D2D1CreateFactory", hr);
  if (PC_FAILED(hr) || d2d == NULL) {
    return 1;
  }

  IDWriteFactory7* dwrite = NULL;
  hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory7),
                           (IUnknown**)&dwrite);
  print_hr("DWriteCreateFactory", hr);
  if (PC_FAILED(hr) || dwrite == NULL) {
    d2d->Release();
    return 1;
  }

  ID3D11Device* device = NULL;
  D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
  const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0,
                                      D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
  hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                         levels, 4, D3D11_SDK_VERSION, &device, &feature_level, NULL);
  print_hr("D3D11CreateDevice(hardware)", hr);
  if (PC_FAILED(hr) && device != NULL) {
    device->Release();
  }

  IDXGIFactory6* dxgi = NULL;
  hr = CreateDXGIFactory1(__uuidof(IDXGIFactory6), (void**)&dxgi);
  print_hr("CreateDXGIFactory1", hr);
  if (PC_FAILED(hr) || dxgi == NULL) {
    dwrite->Release();
    d2d->Release();
    return 1;
  }

  IDXGIAdapter1* adapter = NULL;
  hr = dxgi->EnumAdapters1(0, &adapter);
  if (PC_SUCCEEDED(hr) && adapter != NULL) {
    DXGI_ADAPTER_DESC1 desc;
    adapter->GetDesc1(&desc);
    printf("d2d_probe: adapter 0 vendor=0x%04X device=0x%04X dedicated=%llu MB flags=0x%X\n",
           desc.VendorId, desc.DeviceId,
           (unsigned long long)(desc.DedicatedVideoMemory / (1024ULL * 1024ULL)),
           (unsigned)desc.Flags);
    adapter->Release();
  } else {
    printf("d2d_probe: no adapter enumerated (hr=0x%08lX)\n", (unsigned long)hr);
  }

  d2d->Release();
  if (device != NULL) {
    device->Release();
  }
  dxgi->Release();
  dwrite->Release();
  printf("d2d_probe: PASS (D2D factory ok, DWrite factory ok)\n");
  return 0;
}