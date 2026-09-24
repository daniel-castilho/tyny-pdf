// UI Automation provider for Tyny PDF — story 5.5 (M1 a11y criterion, R24.7).
// src/os/win32/uia/uia.cc
//
// The window exposes one fragment (IRawElementProviderFragment) whose children
// are the page, zoom and focus elements, and a ValuePattern whose get_Value()
// returns the announced text "Page 1 of 5, zoom 150%" (the exact string the
// docs/a11y scripts diff against). The document state it reads is our own
// pc_uia_state (include/pdfcore/uia.h) — no engine handle crosses this file,
// same rule as the render module (ADR-0011 R-M10, R-M11).
//
// Only src/os/win32 may include this header; the public surface the viewer
// calls is include/pdfcore/uia.h.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

// Windows headers first to get proper type definitions
// clang-format off
#include <windows.h>
#include <uiautomationcore.h>
#include <uiautomationcoreapi.h>
#include <objbase.h>
// clang-format on

#include "pdfcore/uia.h"

#include "pdfcore/status.h"
#include "uia.h"

// ValuePattern and the property/control ids the provider replies to live in
// uiautomationclient.h in the SDK; the core header does not define them.
#ifndef UIA_ValuePatternId
#define UIA_ValuePatternId (10018)
#endif
#ifndef UIA_NamePropertyId
#define UIA_NamePropertyId (30001)
#endif
#ifndef UIA_ControlTypePropertyId
#define UIA_ControlTypePropertyId (30003)
#endif
#ifndef UIA_AutomationIdPropertyId
#define UIA_AutomationIdPropertyId (30004)
#endif
#ifndef UIA_IsKeyboardFocusablePropertyId
#define UIA_IsKeyboardFocusablePropertyId (30009)
#endif
#ifndef UIA_IsEnabledPropertyId
#define UIA_IsEnabledPropertyId (30010)
#endif
#ifndef UIA_TextControlTypeId
#define UIA_TextControlTypeId (50020)
#endif

namespace tynypdf {
namespace win32 {

namespace {

// Read-only ValuePattern over the document state. get_Value() is the announced
// text the Narrator/NVDA scripts diff against (R24.7).
class ValueProvider final : public IValueProvider {
 public:
  explicit ValueProvider(pc_uia_state* state) : refs_(1), state_(state) {}

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (!ppv)
      return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IValueProvider) {
      AddRef();
      *ppv = static_cast<IValueProvider*>(this);
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
  }
  STDMETHODIMP_(ULONG) AddRef() override { return ++refs_; }
  STDMETHODIMP_(ULONG) Release() override {
    ULONG n = --refs_;
    if (n == 0)
      delete this;
    return n;
  }

  // IValueProvider. The announcement is read-only from the provider side:
  // page/zoom changes go through the viewer, not through a UIA client.
  STDMETHODIMP SetValue(LPCWSTR) override { return E_NOTIMPL; }
  STDMETHODIMP get_Value(BSTR* pRetVal) override {
    if (!pRetVal)
      return E_POINTER;
    wchar_t text[PC_UIA_ANNOUNCE_MAX] = {};
    pc_status st = pc_uia_state_announcement(state_, text, PC_UIA_ANNOUNCE_MAX);
    if (st.code != PC_ERR_NONE)
      return E_FAIL;
    *pRetVal = SysAllocString(text);
    return *pRetVal ? S_OK : E_OUTOFMEMORY;
  }
  STDMETHODIMP get_IsReadOnly(WINBOOL* pRetVal) override {
    if (!pRetVal)
      return E_POINTER;
    *pRetVal = TRUE;
    return S_OK;
  }

 private:
  ULONG refs_;
  pc_uia_state* state_;
};

// The window provider. UIA asks the hwnd for the provider through
// UiaReturnRawElementProvider; the object returned must answer
// IRawElementProviderSimple (through the host provider) and, because it is the
// root of a fragment tree, IRawElementProviderFragment. The same object is
// both, via one QueryInterface.
class WindowProvider final : public IRawElementProviderFragment, public IRawElementProviderSimple {
 public:
  WindowProvider(HWND hwnd, pc_uia_state* state) : refs_(1), hwnd_(hwnd), state_(state) {}

