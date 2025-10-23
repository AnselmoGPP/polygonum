#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(fragment)

#include "..\..\extern\polygonum\resources\shaders\fragTools.vert"

layout(early_fragment_tests) in;

layout(set = 0, binding = 2) uniform globalUbo {
    vec4 camPos_t;
    Light light[NUMLIGHTS];
} gUbo;

layout(set = 0, binding = 3) uniform ubObj
{
	vec4 test;
} ubo;

layout(set = 0, binding  = 3) uniform sampler2D texSampler[10];   // albedo, specRough, normal

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in TB inTB;

layout (location = 0) out vec4 outPos;
layout (location = 1) out vec4 outAlbedo;
layout (location = 2) out vec4 outSpecRoug;
layout (location = 3) out vec4 outNormal;

void main()
{	
	vec3 worldPos = inPos;
	vec4 albedo = vec4(texture(texSampler[0], inUV).xyz, 1.f);
	vec4 specRough = texture(texSampler[1], inUV);
	vec3 normal = planarNormal(texSampler[2], inNormal, inTB, inUV, 1.f);

	outPos      = vec4(worldPos, 1.f);
	outAlbedo   = albedo;
	outSpecRoug = vec4(specular, roughness);
	outNormal   = vec4(normalize(inNormal), 1.0);
}