// Win32 window creation and message loop for Tyny PDF viewer
// Per Epic 2: window + DComp/D3D11 swapchain + Direct2D blit

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dcomp.h>
#include <d2d1_3.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <pdfcore/render.h>
#include <pdfcore/status.h>
#include <cstdlib>
#include <cstdint>

using Microsoft::WRL::ComPtr;

// Window class name
static const wchar_t* const kWindowClassName = L"TynyPDFWindowClass";

// Window data stored in GWLP_USERDATA
struct WindowData {
    HWND hwnd;
    uint32_t width;
    uint32_t height;
    float dpi_scale;
    
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
    
    // Tile cache
    struct Tile {
        ComPtr<ID2D1Bitmap1> bitmap;
        int x, y;
        uint32_t generation;
    };
    static constexpr uint32_t kTileSize = 256;
    static constexpr uint32_t kMaxTiles = 64;
    Tile tiles[kMaxTiles];
    uint32_t tile_generation = 0;
    
    // Current page being displayed
    void* current_page = nullptr;
    const pc_backend_api* backend = nullptr;
};

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
                data->width = LOWORD(lparam);
                data->height = HIWORD(lparam);
                // Resize swapchain handled in render loop
            }
            return 0;
        }
        
        case WM_DPICHANGED: {
            if (data) {
                const RECT* suggested = reinterpret_cast<const RECT*>(lparam);
                SetWindowPos(data->hwnd, nullptr,
                    suggested->left, suggested->top,
                    suggested->right - suggested->left,
                    suggested->bottom - suggested->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
                data->dpi_scale = HIWORD(wparam) / 96.0f;
            }
            return 0;
        }
        
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_MOUSEWHEEL: {
            // Wheel zoom handled in 2.7
            return 0;
        }
        
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP: {
            // Mouse input for pan/zoom - handled in 2.7
            return 0;
        }
        
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_CHAR: {
            // Keyboard input for text fields - handled in 2.6
            return 0;
        }
        
        default:
            return DefWindowProc(hwnd, msg, wparam, lparam);
    }
    return 0;
}

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
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        feature_levels,
        ARRAYSIZE(feature_levels),
        D3D11_SDK_VERSION,
        &data->d3d_device,
        &feature_level,
        &data->d3d_context
    );
    return hr;
}

static HRESULT CreateSwapchain(WindowData* data) {
    ComPtr<IDXGIDevice3> dxgi_device;
    HRESULT hr = data->d3d_device.As(&dxgi_device);
    if (FAILED(hr)) return hr;
    
    ComPtr<IDXGIAdapter> adapter;
    hr = dxgi_device->GetAdapter(&adapter);
    if (FAILED(hr)) return hr;
    
    ComPtr<IDXGIFactory4> factory;
    hr = adapter->GetParent(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return hr;
    
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = data->width;
    desc.Height = data->height;
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
    hr = factory->CreateSwapChainForHwnd(
        data->d3d_device.Get(),
        data->hwnd,
        &desc,
        nullptr,
        nullptr,
        &swapchain
    );
    if (FAILED(hr)) return hr;
    
    // Disable ALT+ENTER fullscreen toggle
    factory->MakeWindowAssociation(data->hwnd, DXGI_MWA_NO_ALT_ENTER);
    
    data->swapchain = swapchain;
    return S_OK;
}

static HRESULT CreateDCompDevice(WindowData* data) {
    HRESULT hr = DCompositionCreateDevice(
        nullptr,
        IID_PPV_ARGS(&data->dcomp_device)
    );
    if (FAILED(hr)) return hr;
    
    hr = data->dcomp_device->CreateTargetForHwnd(
        data->hwnd,
        TRUE,
        &data->dcomp_target
    );
    if (FAILED(hr)) return hr;
    
    hr = data->dcomp_device->CreateVisual(&data->root_visual);
    if (FAILED(hr)) return hr;
    
    hr = data->dcomp_target->SetRoot(data->root_visual.Get());
    if (FAILED(hr)) return hr;
    
    hr = data->dcomp_device->CreateVisual(&data->content_visual);
    if (FAILED(hr)) return hr;
    
    hr = data->root_visual->AddVisual(data->content_visual.Get(), FALSE, nullptr);
    return hr;
}

static HRESULT CreateD2DFactory(WindowData* data) {
    D2D1_FACTORY_OPTIONS options = {};
#ifdef _DEBUG
    options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    
    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory3),
        &options,
        reinterpret_cast<void**>(&data->d2d_factory)
    );
    return hr;
}

static HRESULT CreateD2DDevice(WindowData* data) {
    ComPtr<IDXGIDevice> dxgi_device;
    HRESULT hr = data->d3d_device.As(&data->d2d_device);
    if (FAILED(hr)) return hr;
    
    hr = data->d2d_factory->CreateDevice(
        data->d2d_device.Get(),
        &data->d2d_device
    );
    if (FAILED(hr)) return hr;
    
    hr = data->d2d_device->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS,
        &data->d2d_context
    );
    return hr;
}

