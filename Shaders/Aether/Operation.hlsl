#ifndef AETHER_OPERATION_HLSL
#define AETHER_OPERATION_HLSL

struct SGPUOperation
{
    uint Type;
    uint Target;
    uint OperandOffset;
    uint OperandCount;
};

#endif
