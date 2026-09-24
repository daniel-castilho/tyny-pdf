#!/usr/bin/env python3
"""
Benchmark harness for M0.4 / Epic 5 metrics.

Measures five metrics for PDF viewers:
1. Open time (ms)
2. First paint time (ms)
3. Line-by-line scroll time (ms)
4. Document search time (ms)
5. Peak RSS (MB)

Extended for Epic 5:
- --binary: path to tynypdf.exe (Windows binary via WSL interop)
- --record-machine: include machine_spec in output
- --compare: compare two JSON runs within 10% tolerance
- --machine: override machine_spec for cross-machine compare
- peak_rss_kib: peak RSS in KiB (Linux /proc only; the Windows probe slipped past story 5.3
  and is tracked in issue #55)

Story 1.5 (R31.1): --bench viewer runs tynypdf.exe --bench over the 1000-page
corpus. The exe drives its own frame loop (TYNYPDF_FRAMES auto-quit) and emits
fwd_frame_ms/ret_frame_ms/full_frame_ms plus per-phase peak_rss_kib JSON
(R30.2's 250 MiB ceiling and R15.1's blit p99 are read from that file); this
harness only launches it, merges machine_spec, and feeds --compare.
"""

import argparse
import json
import os
import subprocess
import sys
import tempfile
import time
import statistics
from pathlib import Path
from typing import Dict, List, Any, Optional
import subprocess
import platform
import json


