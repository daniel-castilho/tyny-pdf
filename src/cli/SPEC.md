# CLI Capability Specification

## Requirements

### R12.1 The CLI SHALL render a specified page to a PNG file.

Verification: unit:tests/unit/test_cli_exit_codes.cc

### R12.2 The CLI SHALL return exit code 0 on success, 1 on corrupt file, 2 on argument error, 3 on unsupported.

Verification: unit:tests/unit/test_cli_exit_codes.cc

## Out of scope

- batch processing
- interactive mode
- text extraction