  // IUnknown — single identity: both interfaces are this same object.
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (!ppv)
      return E_POINTER;
    if (riid == IID_IUnknown) {
      AddRef();
      *ppv = static_cast<IUnknown*>(static_cast<IRawElementProviderFragment*>(this));
      return S_OK;
    }
    if (riid == IID_IRawElementProviderFragment) {
      AddRef();
      *ppv = static_cast<IRawElementProviderFragment*>(this);
      return S_OK;
    }
    if (riid == IID_IRawElementProviderSimple) {
      AddRef();
      *ppv = static_cast<IRawElementProviderSimple*>(this);
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
  }
  STDMETHODIMP_(ULONG) AddRef() override { return ++refs_; }
  STDMETHODIMP_(ULONG) Release() override {
    ULONG n = --refs_;
    if (n == 0)
      delete this;
    return n;
  }

  // ---- IRawElementProviderFragment ----
  STDMETHODIMP Navigate(enum NavigateDirection direction,
                        IRawElementProviderFragment** pRetVal) override {
    if (!pRetVal)
      return E_POINTER;
    *pRetVal = nullptr;
    if (direction == NavigateDirection_FirstChild)
      return MakeChild(kChildPage, pRetVal);
    if (direction == NavigateDirection_LastChild)
      return MakeChild(kChildFocus, pRetVal);
    // Parent/sibling: the window is the root of its own tree; UIA answers the
    // host root for a hwnd-based provider, never this provider.
    return S_OK;
  }
  STDMETHODIMP GetRuntimeId(SAFEARRAY** pRetVal) override {
    if (pRetVal)
      *pRetVal = nullptr;
    // The hwnd-based root owns its identity through the window handle; a
    // provider-supplied runtime id is only needed for non-window elements.
    return E_NOTIMPL;
  }
  STDMETHODIMP get_BoundingRectangle(struct UiaRect* pRetVal) override {
    if (!pRetVal)
      return E_POINTER;
    RECT rc = {};
    if (GetClientRect(hwnd_, &rc)) {
      pRetVal->left = static_cast<double>(rc.left);
      pRetVal->top = static_cast<double>(rc.top);
      pRetVal->width = static_cast<double>(rc.right - rc.left);
      pRetVal->height = static_cast<double>(rc.bottom - rc.top);
    } else {
      pRetVal->left = pRetVal->top = pRetVal->width = pRetVal->height = 0;
    }
    return S_OK;
  }
  STDMETHODIMP GetEmbeddedFragmentRoots(SAFEARRAY** pRetVal) override {
    if (pRetVal)
      *pRetVal = nullptr;
    return S_OK;
  }
  STDMETHODIMP SetFocus() override {
    ::SetFocus(hwnd_);
    return S_OK;
  }
  STDMETHODIMP get_FragmentRoot(IRawElementProviderFragmentRoot** pRetVal) override {
    // This fragment is its own subtree's root and has no embedded-root parent;
    // returning nullptr lets the hwnd host act as the fragment root.
    if (pRetVal)
      *pRetVal = nullptr;
    return S_OK;
  }

  // ---- IRawElementProviderSimple ----
  STDMETHODIMP get_ProviderOptions(enum ProviderOptions* pRetVal) override {
    if (!pRetVal)
      return E_POINTER;
    // Server-side provider: the window and its children are ours, object-backed
    // elements, so the client may treat them as in-process.
    *pRetVal = ProviderOptions_ServerSideProvider;
    return S_OK;
  }
  STDMETHODIMP GetPatternProvider(PATTERNID pattern_id, IUnknown** pRetVal) override {
    if (!pRetVal)
      return E_POINTER;
    if (pattern_id == UIA_ValuePatternId) {
      *pRetVal = new ValueProvider(state_);
      return S_OK;
    }
    *pRetVal = nullptr;
    return S_OK;
  }
  STDMETHODIMP get_HostRawElementProvider(IRawElementProviderSimple** pRetVal) override {
    // The window surface is ours (not a Win32 control we host), so there is no
    // separate host provider to hand back.
    if (pRetVal)
      *pRetVal = nullptr;
    return S_OK;
  }
  STDMETHODIMP GetPropertyValue(PROPERTYID property_id, VARIANT* pRetVal) override {
    if (!pRetVal)
      return E_POINTER;
    VariantInit(pRetVal);
    switch (property_id) {
      case UIA_NamePropertyId: {
        wchar_t text[PC_UIA_ANNOUNCE_MAX] = {};
        pc_status st = pc_uia_state_announcement(state_, text, PC_UIA_ANNOUNCE_MAX);
        if (st.code != PC_ERR_NONE)
          return E_FAIL;
        pRetVal->vt = VT_BSTR;
        pRetVal->bstrVal = SysAllocString(text);
        return pRetVal->bstrVal ? S_OK : E_OUTOFMEMORY;
      }
      case UIA_ControlTypePropertyId:
        pRetVal->vt = VT_I4;
        pRetVal->lVal = static_cast<LONG>(UIA_TextControlTypeId);
        return S_OK;
      case UIA_IsKeyboardFocusablePropertyId:
        pRetVal->vt = VT_BOOL;
        pRetVal->boolVal = VARIANT_TRUE;
        return S_OK;
      case UIA_IsEnabledPropertyId:
        pRetVal->vt = VT_BOOL;
        pRetVal->boolVal = VARIANT_TRUE;
        return S_OK;
      default:
        return S_OK;
    }
  }