class BenchmarkRunner:
    def __init__(self, target: str, corpus_dir: Path, runs: int = 1, backend: str = "null",
                 binary: Optional[str] = None, record_machine: bool = False):
        self.target = target
        self.corpus_dir = Path(corpus_dir)
        self.runs = runs
        self.backend = backend
        self.binary = binary
        self.record_machine = record_machine
        self.results = {}

        # Target configurations
        self.targets = {
            'tynypdf': {
                'cmd_template': ['{binary}', 'render', '--page', '0', '--dpi', '72',
                                 '--backend', self.backend],
                'name': f'tynypdf ({self.backend} backend)',
            },
            'sumatra-3.6.1': {
                'cmd_template': ['SumatraPDF.exe', '-print-to-default', '-silent'],
                'name': 'SumatraPDF 3.6.1',
            },
            'sumatra-3.7pre': {
                'cmd_template': ['SumatraPDF-3.7pre.exe', '-print-to-default', '-silent'],
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
            raise ValueError(f"No PDF files found in {corpus_dir}")
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

        # Monitor memory (Linux /proc)
        peak_rss = 0
        try:
            while True:
                try:
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
            'peak_rss_kib': peak_rss,
        }

    def get_cmd(self, pdf_path: Path) -> List[str]:
        """Get command for target."""
        t = self.targets.get(self.target)
        if not t:
            raise ValueError(f"Unknown target: {self.target}")

        if self.target == 'tynypdf':
            if not self.binary:
                raise ValueError("tynypdf target requires --binary")
            # Resolve binary path
            binary_path = Path(self.binary)
            if not binary_path.is_absolute():
                # Try relative to build dir
                build_dir = Path(os.environ.get('BUILD_DIR', '/tmp/tyny-pdf/build/linux-core/Debug'))
                candidate = build_dir / self.binary
                if candidate.exists():
                    binary_path = candidate
            cmd = [str(binary_path)] + t['cmd_template'][1:] + [str(pdf_path)]
        else:
            cmd = t['cmd_template'] + [str(pdf_path)]
        return cmd

    def get_cmd_for_page(self, pdf_path: Path, page_idx: int) -> List[str]:
        """Get command for rendering a specific page."""
        if self.target == 'tynypdf':
            if not self.binary:
                raise ValueError("tynypdf target requires --binary")
            binary_path = Path(self.binary)
            if not binary_path.is_absolute():
                build_dir = Path(os.environ.get('BUILD_DIR', '/tmp/tyny-pdf/build/linux-core/Debug'))
                candidate = build_dir / self.binary
                if candidate.exists():
                    binary_path = candidate
            return [str(binary_path), 'render', '--page', str(page_idx), '--dpi', '72',
                    '--backend', self.backend, str(pdf_path)]
        return self.get_cmd(pdf_path)

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
        return self.measure_open_time(pdf_path)

    def measure_scroll(self, pdf_path: Path) -> Dict[str, Any]:
        """Measure line-by-line scroll time (simulated)."""
        start = time.perf_counter()
        for i in range(5):
            cmd = self.get_cmd_for_page(pdf_path, i)
            result = self.run_command_timed(cmd)
        end = time.perf_counter()
        return {
            'scroll_time_ms': (end - start) * 1000,
        }

    def measure_search(self, pdf_path: Path, query: str = "the") -> Dict[str, Any]:
        """Measure document search time."""
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
            'peak_rss_kib': result.get('peak_rss_kib', 0),
        }

    def run_benchmarks(self, corpus_dir: Path, runs: int = 1) -> Dict[str, Any]:
        """Run all benchmarks."""
        pdf_files = self.find_pdf_files(corpus_dir)
        if not pdf_files:
            raise ValueError("No PDF files in corpus")

        pdf_path = pdf_files[0]

        results = {
            'target': self.target,
            'pdf_file': str(pdf_path),
            'pdf_size_bytes': pdf_path.stat().st_size,
            'runs': runs,
            'metrics': {},
        }

        if self.record_machine:
            results['machine_spec'] = self.get_machine_spec()

        metrics = {
            'open_time_ms': [],
            'first_paint_ms': [],
            'scroll_time_ms': [],
            'search_time_ms': [],
            'peak_rss_mb': [],
            'peak_rss_kib': [],
        }

        for run in range(runs):
            print(f"Run {run + 1}/{runs} for {self.target}...")

            result = self.measure_open_time(pdf_files[0])
            metrics['open_time_ms'].append(result['open_time_ms'])
            metrics['first_paint_ms'].append(result.get('first_paint_ms', result['open_time_ms']))

            scroll_result = self.measure_scroll(pdf_files[0])
            metrics['scroll_time_ms'].append(scroll_result['scroll_time_ms'])

            search_result = self.measure_search(pdf_files[0])
            metrics['search_time_ms'].append(search_result.get('search_time_ms', 0))

            rss_result = self.measure_peak_rss(pdf_files[0])
            metrics['peak_rss_mb'].append(rss_result['peak_rss_mb'])
            metrics['peak_rss_kib'].append(rss_result['peak_rss_kib'])

            time.sleep(0.5)

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

    def run_viewer_bench(self, corpus_dir: Path, tiles: int, rows: int) -> Dict[str, Any]:
        """Run tynypdf.exe --bench over the corpus and fold in its JSON.

        The exe owns the frame loop (R31.1): forward pass, return pass, then
        the full-region pass, ending via the TYNYPDF_FRAMES budget. It writes
        frame_ms and peak_rss_kib runs itself (Windows psapi; /proc cannot see
        a WSL-interop process), so this harness never re-measures a metric the
        exe already measured - it only launches and merges machine_spec.
        """
        if not self.binary:
            raise ValueError("viewer bench requires --binary")
        binary_path = Path(self.binary)
        if not binary_path.exists():
            raise ValueError(f"binary not found: {self.binary}")

        # Prefer the 1000-page corpus; fall back to the first PDF found.
        big = sorted(Path(corpus_dir).rglob('corpus-1000p.pdf'))
        pdf_files = big if big else self.find_pdf_files(corpus_dir)
        pdf_path = pdf_files[0]

        with tempfile.TemporaryDirectory(prefix='tynypdf-bench-') as tmp:
            exe_json = Path(tmp) / 'bench.json'
            env = os.environ.copy()
            env['TYNYPDF_BENCH_JSON'] = str(exe_json)
            env['TYNYPDF_BENCH_TILES'] = str(tiles)
            env['TYNYPDF_BENCH_ROWS'] = str(rows)
            # Keep the bench's UI timing log away from the viewer's.
            env['TYNYPDF_UI_LOG'] = str(Path(tmp) / 'tynypdf.bench.log')
            # WSL interop only forwards variables named in WSLENV; /w passes
            # them (with path translation) to the Windows process.
            wslenv = env.get('WSLENV', '')
            env['WSLENV'] = (wslenv + ':' if wslenv else '') + \
                'TYNYPDF_BENCH_JSON/w:TYNYPDF_BENCH_TILES/w:TYNYPDF_BENCH_ROWS/w:TYNYPDF_UI_LOG/w'
            proc = subprocess.run(
                [str(binary_path), '--bench', str(pdf_path)],
                capture_output=True, text=True, env=env, timeout=600,
            )
            if proc.returncode != 0:
                raise ValueError(
                    f"tynypdf --bench failed (exit {proc.returncode}): {proc.stderr.strip()}")
            if not exe_json.exists():
                raise ValueError("tynypdf --bench produced no JSON output")
            with open(exe_json) as f:
                exe_results = json.load(f)

        results = {
            'target': self.target,
            'bench': 'viewer',
            'pdf_file': str(pdf_path),
            'pdf_size_bytes': pdf_path.stat().st_size,
            'page_count': exe_results.get('page_count', 0),
            'tiles_per_page': exe_results.get('tiles_per_page', tiles),
            'full_settle_frames': exe_results.get('full_settle_frames', 0),
            'full_teardown_ms': exe_results.get('full_teardown_ms', 0.0),
            'metrics': exe_results.get('metrics', {}),
        }
        if self.record_machine:
            results['machine_spec'] = self.get_machine_spec()
        return results

    @staticmethod
    def compare_results(file1: Path, file2: Path, tolerance: float = 0.10,
                        machine_override: Optional[str] = None) -> int:
        """Compare two benchmark JSON files. Returns 0 if within tolerance, 1 otherwise.
        If machine_override == 'other', forces a cross-machine mismatch (exit 4).

        The tolerance has an absolute noise floor of 0.5 units: story 1.5's
        steady-state frame times sit near 0.5 ms, where 10% of the mean is
        50 us - below timer and scheduler granularity, so two identical runs
        of the same binary would otherwise flunk the compare. The floor is
        scale-neutral: coarse metrics (open_time_ms, peak_rss_kib) are many
        orders above it and keep the strict relative check."""
        NOISE_FLOOR = 0.5
        if not file1.exists() or not file2.exists():
            print(f"Error: Compare file not found: {file1 if not file1.exists() else file2}")
            return 1

        with open(file1) as f:
            r1 = json.load(f)
        with open(file2) as f:
            r2 = json.load(f)

        # Cross-machine check
        if machine_override == 'other':
            print("Cross-machine compare: machine_spec override 'other' -> forced mismatch")
            return 4

        m1 = r1.get('machine_spec', {})
        m2 = r2.get('machine_spec', {})
        if m1 and m2 and m1 != m2:
            print(f"Cross-machine detect: machine_spec differs")
            return 4

        # Metric tolerance check
        metrics1 = r1.get('metrics', {})
        metrics2 = r2.get('metrics', {})

        for key in metrics1:
            if key not in metrics2:
                print(f"Metric {key} missing in second run")
                return 1
            v1 = metrics1[key].get('mean', 0)
            v2 = metrics2[key].get('mean', 0)
            if v1 == 0 and v2 == 0:
                continue
            diff = abs(v1 - v2)
            threshold = max(tolerance * max(abs(v1), abs(v2)), NOISE_FLOOR)
            if diff > threshold:
                print(f"Metric {key} exceeds tolerance: {v1} vs {v2} "
                      f"(diff {diff:.3f} > {threshold:.3f})")
                return 1

        print(f"All metrics within {tolerance*100:.0f}% tolerance "
              f"(noise floor {NOISE_FLOOR})")
        return 0


