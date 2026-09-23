// Window implementation for Tyny PDF — Per-Monitor V2 DPI, DComp visual, message loop.
// src/os/win32/window/window.cc
// No backend includes (ADR-0011 R-M10). Engine-free.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#ifndef MDT_EFFECTIVE_DPI
#define MDT_EFFECTIVE_DPI 0
#endif

// Windows headers first to get proper type definitions
// clang-format off
#include <d2d1_3.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <wrl/client.h>

#include "pdfcore/window.h"
// clang-format on

using Microsoft::WRL::ComPtr;

// Window class name
static const wchar_t* const kWindowClassName = L"TynyPDFWindowClass";

// Window data stored in GWLP_USERDATA
struct WindowData {
  HWND hwnd = nullptr;
  int width = 0;
  int height = 0;
  float dpi_scale = 1.0f;

  // Callbacks
  pc_window_render_callback render_cb = nullptr;
  pc_window_dpi_callback dpi_cb = nullptr;
  pc_window_size_callback size_cb = nullptr;
  void* user_data = nullptr;

  // DComp/D3D11
  ComPtr<ID3D11Device> d3d_device;
  ComPtr<ID3D11DeviceContext> d3d_context;
  ComPtr<IDXGISwapChain1> swapchain;
  ComPtr<IDCompositionDevice> dcomp_device;
  ComPtr<IDCompositionTarget> dcomp_target;
  ComPtr<IDCompositionVisual> root_visual;
  ComPtr<IDCompositionVisual> content_visual;

  // Direct2D
  ComPtr<ID2D1Factory3> d2d_factory;
  ComPtr<ID2D1Device> d2d_device;
  ComPtr<ID2D1DeviceContext> d2d_context;
  ComPtr<ID2D1Bitmap1> target_bitmap;

  // Track if graphics are initialized
  bool graphics_initialized = false;
};

// Forward declarations
static HRESULT CreateD3DDevice(WindowData* data);
static HRESULT CreateSwapchain(WindowData* data);
static HRESULT CreateDCompDevice(WindowData* data);
static HRESULT CreateD2DFactory(WindowData* data);
static HRESULT CreateD2DDevice(WindowData* data);
static HRESULT CreateTargetBitmap(WindowData* data);
static HRESULT InitializeGraphics(WindowData* data);
static void ResizeSwapchain(WindowData* data);
static void RenderFrame(WindowData* data);

// Window procedure
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  WindowData* data = reinterpret_cast<WindowData*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

  switch (msg) {
    case WM_CREATE: {
      CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lparam);
      data = static_cast<WindowData*>(cs->lpCreateParams);
      data->hwnd = hwnd;
      SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
      return 0;
    }

    case WM_DESTROY: {
      PostQuitMessage(0);
      return 0;
    }

    case WM_SIZE: {
      if (data && wparam != SIZE_MINIMIZED) {
        int new_width = LOWORD(lparam);
        int new_height = HIWORD(lparam);
        if (new_width != data->width || new_height != data->height) {
          data->width = new_width;
          data->height = new_height;
          ResizeSwapchain(data);
          if (data->size_cb) {
            data->size_cb(data->width, data->height, data->user_data);
          }
        }
      }
      return 0;
    }

    case WM_DPICHANGED: {
      if (data) {
        const RECT* suggested = reinterpret_cast<const RECT*>(lparam);
        SetWindowPos(data->hwnd, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        float new_dpi = HIWORD(wparam) / 96.0f;
        if (new_dpi != data->dpi_scale) {
          data->dpi_scale = new_dpi;
          ResizeSwapchain(data);
          if (data->dpi_cb) {
            data->dpi_cb(data->dpi_scale, data->user_data);
          }
        }
      }
      return 0;
    }

    case WM_PAINT: {
      PAINTSTRUCT ps;
      BeginPaint(hwnd, &ps);
      EndPaint(hwnd, &ps);
      // Actual rendering happens via render callback on demand
      return 0;
    }

    case WM_ERASEBKGND: {
      // Prevent flicker - we handle background in render
      return 1;
    }

    default:
      return DefWindowProc(hwnd, msg, wparam, lparam);
  }
  return 0;
}

// D3D11 device creation
static HRESULT CreateD3DDevice(WindowData* data) {
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
  HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, feature_levels,
                                 ARRAYSIZE(feature_levels), D3D11_SDK_VERSION, &data->d3d_device,
                                 &feature_level, &data->d3d_context);
  return hr;
}

