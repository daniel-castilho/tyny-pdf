// gpu_probe.cpp - probe #4 of ADR-0010 section 2: whether this machine can offer a
// hardware adapter through the WSL2 interop path (DXGI enumeration) and at which
// Direct3D feature level a device actually creates.
//
// Semantics: exit 0 iff at least one non-software adapter exists AND D3D11CreateDevice
// succeeds against it. On a machine with only the software rasterizer the probe exits 2,
// which is a signal to amend ADR-0010's GPU story (the docs record the hardware path as
// expected, not demonstrated, until this probe prints a hardware adapter).
//
// Windows-only; compiled by win-cross-x64 and windows-msvc, executed from the WSL side
// through interop.

#include <d3d11.h>
#include <dxgi1_6.h>
#include <stdio.h>
#include <windows.h>

int main(void) {
  IDXGIFactory6* factory = NULL;
  HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory6), (void**)&factory);
  if (FAILED(hr) || factory == NULL) {
    printf("gpu_probe: CreateDXGIFactory1 failed 0x%08lX\n", (unsigned long)hr);
    return 1;
  }

  IDXGIAdapter1* hardware = NULL;
  for (UINT i = 0;; ++i) {
    IDXGIAdapter1* adapter = NULL;
    hr = factory->EnumAdapters1(i, &adapter);
    if (hr == DXGI_ERROR_NOT_FOUND) {
      break;
    }
    if (FAILED(hr)) {
      printf("gpu_probe: EnumAdapters1(%u) failed 0x%08lX\n", i, (unsigned long)hr);
      break;
    }
    DXGI_ADAPTER_DESC1 desc;
    adapter->GetDesc1(&desc);
    const int software = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
    printf(
        "gpu_probe: adapter %u vendor=0x%04X device=0x%04X type=%s dedicated=%llu MB "
        "luid=%08lX-%08lX\n",
        i, desc.VendorId, desc.DeviceId, software ? "software" : "hardware",
        (unsigned long long)(desc.DedicatedVideoMemory / (1024ULL * 1024ULL)),
        (unsigned long)desc.AdapterLuid.HighPart, (unsigned long)desc.AdapterLuid.LowPart);
    if (!software && hardware == NULL) {
      hardware = adapter;
      adapter->AddRef();
    }
    adapter->Release();
  }

  if (hardware == NULL) {
    factory->Release();
    printf("gpu_probe: FAIL - no hardware adapter; only software rasterization\n");
    return 2;
  }

  D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
  ID3D11Device* device = NULL;
  hr = D3D11CreateDevice(hardware, D3D_DRIVER_TYPE_UNKNOWN, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                         NULL, 0, D3D11_SDK_VERSION, &device, &feature_level, NULL);
  if (FAILED(hr) || device == NULL) {
    hardware->Release();
    factory->Release();
    printf("gpu_probe: hardware adapter found but device failed 0x%08lX\n", (unsigned long)hr);
    return 1;
  }
  printf("gpu_probe: device feature level 0x%04X\n", (unsigned)feature_level);

  hardware->Release();
  device->Release();
  factory->Release();
  printf("gpu_probe: PASS - hardware adapter + device\n");
  return 0;
}