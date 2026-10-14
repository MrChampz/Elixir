// xyz is the fixed destination. w is zero when disabled, one for simulation space,
// and two for world space. Each emitter owns one target; only its main chains use it.
[[vk::binding(10, 0)]] StructuredBuffer<float4> strandEndAnchors;
