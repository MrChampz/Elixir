"""Build isolated inline-scalar and constant-table runtimes from an existing Release build."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import shutil
import subprocess


def replace_once(source, old, new):
    if source.count(old) != 1:
        raise ValueError(f'Expected one occurrence of {old[:60]!r}')
    return source.replace(old, new, 1)


def prepare(root, build, output):
    output.mkdir(exist_ok=False)
    source_root = root / 'Elixir/Source'
    runtime = build / 'Build/Release/macOS/arm64/Dissolve'
    cache = (build / 'CMakeCache.txt').read_text()
    ninja = re.search(r'CMAKE_MAKE_PROGRAM:[^=]+=(.+)', cache)[1]
    dxc = re.search(r'DXC:[^=]+=(.+)', cache)[1]
    original = output / 'original'
    original.mkdir()
    shutil.copytree(root / 'Shaders', original / 'Shaders')
    source_hashes = {}
    for relative in ['Engine/Aether/Modules/Module.cpp', 'Engine/Aether/Simulation/Simulator.cpp', 'Engine/Aether/System.cpp']:
        path = source_root / relative
        source_hashes[relative] = hashlib.sha256(path.read_bytes()).hexdigest()
        destination = original / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)

    def commands(target):
        return subprocess.check_output([ninja, '-C', str(build), '-t', 'commands', target], text=True).splitlines()

    def compile_source(relative, source, destination):
        target = f'CMakeFiles/Elixir.dir/Elixir/Source/{relative}.o'
        args = shlex.split(commands(target)[-1])
        args[args.index('-c') + 1] = str(source)
        args[args.index('-o') + 1] = str(destination)
        args[args.index('-MF') + 1] = str(destination) + '.d'
        args[args.index('-MT') + 1] = str(destination)
        args += ['-I', str((source_root / relative).parent)]
        subprocess.run(args, cwd=build, check=True)

    common = output / 'common'
    common.mkdir()
    simulator = (source_root / 'Engine/Aether/Simulation/Simulator.cpp').read_text()
    simulator = replace_once(simulator, '#include "Simulator.h"', '#include "Simulator.h"\n#include <chrono>\n#include <cstdlib>\n#include <thread>')
    simulator = replace_once(simulator, '''        m_LastDeltaTimeSeconds = timestep.GetSeconds();
        m_ElapsedTimeSeconds += timestep.GetSeconds();''', '''        // Profiling only: each capture resumes from the same simulation frame.
        static uint64_t profilingFrame = 0;
        ++profilingFrame;
        if (profilingFrame == 2400)
        {
            if (const auto gate = std::getenv("ELIXIR_LITERAL_PROFILE_GATE"))
            {
                std::ofstream(std::string(gate) + ".ready") << profilingFrame;
                while (!std::filesystem::exists(std::string(gate) + ".go"))
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        m_LastDeltaTimeSeconds = 1.0f / 60.0f;
        m_ElapsedTimeSeconds += m_LastDeltaTimeSeconds;''')
    (common / 'Simulator.cpp').write_text(simulator)
    compile_source('Engine/Aether/Simulation/Simulator.cpp', common / 'Simulator.cpp', common / 'Simulator.cpp.o')

    system = (source_root / 'Engine/Aether/System.cpp').read_text()
    system = replace_once(system, '#include "System.h"', '#include "System.h"\n#include <cstdlib>')
    manifest = '''        system.Program.Finalize();

        // Profiling only: save exact instruction payloads for a differential check.
        if (const auto path = std::getenv("ELIXIR_LITERAL_PROFILE_MANIFEST"))
        {
            std::ofstream output(path, std::ios::app);
            output << "{\\"name\\":\\"" << m_Name << "\\",\\"ops\\":[";
            bool first = true;
            for (const auto& op : system.Program.Ops)
            {
                if (!first) output << ',';
                first = false;
                output << '[' << uint32_t(op.Type) << ',' << uint32_t(op.Target) << ','
                       << op.OperandOffset << ',' << op.OperandMetadata << ']';
            }
            output << "],\\"operands\\":[";
            first = true;
            for (const auto value : system.Program.Operands)
            {
                if (!first) output << ',';
                first = false;
                output << value;
            }
            output << "],\\"constants\\":[";
            first = true;
            for (const auto value : system.Program.Constants)
            {
                if (!first) output << ',';
                first = false;
                output << value;
            }
            output << "],\\"emitters\\":[";
            first = true;
            for (const auto& emitter : system.Emitters)
            {
                if (!first) output << ',';
                first = false;
                output << '[' << emitter.SpawnOpOffset << ',' << emitter.SpawnOpCount << ','
                       << emitter.UpdateOpOffset << ',' << emitter.UpdateOpCount << ','
                       << emitter.MaxParticles << ']';
            }
            output << "]}\\n";
        }'''
    system = replace_once(system, '        system.Program.Finalize();', manifest)
    (common / 'System.cpp').write_text(system)
    compile_source('Engine/Aether/System.cpp', common / 'System.cpp', common / 'System.cpp.o')

    link = commands('Elixir')[-1].removeprefix(': && ').removesuffix(' && :')
    link = shlex.split(link)
    for relative in ['Engine/Aether/Simulation/Simulator.cpp', 'Engine/Aether/System.cpp']:
        old = f'CMakeFiles/Elixir.dir/Elixir/Source/{relative}.o'
        link[link.index(old)] = str(common / (Path(relative).name + '.o'))

    for variant in ['inline', 'table']:
        variant_root = output / variant
        app = variant_root / 'Dissolve'
        library = variant_root / 'Elixir/libElixir.dylib'
        app.mkdir(parents=True)
        library.parent.mkdir()
        shutil.copy2(runtime / 'Dissolve', app / 'Dissolve')
        (app / 'Assets').symlink_to(runtime / 'Assets')
        shutil.copytree(runtime / 'Shaders', app / 'Shaders', ignore=shutil.ignore_patterns('Generated'))
        # Freeze current source templates and includes along with compiled shaders.
        shutil.copytree(original / 'Shaders', app / 'Shaders', dirs_exist_ok=True)
        load_commands = subprocess.check_output(['otool', '-l', str(app / 'Dissolve')], text=True)
        rpaths = re.findall(r'path (.+) \(offset \d+\)', load_commands)
        engine_directory = build / 'Build/Release/macOS/arm64/Elixir'
        engine_rpath = next(path for path in rpaths if Path(path).resolve() == engine_directory.resolve())
        subprocess.run(['install_name_tool', '-rpath', engine_rpath,
                        '@executable_path/../Elixir', str(app / 'Dissolve')], check=True)
        subprocess.run(['codesign', '--force', '--sign', '-', str(app / 'Dissolve')], check=True)
        variant_link = list(link)
        variant_link[variant_link.index('-o') + 1] = str(library)
        if variant == 'table':
            module = (source_root / 'Engine/Aether/Modules/Module.cpp').read_text()
            start = module.index('        uint32_t inlineScalarMask = 0;')
            end = module.index('\n    }', start)
            module = module[:start] + '''        for (const auto& operand : operands)
            m_Program.Operands.push_back(operand.Reference);
        m_Program.Ops.push_back({ type, target, offset, uint32_t(operands.size()) });''' + module[end:]
            (variant_root / 'Module.cpp').write_text(module)
            compile_source('Engine/Aether/Modules/Module.cpp', variant_root / 'Module.cpp', variant_root / 'Module.cpp.o')
            old = 'CMakeFiles/Elixir.dir/Elixir/Source/Engine/Aether/Modules/Module.cpp.o'
            variant_link[variant_link.index(old)] = str(variant_root / 'Module.cpp.o')
            operand_path = app / 'Shaders/Aether/ParticleOperands.hlsl'
            operands = operand_path.read_text()
            pattern = r'    if \(argument < 16u && \(\(op\.OperandMetadata >> 16\) & \(1u << argument\)\) != 0u\)\n        return (?:float4\(asfloat\(reference\), 0\.0, 0\.0, 0\.0\)|asfloat\(reference\));\n\n'
            operands, count = re.subn(pattern, '', operands)
            if count != 2:
                raise ValueError('Expected two inline-scalar reader paths')
            operand_path.write_text(operands)
            for name in ['ParticlesSpawn.cs.hlsl', 'ParticlesUpdate.cs.hlsl']:
                source = app / 'Shaders/Aether' / name
                destination = source.with_suffix('.spirv')
                subprocess.run([dxc, '-spirv', '-T', 'cs_6_0', '-E', 'main', '-I', str(source.parent),
                                str(source), '-Fo', str(destination)], check=True)
        else:
            shutil.copy2(source_root / 'Engine/Aether/Modules/Module.cpp', variant_root / 'Module.cpp')
        subprocess.run(variant_link, cwd=build, check=True)
        for name in ['ParticlesSpawn.cs.spirv', 'ParticlesUpdate.cs.spirv']:
            subprocess.run(['spirv-val', '--target-env', 'vulkan1.3', str(app / 'Shaders/Aether' / name)], check=True)
        print(f'Prepared {variant}: {app}', flush=True)

    metadata = {'source_root': str(root), 'build': str(build), 'source_hashes': source_hashes,
                'fixed_delta_seconds': 1 / 60, 'capture_start_simulation_frame': 2400, 'runtime_hashes': {}}
    for variant in ['inline', 'table']:
        directory = output / variant
        files = [directory / 'Elixir/libElixir.dylib', directory / 'Dissolve/Dissolve',
                 directory / 'Dissolve/Shaders/Aether/ParticlesSpawn.cs.spirv',
                 directory / 'Dissolve/Shaders/Aether/ParticlesUpdate.cs.spirv']
        metadata['runtime_hashes'][variant] = {
            str(path.relative_to(directory)): hashlib.sha256(path.read_bytes()).hexdigest() for path in files
        }
    (output / 'build-manifest.json').write_text(json.dumps(metadata, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    prepare(args.root.resolve(), args.build.resolve(), args.output.resolve())
