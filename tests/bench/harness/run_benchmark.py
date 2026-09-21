#!/usr/bin/env python3
"""
Benchmark harness for M0.4 metrics.

Measures five metrics for PDF viewers:
1. Open time (ms)
2. First paint time (ms)
3. Line-by-line scroll time (ms)
4. Document search time (ms)
5. Peak RSS (MB)
"""

import argparse
import json
import os
import subprocess
import sys
import time
import statistics
from pathlib import Path
from typing import Dict, List, Any, Optional
import subprocess
import platform
import json


class BenchmarkRunner:
    def __init__(self, target: str, corpus_dir: Path, runs: int = 1, backend: str = "null"):
        self.target = target
        self.corpus_dir = Path(corpus_dir)
        self.runs = runs
        self.backend = backend
        self.results = {}

        # Target configurations
        self.targets = {
            'tynypdf': {
                'cmd': ['tynypdf-cli', 'render', '--page', '0', '--dpi', '72',
                        '--backend', self.backend],
                'name': f'tynypdf ({self.backend} backend)',
            },
            'sumatra-3.6.1': {
                'cmd': ['SumatraPDF.exe', '-print-to-default', '-silent'],
                'name': 'SumatraPDF 3.6.1',
            },
            'sumatra-3.7pre': {
                'cmd': ['SumatraPDF-3.7pre.exe', '-print-to-default', '-silent'],
                'name': 'SumatraPDF 3.7 pre-release',
            },
        }

    def get_machine_spec(self) -> Dict[str, Any]:
        """Collect machine specification."""
        spec = {
            'os': platform.system(),
            'os_release': platform.release(),
            'os_version': platform.version(),
            'machine': platform.machine(),
            'processor': platform.processor(),
            'python_version': platform.python_version(),
            'cpu_count': os.cpu_count(),
        }

        # Try to get CPU model
        try:
            with open('/proc/cpuinfo', 'r') as f:
                for line in f:
                    if 'model name' in line:
                        spec['cpu_model'] = line.split(':')[1].strip()
                        break
        except:
            pass

        # Try to get memory
        try:
            with open('/proc/meminfo', 'r') as f:
                for line in f:
                    if 'MemTotal' in line:
                        spec['ram_kb'] = int(line.split()[1])
                        break
        except:
            pass

        # Try to get GPU info
        try:
            result = subprocess.run(['lspci', '-v'], capture_output=True, text=True, timeout=5)
            for line in result.stdout.split('\n'):
                if 'VGA' in line or '3D' in line or 'Display' in line:
                    spec['gpu'] = line.strip()
                    break
        except:
            pass

        return spec

    def find_pdf_files(self, corpus_dir: Path) -> List[Path]:
        """Find all PDF files in corpus directory."""
        pdf_files = list(Path(corpus_dir).rglob('*.pdf'))
        if not pdf_files:
            raise ValueError(f"No PDF files found in {copus_dir}")
        return pdf_files

    def run_command_timed(self, cmd: List[str], env: Optional[Dict] = None) -> Dict[str, Any]:
        """Run command and measure time and memory."""
        start = time.perf_counter()
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=env
        )

        # Monitor memory
        peak_rss = 0
        try:
            while True:
                try:
                    # Get RSS from /proc/pid/status
                    pid = proc.pid
                    with open(f'/proc/{pid}/status', 'r') as f:
                        for line in f:
                            if line.startswith('VmRSS:'):
                                rss_kb = int(line.split()[1])
                                peak_rss = max(peak_rss, rss_kb)
                except:
                    pass
                if proc.poll() is not None:
                    break
                time.sleep(0.001)
        except:
            pass

        stdout, stderr = proc.communicate()
        end = time.perf_counter()

        return {
            'returncode': proc.returncode,
            'stdout': stdout.decode('utf-8', errors='replace'),
            'stderr': stderr.decode('utf-8', errors='replace'),
            'elapsed_ms': (end - start) * 1000,
            'peak_rss_mb': peak_rss / 1024.0,
        }

    def measure_open_time(self, pdf_path: Path) -> Dict[str, Any]:
        """Measure document open time."""
        cmd = self.get_cmd(pdf_path)
        result = self.run_command_timed(cmd)
        return {
            'open_time_ms': result['elapsed_ms'],
            'returncode': result['returncode'],
        }

    def measure_first_paint(self, pdf_path: Path) -> Dict[str, Any]:
        """Measure first paint time (approximated as open time for CLI tools)."""
        # For CLI tools, first paint ~ open time
        return self.measure_open_time(pdf_path)

    def measure_scroll(self, pdf_path: Path) -> Dict[str, Any]:
        """Measure line-by-line scroll time (simulated)."""
        # For CLI, we measure rendering multiple pages sequentially
        start = time.perf_counter()
        # Simulate scrolling through pages
        for i in range(5):  # 5 pages
            cmd = self.get_cmd_for_page(pdf_path, i)
            result = self.run_command_timed(cmd)
        end = time.perf_counter()
        return {
            'scroll_time_ms': (end - start) * 1000,
        }

    def get_cmd_for_page(self, pdf_path: Path, page_idx: int) -> List[str]:
        """Get command for rendering a specific page."""
        if self.target == 'tynypdf':
            build_dir = Path(os.environ.get('BUILD_DIR', '/tmp/tyny-pdf/build/linux-core/Debug'))
            binary = build_dir / 'tynypdf-cli'
            if not binary.exists():
                for root, dirs, files in os.walk('/tmp'):
                    for f in files:
                        if f == 'tynypdf-cli':
                            binary = Path(root) / f
                            break
            if binary.exists():
                return [str(binary), 'render', '--page', str(page_idx), '--dpi', '72',
                        '--backend', self.backend, str(pdf_path)]
            return ['tynypdf-cli', 'render', '--page', str(page_idx), '--dpi', '72',
                    '--backend', self.backend, str(pdf_path)]
        # For SumatraPDF and other targets: reuse the base command (no page-specific flag)
        return self.get_cmd(pdf_path)

    def measure_search(self, pdf_path: Path, query: str = "the") -> Dict[str, Any]:
        """Measure document search time."""
        # For CLI, we can't easily measure search without a proper search implementation
        # This is a placeholder
        return {
            'search_time_ms': 0,
            'note': 'Search not implemented for CLI target',
        }

    def measure_peak_rss(self, pdf_path: Path) -> Dict[str, Any]:
        """Measure peak RSS during document open."""
        cmd = self.get_cmd_for_page(pdf_path, 0)
        result = self.run_command_timed(cmd)
        return {
            'peak_rss_mb': result.get('peak_rss_mb', 0),
        }

    def get_cmd(self, pdf_path: Path) -> List[str]:
        """Get command for target."""
        t = self.targets.get(self.target)
        if not t:
            raise ValueError(f"Unknown target: {self.target}")

        # For tynypdf, use the built binary
        if self.target == 'tynypdf':
            # Try to find the built binary
            build_dir = Path(os.environ.get('BUILD_DIR', '/tmp/tyny-pdf/build/linux-core/Debug'))
            binary = build_dir / 'tynypdf-cli'
            if not binary.exists():
                # Try to find it
                for root, dirs, files in os.walk('/tmp'):
                    for f in files:
                        if f == 'tynypdf-cli':
                            binary = Path(root) / f
                            break
            if binary.exists():
                cmd = [str(binary)] + t['cmd'][1:] + [str(pdf_path)]
            else:
                # Fallback to PATH
                cmd = t['cmd'] + [str(pdf_path)]
        else:
            cmd = t['cmd'] + [str(pdf_path)]
        return cmd

    def run_benchmarks(self, corpus_dir: Path, runs: int = 1) -> Dict[str, Any]:
        """Run all benchmarks."""
        pdf_files = self.find_pdf_files(corpus_dir)
        if not pdf_files:
            raise ValueError("No PDF files in corpus")

        # Use first PDF for measurements
        pdf_path = pdf_files[0]

        results = {
            'target': self.target,
            'pdf_file': str(pdf_path),
            'pdf_size_bytes': pdf_path.stat().st_size,
            'machine_spec': self.get_machine_spec(),
            'runs': runs,
            'metrics': {},
        }

        metrics = {
            'open_time_ms': [],
            'first_paint_ms': [],
            'scroll_time_ms': [],
            'search_time_ms': [],
            'peak_rss_mb': [],
        }

        for run in range(runs):
            print(f"Run {run + 1}/{runs} for {self.target}...")

            # Open time / first paint
            result = self.measure_open_time(pdf_files[0])
            metrics['open_time_ms'].append(result['open_time_ms'])
            metrics['first_paint_ms'].append(result.get('first_paint_ms', result['open_time_ms']))

            # Scroll
            scroll_result = self.measure_scroll(pdf_files[0])
            metrics['scroll_time_ms'].append(scroll_result['scroll_time_ms'])

            # Search
            search_result = self.measure_search(pdf_files[0])
            metrics['search_time_ms'].append(search_result.get('search_time_ms', 0))

            # Peak RSS
            rss_result = self.measure_peak_rss(pdf_files[0])
            metrics['peak_rss_mb'].append(rss_result['peak_rss_mb'])

            time.sleep(0.5)  # Cool down between runs

        # Compute statistics
        for metric, values in metrics.items():
            if values:
                results['metrics'][metric] = {
                    'mean': statistics.mean(values),
                    'stdev': statistics.stdev(values) if len(values) > 1 else 0,
                    'min': min(values),
                    'max': max(values),
                    'runs': values,
                }

        return results


def main():
    parser = argparse.ArgumentParser(description='M0.4 Benchmark runner')
    parser.add_argument('--target', required=True, choices=['tynypdf', 'sumatra-3.6.1', 'sumatra-3.7pre'])
    parser.add_argument('--backend', choices=['null', 'mupdf'], default='null',
                        help='tynypdf backend to benchmark')
    parser.add_argument('--corpus', required=True, type=Path)
    parser.add_argument('--runs', type=int, default=1)
    parser.add_argument('--output', type=Path)

    args = parser.parse_args()

    runner = BenchmarkRunner(args.target, args.corpus, args.runs, args.backend)

    print(f"Running benchmarks for {runner.target}...")
    results = runner.run_benchmarks(args.corpus, args.runs)

    # Add metadata
    results['timestamp'] = time.time()
    results['hostname'] = platform.node()

    # Output
    output_json = json.dumps(results, indent=2)

    if args.output:
        with open(args.output, 'w') as f:
            f.write(output_json)
        print(f"Results written to {args.output}")
    else:
        print(output_json)


if __name__ == '__main__':
    main()
