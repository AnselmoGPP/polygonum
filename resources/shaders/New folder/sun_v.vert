#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(vertex)

//#include "..\..\extern\polygonum\resources\shaders\vertexTools.vert"

layout(set = 0, binding = 0) uniform globalUbo {
    mat4 view;
    mat4 proj;
    vec4 camPos_t;
} gUbo;

layout(set = 0, binding = 1) uniform ubobject {
    mat4 model;					// mat4
    mat4 normalMatrix;			// mat3
} ubo;

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUVs;

layout(location = 0) out vec2 outUVs;

void main()
{
    gl_Position = gUbo.proj * gUbo.view * ubo.model * vec4(inPos, 1.0);
	//gl_Position  = (gUbo.proj * gUbo.view * ubo.model * vec4(inPos, 1.0)).xyww;	// this ensures depth == 1
    outUVs = inUVs;
}
