#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(fragment)

#include "..\..\..\resources\shaders\fragTools.vert"

layout(early_fragment_tests) in;

layout(set = 0, binding = 2) uniform globalUbo {
    vec4 camPos_t;
    Light light[NUMLIGHTS];
} gUbo;

//layout(set = 0, binding = 3) uniform ubObj
//{
//	vec4 test;
//} ubo;

layout(set = 0, binding  = 3) uniform sampler2D texSampler[5];		// Textures: albedo, normals, specular, roughness, height

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUVs;
layout(location = 2) in vec3 inTangent;		// U ( x)
layout(location = 3) in vec3 inBitangent;	// V (-y)
layout(location = 4) in vec3 inNormal;		// Z ( z)

layout (location = 0) out vec4 gPos;
layout (location = 1) out vec4 gAlbedo;
layout (location = 2) out vec4 gNormal;
layout (location = 3) out vec4 gSpecRoug;

void main()
{	
	gPos      = vec4(inPos, 1.f);
	gAlbedo   = getColor(texSampler[0], inUVs, 1);
	//gNormal   = vec4(inNormal, 1.f);
	gNormal   = getNormal(texSampler[1], inUVs, 1, inTangent, inBitangent, inNormal);
    gSpecRoug = vec4(
		getData(texSampler[2], inUVs, 1.f).xyz, 
		getData(texSampler[3], inUVs, 1.f).x);
}