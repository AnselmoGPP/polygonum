#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(vertex)

//#include "..\..\..\resources\shaders\vertexTools.vert"

layout(set = 0, binding = 0) uniform globalUbo {
    mat4 view;
    mat4 proj;
    vec4 camPos_t;
} gUbo;

layout(set = 0, binding = 1) uniform ubObj {
    mat4 model;					// mat4
    mat4 normalMatrix;			// mat3
} ubo[1];						// [i]: array of descriptors (represents number of instances of an object)

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inUVs;

layout(location = 0) out vec3 outPos;			// world space vertex position
layout(location = 1) out vec2 outUVs;
layout(location = 2) out vec3 outTangent;
layout(location = 3) out vec3 outBitangent;
layout(location = 4) out vec3 outNormal;

//int i = gl_InstanceIndex;

void main()
{
	gl_Position = gUbo.proj * gUbo.view * ubo[0].model * vec4(inPos, 1.0);
	
	outPos       = (ubo[0].model * vec4(inPos, 1.0)).xyz;
	outUVs       = inUVs;
	outTangent   = mat3(ubo[0].normalMatrix) * inTangent;
	outBitangent = mat3(ubo[0].normalMatrix) * cross(inNormal, inTangent);
	outNormal    = mat3(ubo[0].normalMatrix) * inNormal;
}