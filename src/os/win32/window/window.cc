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
#include "pdfcore/uia.h"
#include "dpi/dpi.h"
#include "uia/uia.h"
// clang-format on

#include <intrin.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifndef WM_POINTERWHEEL
#define WM_POINTERWHEEL 0x024E
#endif

using Microsoft::WRL::ComPtr;

// QPC milliseconds since an arbitrary origin (monotonic). The dynamic
// initializer below runs before main(), so kInitQpcMs is the earliest
// timestamp this module can observe - "process start" within the CRT.
static double qpc_now_ms() {
  static LARGE_INTEGER freq = {};
  if (freq.QuadPart == 0) {
    QueryPerformanceFrequency(&freq);
  }
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return (double)now.QuadPart * 1000.0 / (double)freq.QuadPart;
}

static const double kInitQpcMs = qpc_now_ms();

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
  // ABI 1.2 (story 6.1): mouse click and keyboard callbacks
  typedef void (*pc_window_click_callback)(int button, int x, int y, int modifiers,
                                           void* user_data);
  typedef void (*pc_window_key_callback)(int down, int vk, int modifiers, void* user_data);
  pc_window_click_callback click_cb = nullptr;
  pc_window_key_callback key_cb = nullptr;
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

  // Story 5.4: input/present timing log (build/tynypdf.ui.log).
  FILE* ui_log = nullptr;
  bool first_present_logged = false;
  double input_ts_ms = -1.0;  // pending wheel stamp, logged at next present
  double create_qpc_ms = 0.0;
  int frames_presented = 0;
  int frame_limit = 0;     // TYNYPDF_FRAMES: auto-quit after N presents
  int wheels_pending = 0;  // TYNYPDF_WHEELS: post N WM_MOUSEWHEEL, one per present

  // Story 5.5 (R24.7): document state the UIA provider announces (page, zoom).
  pc_uia_state* uia_state = nullptr;
};

