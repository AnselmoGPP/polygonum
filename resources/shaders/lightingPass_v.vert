#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(vertex)

#include "..\..\..\resources\shaders\vertexTools.vert"

// Uniform
//layout(set = 0, binding = 0) uniform ubobject {
//	vec4 test;
//} ubo;

// Input
layout (location = 0) in vec3 inPos;				// NDC position. Since it's in NDCs, no MVP transformation is required-
layout (location = 1) in vec2 inUVs;

// Output
layout(location = 0) out vec2 outUVs;				// UVs

// Functions
void main()
{
	//gl_Position.x = gl_Position.x * ubo.aspRatio.x;
	gl_Position = vec4(inPos, 1.0f);
    outUVs = inUVs;
}