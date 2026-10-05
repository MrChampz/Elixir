# Aether module operands

Modules compile instructions through `ModuleCompileContext`. A module does not need
its own GPU payload struct. The compiled program contains three flat tables:

- Instructions: 16 bytes each, with an opcode, destination attribute, operand offset,
  and argument count.
- Operands: four-byte references with a three-bit source tag and a 29-bit index.
- Constants: four-byte words containing the original float or integer bits. Scalar,
  vector3, and vector4 literals occupy 4, 12, and 16 bytes respectively.

Opcode values are explicit in `Core/Particle.h`; named argument positions are in
`Core/OperationInputs.h`. `Core/Operation.h` defines `SOperation`, and
`Core/Operand.h` defines `SOperand`, its types, source tags, and index mask.
`SParameter` is defined in `Core/ParameterStore.h`. `Core/CompiledOperations.h`
contains the compiled tables and constant compaction. `Module.cpp` validates each
operation's argument types.
`../Shaders/Aether/Operations.hlsl` declares the matching shader opcodes and
argument positions explicitly. Keep the C++ and HLSL declarations in sync when
adding or changing an operation.

## Adding a module

An authored module emits one or more existing operations using `Scalar`, `Vector`,
`Value`, `UInt`, `Dynamic`, `Attribute`, and `Curve`. For example:

```cpp
context.Emit(Core::EParticleOp::AddWithDelta, Core::EParticleAttribute::Rotation, {
    context.Scalar(m_RadiansPerSecond, m_ParamName),
    context.Dynamic(m_Input)
});
```

Adding an authored module does not require a new operation or payload struct. If a
new GPU operation is necessary, add its opcode, argument contract, and names to the C++ definitions, mirror its
opcode and argument names in `Operations.hlsl`, and implement its shader behavior. Instructions support a
variable number of arguments; the vortex ribbon path currently uses nine.

Scalar and vector parameters retain their declared types. A name identifies one
parameter; replacing its type replaces the previous default. An incompatible named
binding fails during compilation. A missing optional binding uses its literal
fallback. Missing curves retain the existing scalar-zero and color-white defaults.

## Packing and lifetime

Scalar and vector literals store their exact bits in the constant table. Every
operand uses the same source-reference encoding, regardless of its argument
position. Instructions do not need a mask to distinguish references from float
bits, and compacting constants only requires traversing the operand table.

Constants are interned per compiled system using their logical type and exact bits.
Identical typed literals share storage across modules and emitters. Signed zero,
integer values, and float values keep distinct identities. Small unsigned integers
fit directly in the reference; larger integers use one literal word. Ribbon identifiers
are converted to float only when written to the existing particle-state layout.

`System::Compile` calls `CompactConstants` to remove unused literals, relocate the remaining
references, and release the CPU lookup index used to intern literals. Runtime instances share the compiled CPU data. The resource pool also shares
GPU instructions, operand references, and constants for the same source UUID and
compilation revision. Parameters and particle state remain separate for each instance.

References stay relative to their source tables. Shaders apply the instance's operand,
constant, and parameter base offsets. Uploading a program requires no opcode-specific
rebasing. The first allocation uploads the program; subsequent instances reuse it.
A program reservation is released after its last instance retires through the existing
GPU fence path. Older and newer compilation revisions can coexist.

## Memory and execution costs

Program storage is `16 * instructionCount + 4 * operandCount + 4 * constantWordCount`.
Examples for fresh programs, excluding the instance record and mutable parameter table:

| Instruction | Current bytes | Previous bytes |
| --- | ---: | ---: |
| One scalar literal | 24 | 64 |
| Cone with literal direction, angle, and two distinct speeds | 56 | 64 |
| Cone with three parameter bindings | 44 | 64 |
| Vortex ribbon path with nine distinct literal arguments | 96 | 64 |

Packing reduces common instructions, but large contracts can exceed the previous
fixed size. Deduplication and sharing reduce the total across repeated instructions
and instances. The default capacities reserve 3 MiB for the three program buffers;
the previous instruction buffer alone reserved 4 MiB. The GPU instance record grows
from 52 to 60 bytes to hold the additional base offsets.

Evaluation adds an operand-reference load and source selection before reading a
value. Source tags are uniform for particles executing the same instruction. Scalars
load one word, vector3 values load three words, and vector4 values load four words;
there is no per-particle allocation or CPU callback. Bounds testing loads each bound
once. These properties do not establish a GPU speedup: representative GPU timing is
still needed to compare the extra indirection against lower memory traffic.

Each shader instruction creates an `SOperandReader` before selecting the operation.
The reader captures the instruction, instance, source attributes, dynamic-input
context, and particle seed, and predecodes the first two numeric positions into
a small internal cache. Handlers use `ReadValue`, `ReadScalar`, `ReadVector3`,
`ReadUInt`, and `ReadCurve` with the named argument index. Scalar and vector3
methods use cached components for indices zero and one and dedicated typed readers
for later indices. The cache size does not bound the operation's argument count.
UInt and curve methods always use their dedicated readers, preserving integer
payloads and the missing-curve sentinel. The reader snapshots source attributes
before destination writes. The None dynamic input is folded to the literal one
during compilation.

Operand decoding and reference lookup are methods of `SOperandReader` and use its
captured context directly. `DecodeValue`, `DecodeScalar`, and `DecodeVector3`
bypass the cache; the factory uses `DecodeValue` to populate it before cached
reads are allowed. Integer and curve reads decode directly without float caching.

Spawn and update use 256 threads per group, with matching C++ rounded dispatch
counts and HLSL `numthreads` declarations. The emitter scheduler retains its
separate group size. Keep the C++ and HLSL sizes in sync. Spawn threads enumerate emission order and map it to the
ring-buffer slot, keeping each new batch in consecutive lanes without changing
particle indices, emission order, or random seeds.

The header stores the argument count in a full 32-bit word. It contains no packed
mask or C++ bit fields, keeping the CPU/GPU representation explicit on macOS and
Windows. Each operation's contract determines its permitted argument count.

The scalar encoding comparison in
[`InlineScalarComparison.md`](Profiling/AetherOperands/InlineScalarComparison.md)
found no consistent gain from storing float bits directly in operands. Keeping
scalar literals in constants removes that special case without losing a measured
performance benefit in the tested scene.

The reference payload limits source indices to 29 bits. The maximum payload is
reserved as the missing-curve sentinel for curve references. Compilation rejects
unrepresentable references, invalid argument counts, and incompatible argument types.
Pool allocation remains atomic when any table lacks capacity.

## Validation

Unit tests cover module dispatch, scoped parameter lookup, literal fallbacks, curve
inputs, typed constant interning, unsigned integer preservation, nine-argument
instructions, malformed references, parameter type replacement, pool rollback,
shared program lifetime, and coexistence of compilation revisions. DXC compilation
and SPIR-V validation check the shader interface. The instruction and instance layouts
are also checked against their expected byte sizes. Public out-of-line compilation
and pool APIs remain in classes exported with `ELIXIR_API` for Windows DLL consumers.

The constants-only change was validated on 2026-10-04 with a macOS Release build
of Dissolve and Elixir.UnitTests, both spawn/update shaders passing SPIR-V validation,
and all 367 tests passing, including 31 GPU tests. One GPU test crashed during
worker logging at initialization and passed on an isolated retry. The default
Dissolve scene also ran for 25 seconds. Windows execution was not available;
the instruction header remains 16 bytes, the shader bindings are unchanged, and
the change adds no out-of-line API symbols.
