from pathlib import Path
import re, shutil
root=Path('/Users/felpsmac/projects/Elixir')
binpath=Path('/tmp/elixir-pr39-release/Build/Release/macOS/arm64/Dissolve')
control=Path('/tmp/aether-cache-control')
control.mkdir(exist_ok=True)
for name in ['Dissolve','Assets']:
    p=control/name
    if not p.exists(): p.symlink_to(binpath/name)
shutil.copytree(binpath/'Shaders',control/'Shaders',dirs_exist_ok=True)
source=Path('/tmp/aether-cache-shader-source')
source.mkdir(exist_ok=True)
for file,context in [('ParticlesSpawn.cs.hlsl','randomInput'),('ParticlesUpdate.cs.hlsl','life')]:
    s=(root/'Shaders/Aether'/file).read_text()
    start=s.index('    for (uint i = 0u; i < opCount; ++i)')
    head=s[:start]
    body=s[start:]
    body=re.sub(r'ReadOperand\(\s*op,\s*(\w+),\s*instance,\s*attributes,\s*'+context+r',\s*particleSeed\s*\)',lambda m:'ReadCachedOperand(op, '+m[1]+', instance, attributes, '+context+', particleSeed, argument0, argument1)',body)
    needle='        uint target = op.Target;'
    body=body.replace(needle,needle+f'\n\n        float4 argument0 = ReadOperand(op, 0u, instance, attributes, {context}, particleSeed);\n        float4 argument1 = 0.0;\n        if ((op.OperandMetadata & 0xffffu) > 1u)\n            argument1 = ReadOperand(op, 1u, instance, attributes, {context}, particleSeed);',1)
    s=head+body
    s=s.replace('../../Elixir/Source/Engine/Aether/Core/ParticleDispatch.def',str(root/'Elixir/Source/Engine/Aether/Core/ParticleDispatch.def'))
    (source/file).write_text(s)
print('Prepared cache-only control with current executable and identical shader logic.')
