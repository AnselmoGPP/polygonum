#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(fragment)

#include "..\..\..\resources\shaders\fragTools.vert"

layout(early_fragment_tests) in;

//layout(set = 0, binding = 2) uniform globalUbo {
//    vec4 camPos_t;
//    Light light[NUMLIGHTS];
//} gUbo;
//
//layout(set = 0, binding = 3) uniform ubobject		// https://www.reddit.com/r/vulkan/comments/7te7ac/question_uniforms_in_glsl_under_vulkan_semantics/
//{
//	vec4 test;
//} ubo;

layout(set = 0, binding  = 0) uniform sampler2D texSampler[1];

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

void main()
{		
	outColor = getColor(texSampler[0], inUV, 1);
}