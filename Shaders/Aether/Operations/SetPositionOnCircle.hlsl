#ifndef AETHER_OPERATION_SETPOSITIONONCIRCLE_HLSL
#define AETHER_OPERATION_SETPOSITIONONCIRCLE_HLSL

static const uint Operation_SetPositionOnCircle = 10u;
static const uint SetPositionOnCircle_Center = 0u;
static const uint SetPositionOnCircle_Radius = 1u;
static const uint SetPositionOnCircle_AngularSpeed = 2u;
static const uint SetPositionOnCircle_StartAngle = 3u;

void SetPositionOnCircle(inout SOperationExecutor executor, inout SOperationExecutorContext context)
{
    float3 center = executor.Reader.ReadVector3(SetPositionOnCircle_Center);
    float radius = executor.Reader.ReadScalar(SetPositionOnCircle_Radius);
    float angularSpeed = executor.Reader.ReadScalar(SetPositionOnCircle_AngularSpeed);
    float startAngle = executor.Reader.ReadScalar(SetPositionOnCircle_StartAngle);
    float angle = context.Time * angularSpeed + startAngle;

    float3 position = center + float3(cos(angle), sin(angle), 0.0) * radius;
    float4 positionValue = float4(position, 1.0);

    SetAttribute(
        context.Attributes,
        executor.Reader.Operation.Target,
        positionValue
    );

    float orientation = angularSpeed < 0.0 ? -1.0 : 1.0;
    float3 tangent = float3(-sin(angle), cos(angle), 0.0) * orientation;
    float4 tangentValue = float4(tangent, 0.0);

    SetAttribute(
        context.Attributes,
        8u,
        tangentValue
    );

    float4 velocityValue = float4(0.0, 0.0, 0.0, 0.0);

    SetAttribute(
        context.Attributes,
        4u,
        velocityValue
    );

}

#endif
