#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(vertex)

#include "..\..\extern\polygonum\resources\shaders\vertexTools.vert"

layout(set = 0, binding = 0) uniform globalUbo {
    mat4 view;
    mat4 proj;
    vec4 camPos_t;
} gUbo;

layout(set = 0, binding = 1) uniform ubObj {
    mat4 model;					// mat4
    mat4 normalMatrix;			// mat3
} ubo[1];						// [i]: array of UBOs (one per instance)

layout(location = 0) in vec3 inPos;			// Each location has 16 bytes
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTan;

layout(location = 0) out vec3 outPos;		// Vertex position.
layout(location = 1) out vec2 outUV;
layout(location = 2) out vec3 outNormal;	// Ground normal
layout(location = 3) out TB outTB;			// Tangents & Bitangents

int i = gl_InstanceIndex;

void main()
{
	vec3 worldPos = (ubo[i].model * vec4(inPos, 1.0)).xyz;
	vec4 clipPos = gUbo.proj * gUbo.view * vec4(worldPos, 1.0);
	vec2 uv = inUV;
	vec3 normal = mat3(ubo[i].normalMatrix) * inNormal;
	TB tb = getTB(inNormal, inTan);

	gl_Position = clipPos;
	outPos = worldPos;
	outUV = uv;
	outNormal = normal;
	outTB = tb;
}

/*
	(location = X): Indices for the inputs that we can use later to reference them. Note that some types, like dvec3 64 bit vectors, use multiple slots:
		layout(location = 0) in dvec3 inPosition;
		layout(location = 2) in vec3 inColor;
*/