def main():
    parser = argparse.ArgumentParser(description='M0.4 / Epic 5 Benchmark runner', add_help=False)
    parser.add_argument('--target', choices=['tynypdf', 'sumatra-3.6.1', 'sumatra-3.7pre'],
                        help='Target to benchmark')
    parser.add_argument('--backend', choices=['null', 'mupdf'], default='null',
                        help='tynypdf backend to benchmark')
    parser.add_argument('--corpus', type=Path, help='Corpus directory')
    parser.add_argument('--runs', type=int, default=1)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--binary', type=str, help='Path to tynypdf.exe for tynypdf target')
    parser.add_argument('--record-machine', action='store_true', help='Include machine_spec in output')
    parser.add_argument('--compare', nargs=2, type=Path, metavar=('FILE1', 'FILE2'),
                        help='Compare two JSON runs within tolerance')
    parser.add_argument('--machine', type=str, help='Override machine_spec for cross-machine compare')
    parser.add_argument('--bench', choices=['viewer'], help='Viewer bench mode (story 1.5, R31.1)')
    parser.add_argument('--tiles', type=int, default=3,
                        help='Tiles per page strip for the viewer bench (TYNYPDF_BENCH_TILES)')
    parser.add_argument('--rows', type=int, default=1,
                        help='Strip rows for the viewer bench (TYNYPDF_BENCH_ROWS)')
    parser.add_argument('--help', action='help', help='Show this help message and exit')

    args = parser.parse_args()

    if args.compare:
        exit_code = BenchmarkRunner.compare_results(args.compare[0], args.compare[1],
                                                     machine_override=args.machine)
        sys.exit(exit_code)

    if not args.target:
        parser.error("--target is required")
    if not args.corpus:
        parser.error("--corpus is required")

    runner = BenchmarkRunner(args.target, args.corpus, args.runs, args.backend,
                             args.binary, args.record_machine)

    if args.bench == 'viewer':
        print(f"Running viewer bench for {runner.target}...")
        results = runner.run_viewer_bench(args.corpus, args.tiles, args.rows)
        results['timestamp'] = time.time()
        results['hostname'] = platform.node()
        output_json = json.dumps(results, indent=2)
        if args.output:
            with open(args.output, 'w') as f:
                f.write(output_json)
            print(f"Results written to {args.output}")
        else:
            print(output_json)
        sys.exit(0)

    print(f"Running benchmarks for {runner.target}...")
    results = runner.run_benchmarks(args.corpus, args.runs)

    results['timestamp'] = time.time()
    results['hostname'] = platform.node()

    output_json = json.dumps(results, indent=2)

    if args.output:
        with open(args.output, 'w') as f:
            f.write(output_json)
        print(f"Results written to {args.output}")
    else:
        print(output_json)


if __name__ == '__main__':
    main()
