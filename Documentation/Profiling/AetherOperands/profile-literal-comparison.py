"""Capture inline scalars versus constant-table scalars from the same simulation frame."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import time


def normalize_program(record):
    operations = []
    for opcode, target, offset, metadata in record['ops']:
        count, mask = metadata & 0xffff, metadata >> 16
        arguments = []
        for argument in range(count):
            reference = record['operands'][offset + argument]
            if argument < 16 and mask & (1 << argument):
                arguments.append([0, [reference]])
            else:
                source, index = reference >> 29, reference & 0x1fffffff
                if source in (0, 6, 7):
                    width = {0: 1, 6: 3, 7: 4}[source]
                    words = record['constants'][index:index + width]
                    if len(words) != width:
                        raise ValueError('Constant reference exceeds its table')
                    arguments.append([source, words])
                else:
                    arguments.append([source, index])
        operations.append([opcode, target, arguments])
    return {'name': record['name'], 'ops': operations, 'emitters': record.get('emitters', [])}


def stop(process):
    if process is not None and process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


def capture(name, runtime, reference):
    prefix = Path('/tmp') / f'aether-{name}'
    trace = Path(str(prefix) + '.trace')
    if trace.exists():
        raise FileExistsError(trace)
    if subprocess.run(['pgrep', '-x', 'Dissolve'], capture_output=True).returncode != 1:
        raise RuntimeError('Stop any existing Dissolve execution before profiling.')
    gate = Path(str(prefix) + '-gate')
    ready, release = Path(str(gate) + '.ready'), Path(str(gate) + '.go')
    manifest = Path(str(prefix) + '-programs.jsonl')
    for path in [ready, release, manifest]:
        path.unlink(missing_ok=True)
    environment = dict(os.environ)
    environment['ELIXIR_LITERAL_PROFILE_GATE'] = str(gate)
    environment['ELIXIR_LITERAL_PROFILE_MANIFEST'] = str(manifest)
    app = recorder = None
    with Path(str(prefix) + '-app.log').open('w') as app_log, Path(str(prefix) + '-capture.log').open('w') as record_log:
        try:
            app = subprocess.Popen([str(runtime / 'Dissolve')], cwd=runtime, env=environment,
                                   stdout=app_log, stderr=subprocess.STDOUT)
            print(f'{name}: warming PID {app.pid} to simulation frame 2400', flush=True)
            deadline = time.monotonic() + 120
            while not ready.exists() or ready.read_text() != '2400':
                if app.poll() is not None:
                    raise RuntimeError(f'{name}: app exited during warmup ({app.returncode})')
                if time.monotonic() > deadline:
                    raise TimeoutError(f'{name}: simulation gate was not reached')
                time.sleep(0.2)
            if ready.read_text() != '2400':
                raise ValueError('Unexpected gate frame')
            records = [json.loads(line) for line in manifest.read_text().splitlines()]
            if len(records) != 2:
                raise ValueError(f'Expected two compiled effects, found {len(records)}')
            inline_count = sum(bin(op[3] >> 16).count('1') for record in records for op in record['ops'])
            if ('-inline-' in name and inline_count == 0) or ('-table-' in name and inline_count != 0):
                raise ValueError(f'{name}: wrong scalar encoding loaded')
            normalized = [normalize_program(record) for record in records]
            if reference is not None and normalized != reference:
                raise ValueError(f'{name}: compiled effect semantics differ from the control')
            reference = normalized
            print(f'{name}: payloads validated; {inline_count} inline scalar operands; attaching Instruments', flush=True)
            recorder = subprocess.Popen([
                'xcrun', 'xctrace', 'record', '--template', 'Metal System Trace',
                '--instrument', 'Time Profiler', '--attach', str(app.pid),
                '--time-limit', '20s', '--output', str(trace),
            ], stdout=record_log, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 40
            while 'Ctrl-C to stop the recording' not in Path(str(prefix) + '-capture.log').read_text():
                if recorder.poll() is not None:
                    raise RuntimeError(f'{name}: recorder exited before release ({recorder.returncode})')
                if time.monotonic() > deadline:
                    raise TimeoutError(f'{name}: recorder readiness was not reported')
                time.sleep(0.05)
            release.write_text('go')
            print(f'{name}: recording 20s from simulation frame 2400', flush=True)
            if recorder.wait(timeout=100) != 0:
                raise RuntimeError(f'{name}: capture failed')
            if app.poll() is not None:
                raise RuntimeError(f'{name}: app exited during capture')
            print(f'{name}: capture complete', flush=True)
        finally:
            stop(recorder)
            stop(app)
    return reference


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtimes', type=Path, required=True)
    parser.add_argument('--captures', nargs='+', help='Select unfinished captures when resuming.')
    args = parser.parse_args()
    reference = None
    previous = Path('/tmp/aether-literal-inline-1-programs.jsonl')
    if args.captures and previous.exists():
        reference = [normalize_program(json.loads(line)) for line in previous.read_text().splitlines()]
    for variant, number in [('inline', 1), ('table', 1), ('table', 2), ('inline', 2), ('inline', 3), ('table', 3)]:
        name = f'literal-{variant}-{number}'
        if args.captures is None or name in args.captures:
            reference = capture(name, args.runtimes / variant / 'Dissolve', reference)
