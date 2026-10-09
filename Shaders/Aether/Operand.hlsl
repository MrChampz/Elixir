#ifndef AETHER_OPERAND_HLSL
#define AETHER_OPERAND_HLSL

static const uint OperandSource_Constant            = 0u;
static const uint OperandSource_Parameter           = 1u;
static const uint OperandSource_DynamicInput        = 2u;
static const uint OperandSource_ParticleAttribute   = 3u;
static const uint OperandSource_Immediate           = 4u;

static const uint OperandFormat_Scalar              = 0u;
static const uint OperandFormat_Vector2             = 1u;
static const uint OperandFormat_Vector3             = 2u;
static const uint OperandFormat_Vector4             = 3u;
static const uint OperandFormat_UInt                = 4u;
static const uint OperandFormat_Curve               = 5u;

/** @brief Masks the 26-bit word offset or immediate payload. */
static const uint OperandIndexMask = 0x03ffffffu;

static const uint OperandSourceShift = 29u;
static const uint OperandFormatShift = 26u;
static const uint OperandFormatMask = 0x7u;

uint GetOperandSource(uint reference)
{
    return reference >> OperandSourceShift;
}

uint GetOperandFormat(uint reference)
{
    return (reference >> OperandFormatShift) & OperandFormatMask;
}

#endif