 private:
  enum Child { kChildPage = 0, kChildZoom = 1, kChildFocus = 2 };
  static const wchar_t* ChildAutomationId(Child c) {
    switch (c) {
      case kChildPage:
        return L"page";
      case kChildZoom:
        return L"zoom";
      default:
        return L"focus";
    }
  }

  // The name a screen reader announces for each child (kickoff M1: "Narrator
  // and NVDA announce page, zoom, focus"), capitalised like the spoken word.
  static const wchar_t* ChildName(Child c) {
    switch (c) {
      case kChildPage:
        return L"Page";
      case kChildZoom:
        return L"Zoom";
      default:
        return L"Focus";
    }
  }

  // Children (page, zoom, focus) are static text elements with a stable
  // AutomationId; the screen reader announces each by name (kickoff M1:
  // "Narrator and NVDA announce page, zoom, focus") and can walk them in
  // order via sibling navigation.
  STDMETHODIMP MakeChild(Child which, IRawElementProviderFragment** out) {
    *out = new ChildProvider(hwnd_, which, this);
    return S_OK;
  }

  // A leaf element implementing the fragment pair minimally: parent is the
  // window provider, siblings answer page<->zoom<->focus order. No separate
  // identity (same hwnd host), so UIA treats the whole set as one window.
  class ChildProvider final : public IRawElementProviderFragment, public IRawElementProviderSimple {
   public:
    ChildProvider(HWND hwnd, Child which, WindowProvider* parent)
        : refs_(1), hwnd_(hwnd), which_(which), parent_(parent) {}

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
      if (!ppv)
        return E_POINTER;
      if (riid == IID_IUnknown) {
        AddRef();
        *ppv = static_cast<IUnknown*>(static_cast<IRawElementProviderFragment*>(this));
        return S_OK;
      }
      if (riid == IID_IRawElementProviderFragment) {
        AddRef();
        *ppv = static_cast<IRawElementProviderFragment*>(this);
        return S_OK;
      }
      if (riid == IID_IRawElementProviderSimple) {
        AddRef();
        *ppv = static_cast<IRawElementProviderSimple*>(this);
        return S_OK;
      }
      *ppv = nullptr;
      return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++refs_; }
    STDMETHODIMP_(ULONG) Release() override {
      ULONG n = --refs_;
      if (n == 0)
        delete this;
      return n;
    }

    STDMETHODIMP Navigate(enum NavigateDirection direction,
                          IRawElementProviderFragment** pRetVal) override {
      if (!pRetVal)
        return E_POINTER;
      *pRetVal = nullptr;
      switch (direction) {
        case NavigateDirection_Parent:
          parent_->AddRef();
          *pRetVal = parent_;
          return S_OK;
        case NavigateDirection_NextSibling:
          return MakeSibling(which_ + 1, pRetVal);
        case NavigateDirection_PreviousSibling:
          return MakeSibling(which_ - 1, pRetVal);
        default:
          return S_OK;  // leaves have no children
      }
    }
    STDMETHODIMP GetRuntimeId(SAFEARRAY** pRetVal) override {
      if (pRetVal)
        *pRetVal = nullptr;
      return E_NOTIMPL;
    }
    STDMETHODIMP get_BoundingRectangle(struct UiaRect* pRetVal) override {
      // Children share the window's rectangle; the parent computes it from the
      // hwnd, so pass the request through.
      return parent_->get_BoundingRectangle(pRetVal);
    }
    STDMETHODIMP GetEmbeddedFragmentRoots(SAFEARRAY** pRetVal) override {
      if (pRetVal)
        *pRetVal = nullptr;
      return S_OK;
    }
    STDMETHODIMP SetFocus() override {
      ::SetFocus(hwnd_);
      return S_OK;
    }
    STDMETHODIMP get_FragmentRoot(IRawElementProviderFragmentRoot** pRetVal) override {
      // This fragment is its own subtree's root and has no embedded-root parent;
      // returning nullptr lets the hwnd host act as the fragment root.
      if (pRetVal)
        *pRetVal = nullptr;
      return S_OK;
    }

