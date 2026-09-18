# Tyny PDF Conformance Corpus

This directory contains a clause-keyed corpus of PDF files for ISO 32000-1/2 conformance testing.
Each file is keyed by its ISO 32000 clause and documents the expected outcome.

## Structure

```
tests/conformance/
├── MANIFEST.md           # This file
├── iso-32000-1/          # ISO 32000-1 (PDF 1.7) clauses
│   ├── 7.1/              # File structure
│   ├── 7.2/              # Syntax
│   ├── 7.3/              # Objects
│   ├── 7.4/              # Filters
│   ├── 7.5/              # File specification
│   ├── 7.5.2/            # File streams
│   ├── 7.5.3/            # File compression
│   ├── 7.5.4/            # File encryption
│   ├── 7.6/              # Document structure
│   ├── 7.7/              # Content streams
│   ├── 7.8/              # Text
│   ├── 7.9/              # Images
│   ├── 7.10/             # Forms
│   ├── 7.11/             # Annotations
│   ├── 7.12/             # Actions
│   ├── 7.13/             # Metadata
│   ├── 7.14/             # Output intents
│   ├── 7.15/             # Page tree
│   └── 7.16/             # Page description
├── iso-32000-2/          # ISO 32000-2 (PDF 2.0) clauses
│   ├── 12.1/             # General
│   ├── 12.2/             # File structure
│   ├── 12.3/             # Syntax
│   ├── 12.4/             # Objects
│   ├── 12.5/             # Filters
│   ├── 12.6/             # File specification
│   ├── 12.7/             # Document structure
│   ├── 12.8/             # Page tree
│   ├── 12.9/             # Page description
│   ├── 12.10/            # Text
│   ├── 12.11/            # Images
│   ├── 12.11.5/          # Fonts
│   ├── 12.11.6/          # Color
│   ├── 12.11.7/          # Patterns
│   ├── 12.11.8/          # Shadings
│   ├── 12.12/            # Transparency
│   ├── 12.13/            # Document structure
│   ├── 12.13.5/          # Optional content
│   ├── 12.13.6/          # Annotations
│   ├── 12.13.7/          # Actions
│   ├── 12.13.8/          # Forms
│   ├── 12.13.9/          # Signatures
│   └── 12.13.10/         # Output intents
└── iso-19005/            # PDF/A (ISO 19005) clauses
    ├── part-1/           # PDF/A-1
    ├── part-2/           # PDF/A-2
    └── part-3/           # PDF/A-3
```

## File Naming Convention

Each test file follows the pattern:
```
<iso-standard>-<clause>-<description>-<expected-outcome>.pdf
```

Examples:
- `iso-32000-1-7.8.2-text-encoding-valid.pdf`
- `iso-32000-2-12.11.5.2-font-embedding-valid.pdf`
- `iso-19005-1-6.2-font-embedding-invalid.pdf`

## Expected Outcome Values

- `valid` - File should pass validation
- `invalid` - File should fail validation
- `warning` - File should produce warnings but pass
- `error` - File should produce specific error

## Metadata

Each clause directory may contain a `manifest.json` with:
```json
{
  "clause": "7.8.2",
  "standard": "iso-32000-1",
  "description": "Text encoding requirements",
  "files": [
    {
      "path": "iso-32000-1-7.8.2-text-encoding-valid.pdf",
      "expected": "valid",
      "sha256": "...",
      "size": 12345
    }
  ]
}
```

## Verification

Run verification with:
```bash
# Using veraPDF
verapdf --format xml --profile /path/to/profile.xml test.pdf

# Or using the test harness
python3 tools/bench/harness/run_verapdf.py --corpus tests/conformance --output results.xml
```

## Adding New Files

1. Place the PDF in the appropriate clause directory
2. Update the clause's `manifest.json`
3. Run `verapdf` to verify the expected outcome
4. Commit both the PDF and the manifest

## Maintenance

- Corpus is a submodule pinned at a specific commit
- New files added via PR with `verapdf` results attached
- `tools/spec-check.py` ensures every file has a manifest entry
