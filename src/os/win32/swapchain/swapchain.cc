// Swapchain implementation for Tyny PDF — DXGI swapchain + D2D target.
// src/os/win32/swapchain/swapchain.cc
// No engine includes (ADR-0011 R-M10). Engine-free.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

// Windows headers first to get proper type definitions
// clang-format off
#include <d2d1_3.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "pdfcore/swapchain.h"
// clang-format on

#include <cstdint>

using Microsoft::WRL::ComPtr;

struct pc_swapchain {
  HWND hwnd = nullptr;
  int width = 0;
  int height = 0;
  float dpi_scale = 1.0f;

  // D3D11
  ComPtr<ID3D11Device> d3d_device;
  ComPtr<ID3D11DeviceContext> d3d_context;

  // DXGI
  ComPtr<IDXGISwapChain1> swapchain;

  // Direct2D - stored as void* in struct but cast to proper types when used
  void* d2d_factory_raw = nullptr;    // ID2D1Factory3*
  void* d2d_device_raw = nullptr;     // ID2D1Device*
  void* d2d_context_raw = nullptr;    // ID2D1DeviceContext*
  void* target_bitmap_raw = nullptr;  // ID2D1Bitmap1*

  // Track ownership
  bool owns_d3d_device = false;
  bool owns_d2d_factory = false;
  bool initialized = false;

  // Helper methods for type-safe access
  ID2D1Factory3* d2d_factory() { return static_cast<ID2D1Factory3*>(d2d_factory_raw); }
  ID2D1Device* d2d_device() { return static_cast<ID2D1Device*>(d2d_device_raw); }
  ID2D1DeviceContext* d2d_context() { return static_cast<ID2D1DeviceContext*>(d2d_context_raw); }
  ID2D1Bitmap1* target_bitmap() { return static_cast<ID2D1Bitmap1*>(target_bitmap_raw); }
};

static HRESULT CreateD3DDevice(pc_swapchain* sc) {
  D3D_FEATURE_LEVEL feature_levels[] = {
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0,
  };

  UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
  flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  D3D_FEATURE_LEVEL feature_level;
  return D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, feature_levels,
                           ARRAYSIZE(feature_levels), D3D11_SDK_VERSION, &sc->d3d_device,
                           &feature_level, &sc->d3d_context);
}

static HRESULT CreateSwapchain(pc_swapchain* sc) {
  ComPtr<IDXGIDevice3> dxgi_device;
  HRESULT hr = sc->d3d_device.As(&dxgi_device);
  if (FAILED(hr))
    return hr;

  ComPtr<IDXGIAdapter> adapter;
  hr = dxgi_device->GetAdapter(&adapter);
  if (FAILED(hr))
    return hr;

  ComPtr<IDXGIFactory4> factory;
  hr = adapter->GetParent(IID_PPV_ARGS(&factory));
  if (FAILED(hr))
    return hr;

  DXGI_SWAP_CHAIN_DESC1 desc = {};
  desc.Width = static_cast<UINT>(sc->width);
  desc.Height = static_cast<UINT>(sc->height);
  desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.Stereo = FALSE;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.BufferCount = 2;
  desc.Scaling = DXGI_SCALING_STRETCH;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
  desc.Flags = 0;

  ComPtr<IDXGISwapChain1> swapchain;
  hr = factory->CreateSwapChainForHwnd(sc->d3d_device.Get(), sc->hwnd, &desc, nullptr, nullptr,
                                       &swapchain);
  if (FAILED(hr))
    return hr;

  // Disable ALT+ENTER fullscreen toggle
  factory->MakeWindowAssociation(sc->hwnd, DXGI_MWA_NO_ALT_ENTER);

  sc->swapchain = swapchain;
  return S_OK;
}

static HRESULT CreateD2DFactory(pc_swapchain* sc) {
  D2D1_FACTORY_OPTIONS options = {};
#ifdef _DEBUG
  options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

  ID2D1Factory3* factory = nullptr;
  HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory3),
                                 &options, reinterpret_cast<void**>(&factory));
  if (SUCCEEDED(hr)) {
    sc->d2d_factory_raw = factory;
  }
  return hr;
}

static HRESULT CreateD2DDevice(pc_swapchain* sc) {
  ComPtr<IDXGIDevice> dxgi_device;
  HRESULT hr = sc->d3d_device.As(&dxgi_device);
  if (FAILED(hr))
    return hr;

  ID2D1Device* device = nullptr;
  hr = sc->d2d_factory()->CreateDevice(dxgi_device.Get(), &device);
  if (FAILED(hr))
    return hr;
  sc->d2d_device_raw = device;

  ID2D1DeviceContext* context = nullptr;
  hr = device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS,
                                   &context);
  if (SUCCEEDED(hr)) {
    sc->d2d_context_raw = context;
  }
  return hr;
}

