#ifndef AETHER_OPERAND_HLSL
#define AETHER_OPERAND_HLSL

static const uint OperandSource_Constant           = 0;
static const uint OperandSource_Parameter          = 1;
static const uint OperandSource_DynamicInput       = 2;
static const uint OperandSource_ParticleAttribute  = 3;
static const uint OperandSource_ImmediateUInt      = 4;
static const uint OperandSource_Curve              = 5;
static const uint OperandSource_ConstantVector3    = 6;
static const uint OperandSource_ConstantVector4    = 7;

/** @brief Reserves three tag bits and 29 payload bits in each operand reference. */
static const uint OperandIndexMask = 0x1fffffffu;

#endif