    STDMETHODIMP get_ProviderOptions(enum ProviderOptions* pRetVal) override {
      if (!pRetVal)
        return E_POINTER;
      *pRetVal = ProviderOptions_ServerSideProvider;
      return S_OK;
    }
    STDMETHODIMP GetPatternProvider(PATTERNID, IUnknown** pRetVal) override {
      if (pRetVal)
        *pRetVal = nullptr;
      return S_OK;
    }
    STDMETHODIMP get_HostRawElementProvider(IRawElementProviderSimple** pRetVal) override {
      if (pRetVal)
        *pRetVal = nullptr;
      return S_OK;
    }
    STDMETHODIMP GetPropertyValue(PROPERTYID property_id, VARIANT* pRetVal) override {
      if (!pRetVal)
        return E_POINTER;
      VariantInit(pRetVal);
      switch (property_id) {
        case UIA_AutomationIdPropertyId:
          pRetVal->vt = VT_BSTR;
          pRetVal->bstrVal = SysAllocString(ChildAutomationId(which_));
          return pRetVal->bstrVal ? S_OK : E_OUTOFMEMORY;
        case UIA_NamePropertyId:
          // The name is what a screen reader announces when focus lands on the
          // child: "Page", "Zoom" or "Focus" (kickoff M1: Narrator and NVDA
          // announce page, zoom, focus). Distinct from the window-level value
          // "Page 1 of 5, zoom 150%", which is the root's ValuePattern.
          pRetVal->vt = VT_BSTR;
          pRetVal->bstrVal = SysAllocString(ChildName(which_));
          return pRetVal->bstrVal ? S_OK : E_OUTOFMEMORY;
        case UIA_ControlTypePropertyId:
          pRetVal->vt = VT_I4;
          pRetVal->lVal = static_cast<LONG>(UIA_TextControlTypeId);
          return S_OK;
        case UIA_IsKeyboardFocusablePropertyId:
          pRetVal->vt = VT_BOOL;
          pRetVal->boolVal = VARIANT_TRUE;
          return S_OK;
        case UIA_IsEnabledPropertyId:
          pRetVal->vt = VT_BOOL;
          pRetVal->boolVal = VARIANT_TRUE;
          return S_OK;
        default:
          return S_OK;
      }
    }

   private:
    STDMETHODIMP MakeSibling(int which, IRawElementProviderFragment** out) {
      if (which < kChildPage || which > kChildFocus) {
        *out = nullptr;
        return S_OK;
      }
      return parent_->MakeChild(static_cast<Child>(which), out);
    }

    ULONG refs_;
    HWND hwnd_;
    Child which_;
    WindowProvider* parent_;
  };

  ULONG refs_;
  HWND hwnd_;
  pc_uia_state* state_;
};

}  // namespace

// Windows API for window.cc: answer WM_GETOBJECT with the window provider.
// Returns true when the message is handled and *lresult holds the provider.
bool pc_uia_handle_wm_getobject(HWND hwnd, WPARAM wparam, LPARAM lparam, pc_uia_state* state,
                                LRESULT* lresult) {
  if (!hwnd || !lresult)
    return false;
  // WM_GETOBJECT with lParam == UiaRootObjectId (-25) is the UIA request; the
  // comparison must pin the signed value, not the LPARAM cast to unsigned.
  if (lparam != static_cast<LPARAM>(UiaRootObjectId))
    return false;
  WindowProvider* provider = new WindowProvider(hwnd, state);
  *lresult = UiaReturnRawElementProvider(hwnd, wparam, lparam,
                                         static_cast<IRawElementProviderSimple*>(provider));
  // UiaReturnRawElementProvider holds its own reference; release ours.
  provider->Release();
  return true;
}

}  // namespace win32
}  // namespace tynypdf