// DXGI swapchain creation
static HRESULT CreateSwapchain(WindowData* data) {
  ComPtr<IDXGIDevice3> dxgi_device;
  HRESULT hr = data->d3d_device.As(&dxgi_device);
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
  desc.Width = static_cast<UINT>(data->width);
  desc.Height = static_cast<UINT>(data->height);
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
  hr = factory->CreateSwapChainForHwnd(data->d3d_device.Get(), data->hwnd, &desc, nullptr, nullptr,
                                       &swapchain);
  if (FAILED(hr))
    return hr;

  // Disable ALT+ENTER fullscreen toggle
  factory->MakeWindowAssociation(data->hwnd, DXGI_MWA_NO_ALT_ENTER);

  data->swapchain = swapchain;
  return S_OK;
}

// DComp device creation
static HRESULT CreateDCompDevice(WindowData* data) {
  HRESULT hr = DCompositionCreateDevice(nullptr, IID_PPV_ARGS(&data->dcomp_device));
  if (FAILED(hr))
    return hr;

  hr = data->dcomp_device->CreateTargetForHwnd(data->hwnd, TRUE, &data->dcomp_target);
  if (FAILED(hr))
    return hr;

  hr = data->dcomp_device->CreateVisual(&data->root_visual);
  if (FAILED(hr))
    return hr;

  hr = data->dcomp_target->SetRoot(data->root_visual.Get());
  if (FAILED(hr))
    return hr;

  hr = data->dcomp_device->CreateVisual(&data->content_visual);
  if (FAILED(hr))
    return hr;

  hr = data->root_visual->AddVisual(data->content_visual.Get(), FALSE, nullptr);
  return hr;
}

// D2D factory creation
static HRESULT CreateD2DFactory(WindowData* data) {
  D2D1_FACTORY_OPTIONS options = {};
#ifdef _DEBUG
  options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

  HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory3),
                                 &options, &data->d2d_factory);
  return hr;
}

// D2D device creation
static HRESULT CreateD2DDevice(WindowData* data) {
  ComPtr<IDXGIDevice> dxgi_device;
  HRESULT hr = data->d3d_device.As(&dxgi_device);
  if (FAILED(hr))
    return hr;

  hr = data->d2d_factory->CreateDevice(dxgi_device.Get(), &data->d2d_device);
  if (FAILED(hr))
    return hr;

  hr = data->d2d_device->CreateDeviceContext(
      D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS, &data->d2d_context);
  return hr;
}

// Target bitmap creation from swapchain
static HRESULT CreateTargetBitmap(WindowData* data) {
  ComPtr<IDXGISurface2> dxgi_surface;
  HRESULT hr = data->swapchain->GetBuffer(0, IID_PPV_ARGS(&dxgi_surface));
  if (FAILED(hr))
    return hr;

  D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
      D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
      D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
      96.0f * data->dpi_scale, 96.0f * data->dpi_scale);

  hr = data->d2d_context->CreateBitmapFromDxgiSurface(dxgi_surface.Get(), &props,
                                                      &data->target_bitmap);
  return hr;
}

// Initialize all graphics resources
static HRESULT InitializeGraphics(WindowData* data) {
  HRESULT hr = CreateD3DDevice(data);
  if (FAILED(hr))
    return hr;

  hr = CreateSwapchain(data);
  if (FAILED(hr))
    return hr;

  hr = CreateDCompDevice(data);
  if (FAILED(hr))
    return hr;

  hr = CreateD2DFactory(data);
  if (FAILED(hr))
    return hr;

  hr = CreateD2DDevice(data);
  if (FAILED(hr))
    return hr;

  hr = CreateTargetBitmap(data);
  if (FAILED(hr))
    return hr;

  data->graphics_initialized = true;
  return S_OK;
}

// Resize swapchain and recreate target bitmap
static void ResizeSwapchain(WindowData* data) {
  if (!data->swapchain || data->width <= 0 || data->height <= 0)
    return;

  data->target_bitmap.Reset();
  data->d2d_context->SetTarget(nullptr);

  HRESULT hr = data->swapchain->ResizeBuffers(0, static_cast<UINT>(data->width),
                                              static_cast<UINT>(data->height),
                                              DXGI_FORMAT_B8G8R8A8_UNORM, 0);
  if (SUCCEEDED(hr)) {
    CreateTargetBitmap(data);
  }
}

