// DPI module for Tyny PDF - story 5.4: Per-Monitor V2 awareness, monitor
// scale lookup, and the 150%/200% byte-for-byte purity selftest that produces
// tests/approvals/dpi-150.png and dpi-200.png. No engine include
// (ADR-0011 R-M10); every Windows type stays in this tree.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "dpi.h"

#include <d2d1.h>
#include <initguid.h>
#include <objbase.h>
#include <wincodec.h>
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

// Per-Monitor V2 context constant (absent from older MinGW headers).
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((HANDLE) - 4)
#endif

namespace tynypdf {
namespace win32 {

bool set_dpi_awareness_pm2() {
  typedef BOOL(WINAPI * SetCtxFn)(HANDLE);
  static SetCtxFn fn = nullptr;
  if (!fn) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) {
      return false;
    }
    fn = (SetCtxFn)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
  }
  return fn && fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != FALSE;
}

float get_scale_for_monitor(HMONITOR__* monitor) {
  if (!monitor) {
    return 1.0f;
  }
  typedef HRESULT(WINAPI * GetDpiFn)(HMONITOR, int, UINT*, UINT*);
  static GetDpiFn fn = nullptr;
  if (!fn) {
    HMODULE shcore = GetModuleHandleW(L"shcore.dll");
    if (!shcore) {
      return 1.0f;
    }
    fn = (GetDpiFn)GetProcAddress(shcore, "GetDpiForMonitor");
    if (!fn) {
      return 1.0f;
    }
  }
  UINT dpi_x = 96;
  UINT dpi_y = 96;
  if (FAILED(fn(monitor, 0 /* MDT_EFFECTIVE_DPI */, &dpi_x, &dpi_y)) || dpi_x == 0) {
    return 1.0f;
  }
  return (float)dpi_x / 96.0f;
}

namespace {

// Test pattern, 200x100 logical. Even coordinates only, so every edge lands
// on an integer pixel at both 1.5x and 2.0x: the aliased raster of the
// scale-transformed geometry must equal the raster of the pre-scaled
// geometry byte for byte ("no stretch"). via_transform=true applies a D2D
// scale over logical rects; false multiplies the rects (asked size).
void draw_pattern(ID2D1RenderTarget* rt, float scale, bool via_transform) {
  static const float kRects[][4] = {
      {10, 10, 60, 50},
      {70, 20, 110, 80},
      {120, 40, 190, 90},
      {10, 62, 190, 66},
  };
  static const float kColors[][4] = {
      {0.85f, 0.15f, 0.15f, 1.0f},
      {0.15f, 0.35f, 0.85f, 1.0f},
      {0.15f, 0.70f, 0.30f, 1.0f},
      {1.0f, 1.0f, 1.0f, 1.0f},
  };
  rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
  rt->Clear(D2D1::ColorF(0.10f, 0.10f, 0.12f, 1.0f));
  if (via_transform) {
    rt->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale));
  }
  for (int i = 0; i < 4; ++i) {
    ID2D1SolidColorBrush* brush = nullptr;
    if (SUCCEEDED(rt->CreateSolidColorBrush(
            D2D1::ColorF(kColors[i][0], kColors[i][1], kColors[i][2], kColors[i][3]), &brush))) {
      const float m = via_transform ? 1.0f : scale;
      const D2D1_RECT_F r =
          D2D1::RectF(kRects[i][0] * m, kRects[i][1] * m, kRects[i][2] * m, kRects[i][3] * m);
      rt->FillRectangle(&r, brush);
      brush->Release();
    }
  }
}

bool bitmaps_equal(IWICBitmap* a, IWICBitmap* b) {
  UINT wa = 0, ha = 0, wb = 0, hb = 0;
  a->GetSize(&wa, &ha);
  b->GetSize(&wb, &hb);
  if (wa != wb || ha != hb) {
    return false;
  }
  WICRect rc = {0, 0, (INT)wa, (INT)ha};
  IWICBitmapLock *la = nullptr, *lb = nullptr;
  if (FAILED(a->Lock(&rc, WICBitmapLockRead, &la)) ||
      FAILED(b->Lock(&rc, WICBitmapLockRead, &lb))) {
    if (la)
      la->Release();
    if (lb)
      lb->Release();
    return false;
  }
  UINT sa = 0, sb = 0, size_a = 0, size_b = 0;
  BYTE *pa = nullptr, *pb = nullptr;
  la->GetStride(&sa);
  lb->GetStride(&sb);
  la->GetDataPointer(&size_a, &pa);
  lb->GetDataPointer(&size_b, &pb);
  bool equal = sa == sb;
  for (UINT y = 0; equal && y < ha; ++y) {
    equal = memcmp(pa + y * sa, pb + y * sb, wa * 4) == 0;
  }
  la->Release();
  lb->Release();
  return equal;
}

