# Aether module operands

Modules compile instructions through `ModuleCompileContext`. A module does not need
its own GPU payload struct. The compiled program contains three flat tables:

- Instructions: 16 bytes each, with an opcode, destination attribute, operand offset,
  and operand count.
- Operands: four-byte references with a three-bit source tag and a 29-bit index.
- Constants: four-byte words containing the original float or integer bits. Scalar,
  vector3, and vector4 literals occupy 4, 12, and 16 bytes respectively.

`ParticleOperations.def` defines opcode values, argument order, and logical types.
The C++ compiler validates each instruction against this contract. HLSL uses the same
file for opcode values and named argument positions. `ParticleOperandSources.def`
defines the source tags used by both languages. Changes to either file rebuild shaders.

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
new GPU operation is necessary, add its argument contract and names to the shared
`.def` file and its behavior to the appropriate shader. Instructions support a
variable number of arguments; the vortex ribbon path currently uses nine.

Scalar and vector parameters retain their declared types. A name identifies one
parameter; replacing its type replaces the previous default. An incompatible named
binding fails during compilation. A missing optional binding uses its literal
fallback. Missing curves retain the existing scalar-zero and color-white defaults.

## Packing and lifetime

Constants are interned per compiled system using their logical type and exact bits.
Identical typed literals share storage across modules and emitters. Signed zero,
integer values, and float values keep distinct identities. Small unsigned integers
fit directly in the reference; larger integers use one literal word. Ribbon identifiers
are converted to float only when written to the existing particle-state layout.

`System::Compile` calls `Finalize` to release the CPU lookup index used to intern
literals. Runtime instances share the compiled CPU data. The resource pool also shares
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