static HRESULT CreateTargetBitmap(pc_swapchain* sc) {
  ComPtr<IDXGISurface2> dxgi_surface;
  HRESULT hr = sc->swapchain->GetBuffer(0, IID_PPV_ARGS(&dxgi_surface));
  if (FAILED(hr))
    return hr;

  D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
      D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
      D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
      96.0f * sc->dpi_scale, 96.0f * sc->dpi_scale);

  ID2D1Bitmap1* bitmap = nullptr;
  hr = sc->d2d_context()->CreateBitmapFromDxgiSurface(dxgi_surface.Get(), &props, &bitmap);
  if (SUCCEEDED(hr)) {
    sc->target_bitmap_raw = bitmap;
  }
  return hr;
}

static HRESULT InitializeSwapchain(pc_swapchain* sc) {
  HRESULT hr = S_OK;
  if (!sc->d3d_device) {
    hr = CreateD3DDevice(sc);
    if (FAILED(hr))
      return hr;
    sc->owns_d3d_device = true;
  }

  if (!sc->d2d_factory_raw) {
    hr = CreateD2DFactory(sc);
    if (FAILED(hr))
      return hr;
    sc->owns_d2d_factory = true;
  }

  if (!sc->d2d_device_raw) {
    hr = CreateD2DDevice(sc);
    if (FAILED(hr))
      return hr;
  }

  hr = CreateSwapchain(sc);
  if (FAILED(hr))
    return hr;

  hr = CreateTargetBitmap(sc);
  if (FAILED(hr))
    return hr;

  sc->initialized = true;
  return S_OK;
}

pc_status pc_swapchain_create(const pc_swapchain_params* params, pc_swapchain** out_swapchain) {
  if (!params || !out_swapchain) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }

  *out_swapchain = nullptr;

  pc_swapchain* sc = new pc_swapchain();
  sc->hwnd = static_cast<HWND>(params->hwnd);
  sc->width = params->width > 0 ? params->width : 1024;
  sc->height = params->height > 0 ? params->height : 768;
  sc->dpi_scale = params->dpi_scale > 0 ? params->dpi_scale : 1.0f;
  sc->d3d_device = static_cast<ID3D11Device*>(params->d3d_device);
  sc->d2d_factory_raw = params->d2d_factory;

  HRESULT hr = InitializeSwapchain(sc);
  if (FAILED(hr)) {
    delete sc;
    return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "Swapchain initialization failed"};
  }

  *out_swapchain = sc;
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

pc_status pc_swapchain_resize(pc_swapchain* swapchain, int width, int height, float dpi_scale) {
  if (!swapchain) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null swapchain"};
  }

  if (width <= 0 || height <= 0) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "invalid size"};
  }

  if (width == swapchain->width && height == swapchain->height &&
      dpi_scale == swapchain->dpi_scale) {
    return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
  }

  swapchain->target_bitmap_raw = nullptr;
  swapchain->d2d_context()->SetTarget(nullptr);

  HRESULT hr = swapchain->swapchain->ResizeBuffers(
      0, static_cast<UINT>(width), static_cast<UINT>(height), DXGI_FORMAT_B8G8R8A8_UNORM, 0);
  if (FAILED(hr)) {
    return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "ResizeBuffers failed"};
  }

  swapchain->width = width;
  swapchain->height = height;
  swapchain->dpi_scale = dpi_scale;

  hr = CreateTargetBitmap(swapchain);
  if (FAILED(hr)) {
    return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "CreateTargetBitmap failed"};
  }

  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

pc_status pc_swapchain_begin_draw(pc_swapchain* swapchain, void** out_context) {
  if (!swapchain || !out_context) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }

  if (!swapchain->initialized || !swapchain->target_bitmap_raw) {
    return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "not initialized"};
  }

  swapchain->d2d_context()->SetTarget(swapchain->target_bitmap());
  swapchain->d2d_context()->BeginDraw();

  *out_context = swapchain->d2d_context();
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

pc_status pc_swapchain_end_draw(pc_swapchain* swapchain) {
  if (!swapchain) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null swapchain"};
  }

  if (!swapchain->target_bitmap_raw) {
    return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "no target bitmap"};
  }

  HRESULT hr = swapchain->d2d_context()->EndDraw();
  if (FAILED(hr)) {
    return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "EndDraw failed"};
  }

  hr = swapchain->swapchain->Present(1, 0);
  if (FAILED(hr)) {
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
      return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "Device lost"};
    }
    return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "Present failed"};
  }

  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

void pc_swapchain_get_size(pc_swapchain* swapchain, int* out_width, int* out_height) {
  if (!swapchain || !out_width || !out_height)
    return;
  *out_width = swapchain->width;
  *out_height = swapchain->height;
}

float pc_swapchain_get_dpi_scale(pc_swapchain* swapchain) {
  if (!swapchain)
    return 1.0f;
  return swapchain->dpi_scale;
}

void pc_swapchain_destroy(pc_swapchain* swapchain) {
  if (!swapchain)
    return;

  swapchain->target_bitmap_raw = nullptr;
  swapchain->d2d_context_raw = nullptr;
  swapchain->d2d_device_raw = nullptr;
  swapchain->d2d_factory_raw = nullptr;
  swapchain->swapchain.Reset();
  swapchain->d3d_context.Reset();
  if (swapchain->owns_d3d_device) {
    swapchain->d3d_device.Reset();
  }
  delete swapchain;
}