static HRESULT CreateTargetBitmap(WindowData* data) {
    ComPtr<IDXGISurface2> dxgi_surface;
    HRESULT hr = data->swapchain->GetBuffer(0, IID_PPV_ARGS(&dxgi_surface));
    if (FAILED(hr)) return hr;
    
    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f * data->dpi_scale,
        96.0f * data->dpi_scale
    );
    
    hr = data->d2d_context->CreateBitmapFromDxgiSurface(
        dxgi_surface.Get(),
        &props,
        &data->target_bitmap
    );
    return hr;
}

static HRESULT InitializeGraphics(WindowData* data) {
    HRESULT hr = CreateD3DDevice(data);
    if (FAILED(hr)) return hr;
    
    hr = CreateSwapchain(data);
    if (FAILED(hr)) return hr;
    
    hr = CreateDCompDevice(data);
    if (FAILED(hr)) return hr;
    
    hr = CreateD2DFactory(data);
    if (FAILED(hr)) return hr;
    
    hr = CreateD2DDevice(data);
    if (FAILED(hr)) return hr;
    
    hr = CreateTargetBitmap(data);
    return hr;
}

static void ResizeSwapchain(WindowData* data) {
    if (!data->swapchain) return;
    
    data->target_bitmap.Reset();
    data->d2d_context->SetTarget(nullptr);
    
    HRESULT hr = data->swapchain->ResizeBuffers(
        0, data->width, data->height,
        DXGI_FORMAT_B8G8R8A8_UNORM, 0
    );
    if (SUCCEEDED(hr)) {
        CreateTargetBitmap(data);
    }
}

static void RenderFrame(WindowData* data) {
    if (!data->target_bitmap || !data->current_page || !data->backend) {
        // Clear to gray
        data->d2d_context->SetTarget(data->target_bitmap.Get());
        data->d2d_context->Clear(D2D1::ColorF(D2D1::ColorF::Gray));
        data->swapchain->Present(1, 0);
        data->dcomp_device->Commit();
        return;
    }
    
    data->d2d_context->SetTarget(data->target_bitmap.Get());
    data->d2d_context->BeginDraw();
    data->d2d_context->Clear(D2D1::ColorF(D2D1::ColorF::White));
    
    // TODO: Implement tile-based rendering from pc_page_render
    // For now, render a placeholder
    D2D1_RECT_F rect = D2D1::RectF(0, 0, 
        static_cast<float>(data->width), 
        static_cast<float>(data->height));
    data->d2d_context->FillRectangle(&rect, 
        data->d2d_context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::LightGray)));
    
    HRESULT hr = data->d2d_context->EndDraw();
    if (FAILED(hr)) {
        // Handle device lost
        return;
    }
    
    hr = data->swapchain->Present(1, 0);
    if (SUCCEEDED(hr)) {
        data->dcomp_device->Commit();
    }
}

pc_status tynypdf_ui_run(const char* pdf_path, const pc_backend_api* backend, uint32_t page) {
    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return { sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "CoInitializeEx failed" };
    }
    
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClassName;
    RegisterClassExW(&wc);
    
    // Get DPI
    float dpi_scale = 1.0f;
    HDC screen_dc = GetDC(nullptr);
    if (screen_dc) {
        dpi_scale = GetDeviceCaps(screen_dc, LOGPIXELSX) / 96.0f;
        ReleaseDC(nullptr, screen_dc);
    }
    
    // Create window data
    WindowData data = {};
    data.width = 1024;
    data.height = 768;
    data.dpi_scale = 1.0f;
    data.backend = nullptr; // Will be set after window creation
    
    // Create window
    RECT rc = { 0, 0, 1024, 768 };
    AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);
    
    HWND hwnd = CreateWindowExW(
        0,
        kWindowClassName,
        L"Tyny PDF",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    if (!hwnd) {
        CoUninitialize();
        return { sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "CreateWindowEx failed" };
    }
    
    // Store window data in GWLP_USERDATA will be done in WM_CREATE
    
    // Initialize graphics
    WindowData* data = reinterpret_cast<WindowData*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    // The WindowProc will set up the data pointer in WM_CREATE
    // We need to set it before initialization
    SetWindowLongPtr(hwnd, GWLP_USERDATA, 0); // Will be set in WM_CREATE
    
    // For now, create a temporary WindowData to initialize graphics
    WindowData temp_data = {};
    temp_data.hwnd = hwnd;
    temp_data.width = 1024;
    temp_data.height = 768;
    temp_data.dpi_scale = 1.0f;
    
    HRESULT hr = InitializeGraphics(&temp_data);
    if (FAILED(hr)) {
        DestroyWindow(hwnd);
        CoUninitialize();
        return { sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "Graphics initialization failed" };
    }
    
    // Copy initialized graphics to actual window data
    // (This is a simplified approach - real implementation would do this in WM_CREATE)
    
    // Set the backend for rendering
    // Note: In real implementation, we'd open the document here
    // and get the page to render
    
    // Show window
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    
    // Message loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        
        // Render frame if window is valid
        // In real implementation, this would be driven by the swapchain present
    }
    
    CoUninitialize();
    return { sizeof(pc_status), PC_ERR_NONE, 0, nullptr };
}