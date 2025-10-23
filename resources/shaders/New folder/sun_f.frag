#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(fragment)

#include "..\..\extern\polygonum\resources\shaders\fragTools.vert"

// layout(early_fragment_tests) in;

layout(set = 0, binding = 2) uniform globalUbo {
    vec4 camPos_t;
    Light light[5];
} gUbo;

//layout(set = 0, binding = 3) uniform ubobject		// https://www.reddit.com/r/vulkan/comments/7te7ac/question_uniforms_in_glsl_under_vulkan_semantics/
//{
//	vec4 test;
//} ubo;

layout(set = 0, binding  = 3) uniform sampler2D texSampler;

layout(location = 0) in vec2 inUVs;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor = texture(texSampler, inUVs);
	gl_FragDepth = 0.9999999;
}