// Forward declarations
static HRESULT CreateD3DDevice(WindowData* data);
static HRESULT CreateSwapchain(WindowData* data);
static HRESULT CreateDCompDevice(WindowData* data);
static HRESULT CreateD2DFactory(WindowData* data);
static HRESULT CreateD2DDevice(WindowData* data);
static HRESULT CreateTargetBitmap(WindowData* data);
static HRESULT InitializeGraphics(WindowData* data, const char** out_step);
static void ResizeSwapchain(WindowData* data);
static void RenderFrame(WindowData* data);
static std::string machine_line(WindowData* data);

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

    case WM_GETOBJECT: {
      // Story 5.5 (R24.7): hand the UIA client our provider when it asks for
      // the window's root. Returns false-and-DefWindowProc when the request is
      // not ours, so the accessibility proxy (AccPropSvc) still gets the system
      // fallback for everything else.
      LRESULT uia_result = 0;
      if (data && data->uia_state &&
          tynypdf::win32::pc_uia_handle_wm_getobject(hwnd, wparam, lparam, data->uia_state,
                                                     &uia_result)) {
        return uia_result;
      }
      break;
    }

    case WM_MOUSEWHEEL:
    case WM_POINTERWHEEL: {
      // Story 5.4 gesture latency: stamp the arrival, present on the next
      // loop pass, log input_ts/present_ts there (R24.6).
      if (data) {
        data->input_ts_ms = qpc_now_ms();
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    }

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN: {
      if (data && data->click_cb) {
        int x = GET_X_LPARAM(lparam);
        int y = GET_Y_LPARAM(lparam);
        int button = 0;
        if (msg == WM_LBUTTONDOWN)
          button = 1;
        else if (msg == WM_RBUTTONDOWN)
          button = 2;
        else if (msg == WM_MBUTTONDOWN)
          button = 3;
        int modifiers = 0;
        if (wparam & MK_SHIFT)
          modifiers |= 1;
        if (wparam & MK_CONTROL)
          modifiers |= 2;
        if (wparam & MK_ALT)
          modifiers |= 4;
        data->click_cb(button, x, y, modifiers, data->user_data);
      }
      return 0;
    }

    case WM_KEYDOWN:
    case WM_KEYUP: {
      if (data && data->key_cb) {
        int down = (msg == WM_KEYDOWN) ? 1 : 0;
        int vk = static_cast<int>(wparam);
        int modifiers = 0;
        if (GetKeyState(VK_SHIFT) & 0x8000)
          modifiers |= 1;
        if (GetKeyState(VK_CONTROL) & 0x8000)
          modifiers |= 2;
        if (GetKeyState(VK_MENU) & 0x8000)
          modifiers |= 4;
        data->key_cb(down, vk, modifiers, data->user_data);
      }
      return 0;
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
  // Premultiplied is only valid for per-pixel-composed scenarios; the window
  // fills its visual opaquely, so the hwnd swapchain takes IGNORE
  // (CreateSwapChainForHwnd rejects PREMULTIPLIED here with
  // DXGI_ERROR_INVALID_CALL, seen on the story 5.4 first run).
  desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
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
  if (SUCCEEDED(hr)) {
    // The context must be pointed at the buffer every time it is recreated;
    // 5.2 never ran this path and EndDraw answered D2DERR (0x88990001) with
    // no target bound (story 5.4 first run).
    data->d2d_context->SetTarget(data->target_bitmap.Get());
  }
  return hr;
}

// Initialize all graphics resources; out_step names the first failure
// (story 5.4: "Graphics initialization failed" was not actionable).
static HRESULT InitializeGraphics(WindowData* data, const char** out_step) {
  struct Step {
    const char* name;
    HRESULT (*fn)(WindowData*);
  };
  static const Step steps[] = {
      {"d3d11 device", CreateD3DDevice},   {"dxgi swapchain", CreateSwapchain},
      {"dcomp device", CreateDCompDevice}, {"d2d factory", CreateD2DFactory},
      {"d2d device", CreateD2DDevice},     {"target bitmap", CreateTargetBitmap},
  };
  HRESULT hr = E_FAIL;
  for (const Step& step : steps) {
    hr = step.fn(data);
    if (FAILED(hr)) {
      if (out_step) {
        *out_step = step.name;
      }
      return hr;
    }
  }
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

  // Story 5.4: present_ts is stamped when the frame is ready for Present -
  // the loop's processing latency; the compositor owns the queue wait.
  const double present_ts_ms = qpc_now_ms();

  // Present the frame. Sync 0: this minimal viewer draws a clear frame and
  // the measurement sessions run headless, where a vblank wait never
  // returns; the vsync criterion belongs to the content stories.
  hr = data->swapchain->Present(0, 0);
  if (SUCCEEDED(hr)) {
    data->dcomp_device->Commit();
    data->frames_presented++;

    if (data->ui_log) {
      if (!data->first_present_logged) {
        data->first_present_logged = true;
        fprintf(data->ui_log, "presenting frame create_to_present_ms=%.3f present_ts=%.3f\n",
                present_ts_ms - data->create_qpc_ms, present_ts_ms);
        fprintf(data->ui_log,
                "cold_start: init_to_create_ms=%.3f create_to_present_ms=%.3f "
                "init_to_present_ms=%.3f\n",
                data->create_qpc_ms - kInitQpcMs, present_ts_ms - data->create_qpc_ms,
                present_ts_ms - kInitQpcMs);
        fprintf(data->ui_log, "machine: %s\n", machine_line(data).c_str());
      }
      if (data->input_ts_ms >= 0.0) {
        fprintf(data->ui_log, "input_ts=%.3f present_ts=%.3f latency_ms=%.3f\n", data->input_ts_ms,
                present_ts_ms, present_ts_ms - data->input_ts_ms);
        data->input_ts_ms = -1.0;
      }
      fflush(data->ui_log);
    }
  } else if (data->ui_log) {
    // Fail loud and never leave a measurement run hanging on a session that
    // cannot present: log the failure, count the attempt, let TYNYPDF_FRAMES
    // stop the loop.
    fprintf(data->ui_log, "present failed hr=0x%lx\n", (unsigned long)hr);
    fflush(data->ui_log);
    data->frames_presented++;
  }

  if (data->frame_limit > 0 && data->frames_presented >= data->frame_limit) {
    PostMessage(data->hwnd, WM_CLOSE, 0, 0);
  }
}

// Story 5.4: one-line machine block for the cold-start log entry.
static std::string machine_line(WindowData* data) {
  char cpu[49] = "unknown";
  int brand[12] = {}, regs[4] = {};
  __cpuid(regs, 0x80000000);
  if ((unsigned int)regs[0] >= 0x80000004u) {
    for (int i = 0; i < 3; ++i) {
      __cpuid(brand + i * 4, 0x80000002 + i);
    }
    memcpy(cpu, brand, 48);
    cpu[48] = 0;
  }
  const char* cpu_trim = cpu;
  while (*cpu_trim == ' ') {
    ++cpu_trim;
  }

  wchar_t gpu[128] = L"unknown";
  ComPtr<IDXGIDevice> dxgi_device;
  ComPtr<IDXGIAdapter> adapter;
  if (SUCCEEDED(data->d3d_device.As(&dxgi_device)) &&
      SUCCEEDED(dxgi_device->GetAdapter(&adapter))) {
    DXGI_ADAPTER_DESC desc = {};
    if (SUCCEEDED(adapter->GetDesc(&desc))) {
      wcsncpy(gpu, desc.Description, 127);
      gpu[127] = 0;
    }
  }

  // RtlGetVersion (not the manifest-shimmed GetVersionEx) for the true build.
  struct RtlOsVersion {
    unsigned long size, major, minor, build, platform;
    wchar_t csd[128];
  } vi = {sizeof(vi), 0, 0, 0, 0, {}};
  typedef long(WINAPI * RtlGetVersionFn)(RtlOsVersion*);
  RtlGetVersionFn rtl =
      (RtlGetVersionFn)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
  if (!rtl || rtl(&vi) != 0) {
    vi.major = vi.minor = vi.build = 0;
  }

  char line[512];
  _snprintf(line, sizeof(line), "cpu=%s gpu=%ls os=Windows %lu.%lu.%lu dpi_scale=%.2f size=%dx%d",
            cpu_trim, gpu, vi.major, vi.minor, vi.build, data->dpi_scale, data->width,
            data->height);
  return std::string(line);
}

// PC API implementation
pc_status pc_window_create(const pc_window_params* params, const pc_window_callbacks* callbacks,
                           pc_window** out_window) {
  if (!params || !callbacks || !out_window) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }

  *out_window = nullptr;

  // Story 5.4 (R24.4): PMv2 must be declared before any window exists or
  // WM_DPICHANGED never arrives.
  tynypdf::win32::set_dpi_awareness_pm2();

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
  data->click_cb = callbacks->click;
  data->key_cb = callbacks->key;
  data->user_data = params->user_data;

  // Story 5.5 (R24.7): the window owns the a11y document state the UIA
  // provider announces. Defaults to page 1 of 1 at 100%; the viewer updates it
  // as documents open and zoom changes (the minimal viewer has no opener yet,
  // so the announcement is the placeholder "Page 1 of 1, zoom 100%").
  pc_status uia_st = pc_uia_state_create(&data->uia_state);
  if (uia_st.code != PC_ERR_NONE) {
    delete data;
    return uia_st;
  }

  // Get initial DPI for the monitor where window will be created (R24.4)
  HMONITOR monitor = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
  data->dpi_scale = tynypdf::win32::get_scale_for_monitor(monitor);

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
  const char* failed_step = "unknown";
  HRESULT hr = InitializeGraphics(data, &failed_step);
  if (FAILED(hr)) {
    DestroyWindow(hwnd);
    delete data;
    static char detail[96];  // pc_status.detail is a borrowed pointer; create is one-shot
    _snprintf(detail, sizeof(detail), "%s failed hr=0x%lx", failed_step, (unsigned long)hr);
    return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, detail};
  }

  // Show window
  ShowWindow(hwnd, SW_SHOW);
  UpdateWindow(hwnd);

  // Story 5.4: cold-start and gesture timing log. The default path matches
  // the DOD commands (run from the repo root); TYNYPDF_UI_LOG overrides it.
  // TYNYPDF_FRAMES=N auto-quits after N presents so measurements terminate.
  data->create_qpc_ms = qpc_now_ms();
  const char* log_path = getenv("TYNYPDF_UI_LOG");
  const char* frames = getenv("TYNYPDF_FRAMES");
  data->ui_log = fopen(log_path ? log_path : "build/tynypdf.ui.log", "a");
  if (data->ui_log) {
    fprintf(data->ui_log, "viewer: start limit=%s\n", frames ? frames : "off");
    fflush(data->ui_log);
  } else {
    fprintf(stderr, "tynypdf: cannot open ui log (%s)\n",
            log_path ? log_path : "build/tynypdf.ui.log");
  }
  if (frames) {
    data->frame_limit = atoi(frames);
  }

  // Story 5.4 R24.6: TYNYPDF_WHEELS=N posts N real WM_MOUSEWHEEL messages
  // (one per present) so the gesture path is measured end to end, headless,
  // with every stamp going through the actual wheel handler -> input_ts ->
  // next present -> latency_ms log line. This is a selftest knob like
  // TYNYPDF_FRAMES; the interactive product never reads it.
  const char* wheels = getenv("TYNYPDF_WHEELS");
  if (wheels) {
    data->wheels_pending = atoi(wheels);
    if (data->wheels_pending > 0 && !data->frame_limit) {
      // Auto-quit once every posted wheel has been measured: N wheels need
      // at most N+1 presents (first present has no pending input).
      data->frame_limit = data->wheels_pending + 1;
    }
  }

  *out_window = reinterpret_cast<pc_window*>(data);
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