// Render a frame using the callback
static void RenderFrame(WindowData* data) {
  if (!data->graphics_initialized || !data->target_bitmap) {
    return;
  }

  HRESULT hr = S_OK;
  data->d2d_context->BeginDraw();

  // Clear to white
  data->d2d_context->Clear(D2D1::ColorF(D2D1::ColorF::White));

  // Call the render callback if provided
  if (data->render_cb) {
    pc_status cb_status = data->render_cb(data->d2d_context.Get(), data->user_data);
    if (cb_status.code != PC_ERR_NONE) {
      // Render callback failed
      data->d2d_context->EndDraw();
      return;
    }
  }

  hr = data->d2d_context->EndDraw();
  if (FAILED(hr)) {
    // Device lost, will be handled on next frame
    return;
  }

  // Present the frame
  hr = data->swapchain->Present(1, 0);
  if (SUCCEEDED(hr)) {
    data->dcomp_device->Commit();
  }
}

// PC API implementation
pc_status pc_window_create(const pc_window_params* params, const pc_window_callbacks* callbacks,
                           pc_window** out_window) {
  if (!params || !callbacks || !out_window) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }

  *out_window = nullptr;

  // Register window class (once)
  static bool class_registered = false;
  if (!class_registered) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClassName;
    if (!RegisterClassExW(&wc)) {
      return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "RegisterClassExW failed"};
    }
    class_registered = true;
  }

  // Allocate window data
  WindowData* data = new WindowData();
  data->width = params->width > 0 ? params->width : 1024;
  data->height = params->height > 0 ? params->height : 768;
  data->render_cb = callbacks->render;
  data->dpi_cb = callbacks->dpi_changed;
  data->size_cb = callbacks->size_changed;
  data->user_data = params->user_data;

  // Get initial DPI for the monitor where window will be created
  HMONITOR monitor = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
  UINT dpi = 96;
  if (monitor) {
    // GetDpiForMonitor is in shellapi.h, may not be in MinGW headers
    typedef HRESULT(WINAPI * GetDpiForMonitorFn)(HMONITOR, int, UINT*, UINT*);
    static GetDpiForMonitorFn pGetDpiForMonitor = nullptr;
    if (!pGetDpiForMonitor) {
      HMODULE shcore = GetModuleHandleW(L"shcore.dll");
      if (shcore) {
        pGetDpiForMonitor = (GetDpiForMonitorFn)GetProcAddress(shcore, "GetDpiForMonitor");
      }
    }
    if (pGetDpiForMonitor) {
      pGetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi, nullptr);
    }
  }
  data->dpi_scale = dpi / 96.0f;

  // Create window
  RECT rc = {0, 0, data->width, data->height};
  AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);

  HWND hwnd = CreateWindowExW(0, kWindowClassName, params->title ? params->title : L"Tyny PDF",
                              WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left,
                              rc.bottom - rc.top, nullptr, nullptr, GetModuleHandle(nullptr), data);

  if (!hwnd) {
    delete data;
    return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "CreateWindowEx failed"};
  }

  // Initialize graphics
  HRESULT hr = InitializeGraphics(data);
  if (FAILED(hr)) {
    DestroyWindow(hwnd);
    delete data;
    return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "Graphics initialization failed"};
  }

  // Show window
  ShowWindow(hwnd, SW_SHOW);
  UpdateWindow(hwnd);

  *out_window = reinterpret_cast<pc_window*>(data);
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

pc_status pc_window_run(pc_window* window) {
  if (!window) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null window"};
  }

  WindowData* data = reinterpret_cast<WindowData*>(window);

  // Message loop
  MSG msg = {};
  while (GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);

    // Render frame after processing messages
    RenderFrame(data);
  }

  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

pc_status pc_window_request_redraw(pc_window* window) {
  if (!window) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null window"};
  }

  WindowData* data = reinterpret_cast<WindowData*>(window);
  InvalidateRect(data->hwnd, nullptr, FALSE);
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

float pc_window_get_dpi_scale(pc_window* window) {
  if (!window)
    return 1.0f;
  WindowData* data = reinterpret_cast<WindowData*>(window);
  return data->dpi_scale;
}

void pc_window_get_size(pc_window* window, int* out_width, int* out_height) {
  if (!window || !out_width || !out_height)
    return;
  WindowData* data = reinterpret_cast<WindowData*>(window);
  *out_width = data->width;
  *out_height = data->height;
}

void pc_window_destroy(pc_window* window) {
  if (!window)
    return;
  WindowData* data = reinterpret_cast<WindowData*>(window);

  if (data->hwnd) {
    DestroyWindow(data->hwnd);
  }
  delete data;
}