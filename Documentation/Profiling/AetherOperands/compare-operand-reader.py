"""Compare complete six-stage GPU frames exported by aggregate-aether-profile.py."""

import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import statistics
import struct
import subprocess


def stats(values):
    return {
        'n': len(values),
        'mean': statistics.mean(values),
        'median': statistics.median(values),
        'p95': sorted(values)[int(0.95 * (len(values) - 1))],
    }


def summarize(directory, captures):
    frames = []
    for capture in captures:
        with (directory / f'{capture}-compute.csv').open() as source:
            frames.extend(csv.DictReader(source))
    result = {'captures': captures}
    result['total'] = stats([float(row['compute_ms']) for row in frames])
    result['scheduling'] = stats([
        sum(float(row[f'encoder_{i}_ms']) for i in range(4)) for row in frames
    ])
    result['spawn'] = stats([float(row['encoder_4_ms']) for row in frames])
    result['update'] = stats([float(row['encoder_5_ms']) for row in frames])
    return result


def shader_stats(path):
    data = path.read_bytes()
    words = struct.unpack(f'<{len(data) // 4}I', data)
    counts = {}
    position = 5
    while position < len(words):
        length, opcode = words[position] >> 16, words[position] & 0xffff
        if length == 0 or position + length > len(words):
            raise ValueError(f'Invalid SPIR-V instruction at word {position}: {path}')
        counts[opcode] = counts.get(opcode, 0) + 1
        position += length
    return {
        'bytes': len(data),
        'instructions': sum(counts.values()),
        'OpLoad': counts.get(61, 0),
        'OpBranchConditional': counts.get(250, 0),
        'sha256': hashlib.sha256(data).hexdigest(),
    }


def normalized_shader(path):
    """Ignore ID labels and global declaration order while retaining named identities."""
    lines = subprocess.check_output(['spirv-dis', '--raw-id', str(path)], text=True).splitlines()
    start = next(i for i, line in enumerate(lines) if ' = OpFunction ' in line)
    globals_ = {}
    locals_ = {}
    names = {}
    for line in lines[:start]:
        definition = re.match(r'\s*(%\d+) = (.*)', line)
        if definition:
            globals_[definition[1]] = definition[2]
        name = re.match(r'\s*OpName (%\d+) (.*)', line)
        if name:
            names[name[1]] = name[2]
    for line in lines[start:]:
        definition = re.match(r'\s*(%\d+) = (.*)', line)
        if definition:
            locals_[definition[1]] = f'local{len(locals_)}'
    signatures = {}

    def resolve(identifier):
        if identifier in locals_:
            return locals_[identifier]
        if identifier not in signatures:
            definition = re.sub(r'%\d+', lambda match: resolve(match[0]), globals_[identifier])
            signatures[identifier] = hashlib.sha256(
                (definition + '|' + names.get(identifier, '')).encode()
            ).hexdigest()
        return signatures[identifier]

    def normalize(line):
        return re.sub(r'%\d+', lambda match: resolve(match[0]), line.strip())

    declarations = sorted(
        normalize(line) for line in lines[:start]
        if line.strip() and not line.strip().startswith(';')
    )
    body = [normalize(line) for line in lines[start:]]
    return declarations, body


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, default=Path(__file__).parent)
    parser.add_argument('--control-shaders', type=Path, required=True)
    parser.add_argument('--reader-shaders', type=Path, required=True)
    args = parser.parse_args()
    result = {
        'control': summarize(args.data, ['operand-control-1', 'operand-control-2']),
        'reader': summarize(args.data, ['operand-reader-1', 'operand-reader-2']),
    }
    result['mean_change_percent'] = {
        stage: (result['reader'][stage]['mean'] / result['control'][stage]['mean'] - 1) * 100
        for stage in ['scheduling', 'spawn', 'update', 'total']
    }
    historical = json.loads((args.data / 'small-array-comparison.json').read_text())
    result['historical_small_array'] = historical['small_array']
    result['mean_change_vs_historical_percent'] = {
        stage: (result['reader'][stage]['mean'] / historical['small_array'][stage]['mean'] - 1) * 100
        for stage in ['scheduling', 'spawn', 'update', 'total']
    }
    result['static_spirv'] = {
        variant: {
            stage: shader_stats(directory / f'Particles{stage}.cs.spirv')
            for stage in ['Spawn', 'Update']
        }
        for variant, directory in [('control', args.control_shaders), ('reader', args.reader_shaders)]
    }
    result['normalized_spirv'] = {}
    for stage in ['Spawn', 'Update']:
        name = f'Particles{stage}.cs.spirv'
        control = normalized_shader(args.control_shaders / name)
        reader = normalized_shader(args.reader_shaders / name)
        result['normalized_spirv'][stage] = {
            'global_declarations_match': control[0] == reader[0],
            'function_instructions_match': control[1] == reader[1],
        }
    output = args.data / 'operand-reader-comparison.json'
    output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
