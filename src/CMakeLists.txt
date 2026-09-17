# Vertical slices under src/core, seams where a second implementation is expected
# (ADR-0002). The dependency arrow points inward: core, then backends, then apps.
add_subdirectory(core)
add_subdirectory(backends)
add_subdirectory(app)
add_subdirectory(cli)
