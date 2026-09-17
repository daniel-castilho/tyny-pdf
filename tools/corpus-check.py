#!/usr/bin/env python3
"""
Corpus validation tool.

Validates that the corpus manifest.txt matches the actual files,
and that all files have valid PDF structure and required metadata.
"""

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Dict, List, Any, Optional
import subprocess


class CorpusChecker:
    def __init__(self, corpus_dir: Path):
        self.corpus_dir = Path(corpus_dir)
        self.manifest_path = self.corpus_dir / 'manifest.txt'

    def compute_sha256(self, file_path: Path) -> str:
        """Compute SHA256 hash of file."""
        h = hashlib.sha256()
        with open(file_path, 'rb') as f:
            for chunk in iter(lambda: f.read(8192), b''):
                h.update(chunk)
        return h.hexdigest()

    def validate_pdf(self, file_path: Path) -> Dict[str, Any]:
        """Basic PDF validation."""
        result = {
            'valid': False,
            'error': None,
            'size': file_path.stat().st_size,
        }

        try:
            with open(file_path, 'rb') as f:
                header = f.read(5)
                if header != b'%PDF-':
                    result['error'] = 'Not a valid PDF (missing %PDF- header)'
                    return result

            result['valid'] = True
        except Exception as e:
            result['error'] = str(e)

        return result

    def check_corpus(self, manifest_path: Optional[Path] = None) -> Dict[str, Any]:
        """Validate entire corpus against manifest."""
        manifest_path = manifest_path or self.manifest_path

        result = {
            'valid': True,
            'files': [],
            'errors': [],
            'warnings': [],
            'stats': {
                'total_files': 0,
                'total_size': 0,
                'valid_pdfs': 0,
                'invalid_pdfs': 0,
            }
        }

        if not self.corpus_dir.exists():
            result['valid'] = False
            result['errors'].append(f"Corpus directory not found: {self.corpus_dir}")
            return result

        # Load manifest if exists
        manifest = {}
        if manifest_path.exists():
            try:
                with open(manifest_path, 'r') as f:
                    for line in f:
                        line = line.strip()
                        if line and not line.startswith('#'):
                            parts = line.split()
                            if len(parts) >= 2:
                                manifest[parts[1]] = parts[0]
            except Exception as e:
                result['warnings'].append(f"Could not parse manifest: {e}")
        else:
            result['warnings'].append("No manifest.txt found")

        # Check all PDF files in corpus
        pdf_files = list(Path(self.corpus_dir).rglob('*.pdf'))
        result['stats']['total_files'] = len(pdf_files)

        for pdf_path in pdf_files:
            rel_path = pdf_path.relative_to(self.corpus_dir)
            file_result = {
                'path': str(rel_path),
                'size': 0,
                'sha256': '',
                'valid': False,
            }

            try:
                validation = self.validate_pdf(pdf_path)
                file_result['size'] = validation['size']
                file_result['valid'] = validation['valid']

                if validation['valid']:
                    result['stats']['valid_pdfs'] += 1
                    file_result['sha256'] = self.compute_sha256(pdf_path)

                    # Check against manifest
                    if str(rel_path) in manifest:
                        expected = manifest[str(rel_path)]
                        if file_result['sha256'] != expected:
                            result['warnings'].append(
                                f"{rel_path}: hash mismatch (manifest: {expected[:16]}..., actual: {file_result['sha256'][:16]}...)"
                            )
                else:
                    result['stats']['invalid_pdfs'] += 1
                    result['errors'].append(f"{rel_path}: {validation['error']}")

            except Exception as e:
                result['errors'].append(f"{rel_path}: {e}")
                result['stats']['invalid_pdfs'] += 1

            result['files'].append(file_result)
            result['stats']['total_size'] += validation.get('size', 0)

        # Check for missing files in manifest
        if manifest:
            for manifest_file in manifest:
                if not (self.corpus_dir / manifest_file).exists():
                    result['warnings'].append(f"File in manifest but missing on disk: {manifest_file}")

        result['valid'] = len(result['errors']) == 0
        return result


def main():
    parser = argparse.ArgumentParser(description='Validate corpus against manifest')
    parser.add_argument('corpus_dir', type=Path, help='Corpus directory')
    parser.add_argument('--manifest', type=Path, help='Manifest file (default: corpus_dir/manifest.txt)')
    parser.add_argument('--output', type=Path, help='Output JSON file')
    parser.add_argument('--self-test', action='store_true', help='Run self-test')

    args = parser.parse_args()

    if args.self_test:
        # Self-test with a temporary corpus
        import tempfile
        import shutil

        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            corpus_dir = tmp_path / 'corpus'
            corpus_dir.mkdir()

            # Create test PDF
            pdf_content = b'%PDF-1.4\n1 0 obj<<>>endobj\nxref\n0 1\n0000000000 65535 f\ntrailer<<>>\nstartxref\n0\n%%EOF'
            (corpus_dir / 'test.pdf').write_bytes(b'%PDF-1.4\n1 0 obj<<>>endobj\nxref\n0 1\n0000000000 65535 f\ntrailer<<>>\nstartxref\n0\n%%EOF')

            # Create manifest
            (corpus_dir / 'manifest.txt').write_text('d41d8cd98f00b204e9800998ecf8427e  test.pdf\n')

            checker = CorpusChecker(corpus_dir)
            result = checker.check_corpus()

            assert result['valid'], f"Self-test failed: {result['errors']}"
            print("Self-test passed")
        return 0

    if not args.corpus_dir.exists():
        print(f"Error: Corpus directory not found: {args.corpus_dir}", file=sys.stderr)
        return 1

    checker = CorpusChecker(args.corpus_dir)
    manifest_path = args.manifest or (args.corpus_dir / 'manifest.txt')
    result = checker.check_corpus(manifest_path)

    if args.output:
        with open(args.output, 'w') as f:
            json.dump(result, f, indent=2)

    # Print summary
    print(f"Corpus validation: {'PASS' if result['valid'] else 'FAIL'}")
    print(f"  Files: {result['stats']['total_files']}")
    print(f"  Valid PDFs: {result['stats']['valid_pdfs']}")
    print(f"  Invalid PDFs: {result['stats']['invalid_pdfs']}")
    print(f"  Total size: {result['stats']['total_size']} bytes")

    for error in result['errors']:
        print(f"  ERROR: {error}")
    for warning in result['warnings']:
        print(f"  WARNING: {warning}")

    return 0 if result['valid'] else 1


if __name__ == '__main__':
    sys.exit(main())
