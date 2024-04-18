#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(vertex)

#include "..\..\..\resources\shaders\vertexTools.vert"

//layout(set = 0, binding = 0) uniform globalUbo {
//    mat4 view;
//    mat4 proj;
//    vec4 camPos_t;
//} gUbo;
//
//layout(set = 0, binding = 1) uniform ubobject {
//    mat4 model;					// mat4
//    mat4 normalMatrix;			// mat3
//	vec4 sideDepthsDiff;
//} ubo;

layout(location = 0) in vec3 inPos;				// Each location has 16 bytes
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 outUV;

void main()
{
	gl_Position = vec4(inPos, 1.f);
	outUV = inUV;
}