pc_status pc_window_run(pc_window* window) {
  if (!window) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null window"};
  }

  WindowData* data = reinterpret_cast<WindowData*>(window);

  // Message loop. The first frame is presented before GetMessage so cold
  // start is deterministic and a message-less session (a disconnected or
  // automated desktop) still presents once; TYNYPDF_FRAMES=1 then exits.
  RenderFrame(data);

  MSG msg = {};
  while (GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);

    // Story 5.4 R24.6 selftest: TYNYPDF_WHEELS=N posts one real WM_MOUSEWHEEL
    // (delta 120, rel position) to our own queue after each present, so each
    // wheel travels PostMessage -> queue -> handler (stamps input_ts) ->
    // next present (logs latency_ms). Wheels are stamped at the wheel that
    // follows their message, so N posts need at most N+1 presents.
    if (data->wheels_pending > 0) {
      // One real notch per present: delta 120 lives in the HIGH word of
      // WM_MOUSEWHEEL's wparam, matching the wheel handler's
      // GET_WHEEL_DELTA_WPARAM path that the product uses (R24.6).
      PostMessage(data->hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, 120), MAKELPARAM(0, 0));
      data->wheels_pending--;
    }

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

  if (data->uia_state) {
    pc_uia_state_destroy(data->uia_state);
    data->uia_state = nullptr;
  }
  if (data->ui_log) {
    fclose(data->ui_log);
    data->ui_log = nullptr;
  }
  if (data->hwnd) {
    DestroyWindow(data->hwnd);
  }
  delete data;
}