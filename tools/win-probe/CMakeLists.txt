# tools/win-probe - the four ASK questions of ADR-0010 section 2, built as small
# executables so each claim is answered by output instead of assumed. The ABIS probe is
# toolchain-neutral (built by linux-core too); the Windows-only probes build when the
# target system is Windows, i.e. under win-cross-x64 and windows-msvc.
#
# The CI matrix runs these through the "winprobe_test" aggregate: whatever a particular
# configuration offers, the aggregate's dependency list is the checklist. The probes
# themselves run through the WSL interop path or on a windows-latest runner.

set(tynypdf_winprobe_targets "")

# ABIS probe: the story 1.4 surface mirror. Host and cross builds emit identical
# pc_* symbol tables; CI diffs them (nm -C --defined-only) so a toolchain-specific
# mangling or calling-convention drift fails the job instead of shipping.
add_library(tynypdf-abi-probe STATIC abi_probe.cc)
pc_apply_core_flags(tynypdf-abi-probe)
list(APPEND tynypdf_winprobe_targets tynypdf-abi-probe)

if(WIN32)
  # Runtime probe: a statically linked exe must not map a compiler runtime DLL.
  add_executable(winprobe_runtime runtime_probe.c)
  pc_apply_warnings(winprobe_runtime)
  if(MSVC)
    # /MT on MSVC mirrors the MinGW -static contract (ADR-0010 section 2, D7b).
    set_target_properties(winprobe_runtime PROPERTIES
      MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
  list(APPEND tynypdf_winprobe_targets winprobe_runtime)

  # Direct2D/DirectWrite/D3D11/DXGI surface probe. The factory7-era interfaces sit behind
  # NTDDI_VERSION guards in the MinGW headers, and the same headers leave C mode as an
  # explicit "FIXME: Add full C declaration" stub, so this file keeps its documented
  # name but compiles as C++ (ADR-0010 "Unverified" item 1's failure mode, measured).
  add_executable(winprobe_d2d d2d_probe.c)
  set_source_files_properties(d2d_probe.c PROPERTIES LANGUAGE CXX)
  target_compile_definitions(winprobe_d2d PRIVATE
    _WIN32_WINNT=0x0A00 NTDDI_VERSION=0x0A00000B)
  target_link_libraries(winprobe_d2d PRIVATE d2d1 dwrite d3d11 dxgi)
  pc_apply_warnings(winprobe_d2d)
  list(APPEND tynypdf_winprobe_targets winprobe_d2d)

  # GPU adapter probe: hardware adapter required for the docs' hardware path to hold.
  add_executable(winprobe_gpu gpu_probe.cpp)
  target_compile_definitions(winprobe_gpu PRIVATE
    _WIN32_WINNT=0x0A00 NTDDI_VERSION=0x0A00000B)
  target_link_libraries(winprobe_gpu PRIVATE d3d11 dxgi)
  pc_apply_warnings(winprobe_gpu)
  list(APPEND tynypdf_winprobe_targets winprobe_gpu)
endif()

add_custom_target(winprobe_test ALL DEPENDS ${tynypdf_winprobe_targets})
