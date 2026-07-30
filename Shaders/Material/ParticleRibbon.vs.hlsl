// The material fragment stage needs MaterialIndex. This macro extends the
// shared Ribbon push-constant ABI, so both generated stages declare 76 bytes.
#define MATERIAL_RIBBON 1

#include "../Aether/Ribbon.vs.hlsl"