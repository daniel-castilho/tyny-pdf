# Naming: single source of truth

Every product identifier in this repository is derived from the table below. `AGENTS.md` forbids
hand-editing a derived value: `tools/naming-sync.py` generates `generated/naming.env` and
`generated/naming.cmake` from this file, and `tools/naming-sync.py check` fails the build when a
generated value differs from what is committed or when a retired brand token is found in the tree.

Machine-readable form (consumed by `tools/naming-sync.py`):

```yaml
brand:
  product_name: "Tyny PDF"
  product_name_long: "Tyny PDF Reader"
  publisher: "Tyny"
  tagline: "An offline PDF reader that keeps your annotations in a file you can diff."
  repository: "tyny-pdf"

identifiers:
  app_id: "ca.tyny.pdf"            # reverse DNS of tyny.ca; used on every platform
  windows_appid: "Tyny.TynyPDF"    # AppUserModelID
  windows_registry: "Software\\Tyny\\TynyPDF"
  windows_registry_policy: "Software\\Policies\\Tyny\\TynyPDF"
  linux_desktop_file: "ca.tyny.pdf.desktop"
  linux_instance: "ca.tyny.pdf"    # GApplication id
  macos_bundle_id: "ca.tyny.pdf"
  winget_package_id: "Tyny.TynyPDF"
  msix_package_name: "Tyny.TynyPDF"

executables:
  viewer: "tynypdf"
  cli: "tynypdf-cli"
  render_worker: "tynypdf-worker"  # out-of-process engine, ADR-0002

library:
  core_target: "pdfcore"
  public_header: "pdfcore.h"
  backend_dll_prefix: "tynypdf-backend-"  # e.g. tynypdf-backend-mupdf.dll
  render_dll_prefix: "tynypdf-render-"    # e.g. tynypdf-render.dll
  abi_namespace: "pc_"                    # pdfcore C symbols; brand-free on purpose

paths:
  config_windows: "%APPDATA%\\Tyny\\TynyPDF"
  config_linux: "tyny/tynypdf"            # relative to XDG_CONFIG_HOME
  config_macos: "Tyny PDF"                # relative to ~/Library/Application Support
  cache_linux: "tyny/tynypdf"
  sidecar_suffix: ".tynypdf.json"         # frozen at first public release
  sidecar_alt_suffix: ".tynypdf.lock"     # write-lock marker

env:
  prefix: "TYNYPDF_"                      # TYNYPDF_DISABLE_UPDATE, TYNYPDF_BACKEND, ...
  log_subsystems: ["tynypdf.core", "tynypdf.backend.mupdf", "tynypdf.ui", "tynypdf.cli"]
  log_target_name: "ca.tyny.pdf"

release:
  artifact_stem: "tynypdf"                       # tynypdf-0.1.0-win-x64.zip
  installer_msi_stem: "TynyPDF"                  # TynyPDF-0.1.0-x64.msi
  news_feed_path: "/releases/tynypdf/latest.json"

retired_tokens:
  - "Recto"                                 # ADR-0006, superseded by ADR-0008 on 2026-09-16
```

## Rules

1. The brand token appears once per artifact, never as a substring inside a generic word.
2. C symbols stay in the `pc_` namespace. The library is a component, not the brand: this keeps
   a future donation of `pdfcore` upstream free of a rename.
3. `sidecar_suffix` and `windows_registry` are frozen when the first public release ships. Before
   that they are cheap to change; after that they need a migration and a deprecation window.
4. The engine brand (`MuPDF`, `Artifex`) never appears in the product name, the tagline, an
   environment variable or a file name. It appears in `README.md`, in the about dialog and in
   `LICENSE.third-party` (generated at build time from the SBOM, ADR-0004), where attribution
   belongs.
5. Renaming procedure: edit this file, run `python3 tools/naming-sync.py write &&
   tools/check.sh` (which fails on any retired token), then run the test suite. Anything
   that still contains the old token is a bug in the sync tool, not in the source tree.
6. Every retired token is appended to `retired_tokens` and never removed, so the check keeps
   guarding the history instead of repeating it.

## Swap history

- 2026-09-16: `Recto` to `Tyny PDF` (ADR-0008). Executed by editing this file and running the
  sync; 7 files changed, including the fixture rename `example.recto.json` to
  `example.tynypdf.json` and the schema re-ID to `urn:tynypdf:sidecar:1`. Every check passed
  afterwards, which is the evidence that rule 5 is real rather than aspirational.
