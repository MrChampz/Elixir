"""Compare repeated GPU captures with inline and constant-table scalar operands."""

import argparse
import csv
import json
from pathlib import Path
import runpy
import statistics


def compare(data, runtimes):
    helpers = runpy.run_path(str(Path(__file__).with_name('compare-operand-reader.py')))
    normalize = runpy.run_path(str(Path(__file__).with_name('profile-literal-comparison.py')))['normalize_program']
    stats, shader_stats = helpers['stats'], helpers['shader_stats']
    captures = [f'literal-{variant}-{number}' for number in range(1, 4) for variant in ('inline', 'table')]
    frames, manifests = {}, {}
    reference = None
    for name in captures:
        with (data / f'{name}-compute.csv').open() as source:
            frames[name] = sorted(list(csv.DictReader(source)), key=lambda row: float(row['start_ms']))
        records = [json.loads(line) for line in (data / f'{name}-programs.jsonl').read_text().splitlines()]
        normalized = [normalize(record) for record in records]
        if reference is not None and reference != normalized:
            raise ValueError(f'Compiled payloads differ: {name}')
        reference = normalized
        manifests[name] = records
    common_count = min(len(rows) for rows in frames.values())
    if common_count < 100:
        raise ValueError('Insufficient complete GPU frames')

    def summarize(rows):
        return {
            'total': stats([float(row['compute_ms']) for row in rows]),
            'scheduling': stats([sum(float(row[f'encoder_{i}_ms']) for i in range(4)) for row in rows]),
            'spawn': stats([float(row['encoder_4_ms']) for row in rows]),
            'update': stats([float(row['encoder_5_ms']) for row in rows]),
        }

    stages = ['scheduling', 'spawn', 'update', 'total']
    result = {
        'common_complete_frames_per_capture': common_count,
        'all_decoded_payloads_match': True,
        'captures': {name: {'all_frames': summarize(rows), 'common_prefix': summarize(rows[:common_count])}
                     for name, rows in frames.items()},
    }
    for sample, limit in [('all_frames', None), ('common_prefix', common_count)]:
        pooled = {variant: summarize([row for number in range(1, 4)
                                     for row in frames[f'literal-{variant}-{number}'][:limit]])
                  for variant in ('inline', 'table')}
        pooled['inline_reduction_percent'] = {
            stage: (1 - pooled['inline'][stage]['mean'] / pooled['table'][stage]['mean']) * 100
            for stage in stages
        }
        result[sample] = pooled
    result['paired_inline_reduction_percent'] = [
        {stage: (1 - result['captures'][f'literal-inline-{number}']['common_prefix'][stage]['mean'] /
                 result['captures'][f'literal-table-{number}']['common_prefix'][stage]['mean']) * 100
         for stage in stages} for number in range(1, 4)
    ]
    result['pair_total_reduction_range_percent'] = [
        min(pair['total'] for pair in result['paired_inline_reduction_percent']),
        max(pair['total'] for pair in result['paired_inline_reduction_percent']),
    ]
    result['program_storage'] = {}
    for variant in ('inline', 'table'):
        result['program_storage'][variant] = [
            {'name': record['name'], 'operations': len(record['ops']),
             'operand_words': len(record['operands']), 'constant_words': len(record['constants']),
             'inline_scalars': sum(bin(op[3] >> 16).count('1') for op in record['ops']),
             'payload_bytes': 16 * len(record['ops']) + 4 * (len(record['operands']) + len(record['constants']))}
            for record in manifests[f'literal-{variant}-1']
        ]
    result['static_spirv'] = {
        variant: {stage: shader_stats(runtimes / variant / 'Dissolve/Shaders/Aether' / f'Particles{stage}.cs.spirv')
                  for stage in ('Spawn', 'Update')} for variant in ('inline', 'table')
    }
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, default=Path(__file__).parent)
    parser.add_argument('--runtimes', type=Path, required=True)
    args = parser.parse_args()
    result = compare(args.data, args.runtimes)
    (args.data / 'literal-comparison.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
