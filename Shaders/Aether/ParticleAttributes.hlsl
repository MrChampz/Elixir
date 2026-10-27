#ifndef AETHER_PARTICLE_ATTRIBUTES_HLSL
#define AETHER_PARTICLE_ATTRIBUTES_HLSL

static const uint ParticleAttribute_None       = 0;
static const uint ParticleAttribute_Position   = 1;
static const uint ParticleAttribute_Rotation   = 2;
static const uint ParticleAttribute_Scale      = 3;
static const uint ParticleAttribute_Velocity   = 4;
static const uint ParticleAttribute_Color      = 5;
static const uint ParticleAttribute_Size       = 6;
static const uint ParticleAttribute_Lifetime   = 7;
static const uint ParticleAttribute_Tangent    = 8;
static const uint ParticleAttribute_RibbonId   = 9;
static const uint ParticleAttribute_Temp0      = 10;
static const uint ParticleAttribute_Temp1      = 11;
static const uint ParticleAttribute_Temp2      = 12;
static const uint ParticleAttribute_Temp3      = 13;

struct AttributeTable
{
    float4 Position;
    float4 Rotation;
    float4 Scale;
    float4 Velocity;
    float4 Color;
    float4 Size;
    float4 Lifetime;
    float4 Tangent;
    uint   RibbonId;
    float4 Temp0;
    float4 Temp1;
    float4 Temp2;
    float4 Temp3;
};

AttributeTable EmptyAttributeTable()
{
    AttributeTable attributes;
    attributes.Position = 0.0;
    attributes.Rotation = 0.0;
    attributes.Scale = float4(1.0, 0.0, 0.0, 0.0);
    attributes.Velocity = 0.0;
    attributes.Color = 1.0;
    attributes.Size = float4(6.0, 0.0, 0.0, 0.0);
    attributes.Lifetime = float4(1.0, 0.0, 0.0, 0.0);
    attributes.Tangent = float4(1.0, 0.0, 0.0, 0.0);
    attributes.RibbonId = 0u;
    attributes.Temp0 = 0.0;
    attributes.Temp1 = 0.0;
    attributes.Temp2 = 0.0;
    attributes.Temp3 = 0.0;

    return attributes;
}

AttributeTable LoadAttributes(SGPUParticle state)
{
    AttributeTable table;
    table.Position = float4(state.Position, state.Alive);
    table.Rotation = float4(state.Transform.x, 0.0, 0.0, 0.0);
    table.Scale = float4(state.Transform.y, 0.0, 0.0, 0.0);
    table.Velocity = float4(state.Velocity, 0.0);
    table.Tangent = float4(state.Tangent, 0.0);
    table.Color = state.Color;
    table.Size = float4(state.Size, 0.0, 0.0, 0.0);
    table.Lifetime = float4(state.Lifetime, state.Age, 0.0, 0.0);
    table.RibbonId = state.RibbonId;
    table.Temp0 = 0.0;
    table.Temp1 = 0.0;
    table.Temp2 = 0.0;
    table.Temp3 = 0.0;

    return table;
}

SGPUParticle StoreAttributes(SGPUParticle state, AttributeTable table, float age)
{
    state.Position = table.Position.xyz;
    state.Size = table.Size.x;
    state.Velocity = table.Velocity.xyz;
    state.Age = age;
    state.Transform = float4(table.Rotation.x, table.Scale.x, 0.0, 0.0);
    state.Tangent = table.Tangent.xyz;
    state.RibbonId = table.RibbonId;
    state.Color = table.Color;
    state.Lifetime = table.Lifetime.x;
    state.Alive = table.Position.w;
    return state;
}

float4 GetAttribute(AttributeTable table, uint attrId)
{
    if (attrId == ParticleAttribute_Position)
        return table.Position;
    if (attrId == ParticleAttribute_Rotation)
        return table.Rotation;
    if (attrId == ParticleAttribute_Scale)
        return table.Scale;
    if (attrId == ParticleAttribute_Velocity)
        return table.Velocity;
    if (attrId == ParticleAttribute_Color)
        return table.Color;
    if (attrId == ParticleAttribute_Size)
        return table.Size;
    if (attrId == ParticleAttribute_Lifetime)
        return table.Lifetime;
    if (attrId == ParticleAttribute_Tangent)
        return table.Tangent;
    if (attrId == ParticleAttribute_RibbonId)
        return float4(float(table.RibbonId), 0.0, 0.0, 0.0);
    if (attrId == ParticleAttribute_Temp0)
        return table.Temp0;
    if (attrId == ParticleAttribute_Temp1)
        return table.Temp1;
    if (attrId == ParticleAttribute_Temp2)
        return table.Temp2;
    if (attrId == ParticleAttribute_Temp3)
        return table.Temp3;

    return float4(0.0, 0.0, 0.0, 0.0);
}

void SetAttribute(inout AttributeTable table, uint attrId, float4 value)
{
    if (attrId == ParticleAttribute_Position)
        table.Position = value;
    else if (attrId == ParticleAttribute_Rotation)
        table.Rotation = value;
    else if (attrId == ParticleAttribute_Scale)
        table.Scale = value;
    else if (attrId == ParticleAttribute_Velocity)
        table.Velocity = value;
    else if (attrId == ParticleAttribute_Color)
        table.Color = value;
    else if (attrId == ParticleAttribute_Size)
        table.Size = value;
    else if (attrId == ParticleAttribute_Lifetime)
        table.Lifetime = value;
    else if (attrId == ParticleAttribute_Tangent)
        table.Tangent = value;
    else if (attrId == ParticleAttribute_Temp0)
        table.Temp0 = value;
    else if (attrId == ParticleAttribute_Temp1)
        table.Temp1 = value;
    else if (attrId == ParticleAttribute_Temp2)
        table.Temp2 = value;
    else if (attrId == ParticleAttribute_Temp3)
        table.Temp3 = value;
}

void SetAttribute(inout AttributeTable table, uint attrId, uint value)
{
    if (attrId == ParticleAttribute_RibbonId)
        table.RibbonId = value;
}

#endif
