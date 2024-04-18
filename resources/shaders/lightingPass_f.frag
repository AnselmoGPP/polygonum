#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(fragment)

#include "..\..\..\resources\shaders\fragTools.vert"

//layout(early_fragment_tests) in;

// Uniform
layout(set = 0, binding = 0) uniform ubobject {
	vec4 camPos;
	Light lights[NUMLIGHTS];
} ubo;

// Samplers
//layout(set = 0, binding = 1) uniform sampler2D texSampler[2];		// Opt. depth, Density
layout(set = 0, binding = 1) uniform sampler2D inputAttachments[4];	// Position, Albedo, Normal, Specular_roughness (sampler2D for single-sample | sampler2DMS for multisampling)

// Input
layout(location = 0) in vec2 inUVs;

// Output
layout(location = 0) out vec4 outColor;

// Functions
vec4 showPositions(float divider) { return vec4(texture(inputAttachments[0], inUVs).xyz / divider, 1.0); }
vec4 showAlbedo() { return vec4(texture(inputAttachments[1], inUVs).xyz, 1.0); }
vec4 showNormals() { return vec4(texture(inputAttachments[2], inUVs).xyz, 1.0); }
vec4 showSpecularity() { return vec4(texture(inputAttachments[3], inUVs).xyz, 1.0); }
vec4 showRoughness() { return vec4(vec3(texture(inputAttachments[3], inUVs).w), 1.0); }

void main()
{
	//outColor = showPositions(5000); return;
	//outColor = showAlbedo();        return;
	//outColor = showNormals();       return;
	//outColor = showSpecularity();   return;
	//outColor = showRoughness();     return;
	
	vec3 fragPos = texture(inputAttachments[0], inUVs).xyz;
	vec3 albedo = texture(inputAttachments[1], inUVs).xyz;
	vec3 normal = texture(inputAttachments[2], inUVs, 1).xyz;		//unpackNormal(texture(inputAttachments[2], unpackUV(inUVs, 1)).xyz);	//unpackNormal(texture(inputAttachments[2], unpackUV(inUVs, 1)).xyz);
	vec4 specRough = texture(inputAttachments[3], inUVs);
	
	savePrecalcLightValues(fragPos, ubo.camPos.xyz, ubo.lights);	//ubo.lightPosDir);
	//TB3 empty;
	//savePNT(fragPos, normalize(inNormal), empty);
	
	outColor = vec4(getFragColor(albedo, normal, specRough.xyz, specRough.w * 255), 1.0);
}
