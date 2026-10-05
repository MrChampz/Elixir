"""Capture the preserved array control and OperandReader with the same workload."""

import argparse
from pathlib import Path
import subprocess


def capture(variant, runtime, warmup):
    prefix = Path('/tmp') / f'aether-{variant}'
    trace = prefix.with_suffix('.trace')
    if trace.exists():
        raise FileExistsError(trace)

    existing = subprocess.run(['pgrep', '-x', 'Dissolve'], capture_output=True)
    if existing.returncode != 1:
        raise RuntimeError('Stop any existing Dissolve execution before profiling.')

    with Path(f'{prefix}-app.log').open('w') as app_log:
        app = subprocess.Popen([str(runtime / 'Dissolve')], cwd=runtime,
                               stdout=app_log, stderr=subprocess.STDOUT)
        try:
            print(f'{variant}: warming PID {app.pid} for {warmup}s', flush=True)
            try:
                app.wait(timeout=warmup)
            except subprocess.TimeoutExpired:
                pass
            else:
                raise RuntimeError(f'{variant}: app exited during warmup ({app.returncode})')
            print(f'{variant}: recording 15s', flush=True)
            with Path(f'{prefix}-capture.log').open('w') as capture_log:
                subprocess.run([
                    'xcrun', 'xctrace', 'record', '--template', 'Metal System Trace',
                    '--instrument', 'Time Profiler', '--attach', str(app.pid),
                    '--time-limit', '15s', '--output', str(trace),
                ], stdout=capture_log, stderr=subprocess.STDOUT, check=True)
            print(f'{variant}: capture complete', flush=True)
        finally:
            if app.poll() is None:
                app.terminate()
                try:
                    app.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    app.kill()
                    app.wait()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', type=Path, required=True)
    parser.add_argument('--reader', type=Path, required=True)
    parser.add_argument('--warmup', type=int, default=45)
    parser.add_argument('--captures', nargs='+', choices=[
        'operand-control-1', 'operand-reader-1', 'operand-reader-2', 'operand-control-2',
    ], help='Select unfinished captures when resuming an interrupted comparison.')
    args = parser.parse_args()
    # ABBA order balances changes in system conditions across both variants.
    for variant, runtime in [
        ('operand-control-1', args.control),
        ('operand-reader-1', args.reader),
        ('operand-reader-2', args.reader),
        ('operand-control-2', args.control),
    ]:
        if args.captures is None or variant in args.captures:
            capture(variant, runtime, args.warmup)
