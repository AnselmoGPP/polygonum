#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(fragment)

#include "..\..\extern\polygonum\resources\shaders\fragTools.vert"

layout(early_fragment_tests) in;

layout(set = 0, binding = 2) uniform globalUbo {
    vec4 camPos_t;
    Light light[];
} gUbo;

layout(set = 0, binding = 3) uniform ubobject {
	vec4 test;
} ubo;

layout(set = 0, binding  = 3) uniform sampler2D texSampler[10];   // albedo, specRough, normal

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in TB inTB;

layout(location = 0) out vec4 outColor;

void main()
{
	vec3 albedo = texture(texSampler[0], inUV).xyz;
	vec4 specRough = texture(texSampler[1], inUV);
	vec3 normal = planarNormal(texSampler[2], inNormal, inTB, inUV, 1.f);
	
	outColor = getFragColor(albedo, normal, specRough.xyz, specRough.w * 255, gUbo.light, inPos, gUbo.camPos_t.xyz);
}