bool encode_png(IWICImagingFactory* factory, IWICBitmap* bitmap, const wchar_t* path) {
  IWICStream* stream = nullptr;
  IWICBitmapEncoder* encoder = nullptr;
  IWICBitmapFrameEncode* frame = nullptr;
  bool ok = SUCCEEDED(factory->CreateStream(&stream)) &&
            SUCCEEDED(stream->InitializeFromFilename(path, GENERIC_WRITE)) &&
            SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) &&
            SUCCEEDED(encoder->Initialize(stream, WICBitmapEncoderNoCache)) &&
            SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr)) &&
            SUCCEEDED(frame->Initialize(nullptr));
  if (ok) {
    UINT w = 0, h = 0;
    bitmap->GetSize(&w, &h);
    WICPixelFormatGUID pf = GUID_WICPixelFormat32bppPBGRA;
    ok = SUCCEEDED(frame->SetSize(w, h)) && SUCCEEDED(frame->SetPixelFormat(&pf)) &&
         SUCCEEDED(frame->WriteSource(bitmap, nullptr)) && SUCCEEDED(frame->Commit()) &&
         SUCCEEDED(encoder->Commit());
  }
  if (frame)
    frame->Release();
  if (encoder)
    encoder->Release();
  if (stream)
    stream->Release();
  return ok;
}

std::wstring utf8_to_wide(const char* s) {
  std::wstring out;
  const int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
  if (n > 0) {
    out.resize((size_t)n - 1);
    MultiByteToWideChar(CP_UTF8, 0, s, -1, &out[0], n);
  }
  return out;
}

}  // namespace

bool dpi_selftest(const char* out_dir) {
  CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  IWICImagingFactory* wic = nullptr;
  ID2D1Factory* d2d = nullptr;
  const HRESULT hr_wic =
      CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic));
  const HRESULT hr_d2d = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d);
  if (FAILED(hr_wic) || FAILED(hr_d2d)) {
    printf("dpi: init failed wic=0x%lx d2d=0x%lx\n", (unsigned long)hr_wic, (unsigned long)hr_d2d);
  }
  bool all_ok = SUCCEEDED(hr_wic) && SUCCEEDED(hr_d2d);
  const float scales[2] = {1.5f, 2.0f};
  const char* names[2] = {"dpi-150.png", "dpi-200.png"};
  for (int i = 0; all_ok && i < 2; ++i) {
    const UINT pw = (UINT)(200 * scales[i] + 0.5f);
    const UINT ph = (UINT)(100 * scales[i] + 0.5f);
    IWICBitmap *rendered = nullptr, *asked = nullptr;
    ID2D1RenderTarget *rt_r = nullptr, *rt_a = nullptr;
    const HRESULT hr_b1 =
        wic->CreateBitmap(pw, ph, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &rendered);
    const HRESULT hr_b2 =
        wic->CreateBitmap(pw, ph, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &asked);
    all_ok = SUCCEEDED(hr_b1) && SUCCEEDED(hr_b2);
    if (!all_ok) {
      printf("dpi: %s wic bitmap hr=0x%lx/0x%lx\n", names[i], (unsigned long)hr_b1,
             (unsigned long)hr_b2);
    }
    HRESULT hr_rt1 = E_FAIL;
    HRESULT hr_rt2 = E_FAIL;
    if (all_ok) {
      hr_rt1 = d2d->CreateWicBitmapRenderTarget(rendered, D2D1::RenderTargetProperties(), &rt_r);
      hr_rt2 = d2d->CreateWicBitmapRenderTarget(asked, D2D1::RenderTargetProperties(), &rt_a);
      all_ok = SUCCEEDED(hr_rt1) && SUCCEEDED(hr_rt2);
      if (!all_ok) {
        printf("dpi: %s render target hr=0x%lx/0x%lx\n", names[i], (unsigned long)hr_rt1,
               (unsigned long)hr_rt2);
      }
    }
    if (all_ok) {
      rt_r->BeginDraw();
      draw_pattern(rt_r, scales[i], true);
      rt_r->EndDraw();
      rt_a->BeginDraw();
      draw_pattern(rt_a, scales[i], false);
      rt_a->EndDraw();
      const bool equal = bitmaps_equal(rendered, asked);
      std::wstring path = utf8_to_wide(out_dir);
      path += L"\\";
      path += utf8_to_wide(names[i]);
      const bool wrote = encode_png(wic, rendered, path.c_str());
      printf("dpi: %s scale=%.1f rendered==asked byte-for-byte: %s (%s)\n", names[i], scales[i],
             equal ? "YES" : "NO", wrote ? "golden written" : "PNG WRITE FAILED");
      all_ok = equal && wrote;
    }
    if (rt_r)
      rt_r->Release();
    if (rt_a)
      rt_a->Release();
    if (rendered)
      rendered->Release();
    if (asked)
      asked->Release();
  }
  if (d2d)
    d2d->Release();
  if (wic)
    wic->Release();
  CoUninitialize();
  return all_ok;
}

}  // namespace win32
}  // namespace tynypdf
