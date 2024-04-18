#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(fragment)

#include "..\..\..\resources\shaders\fragTools.vert"

//layout(early_fragment_tests) in;

// Uniform
//layout(set = 0, binding = 0) uniform ubobject {
//	vec4 test;
//} ubo;

// Samplers
//layout(set = 0, binding = 1) uniform sampler2D texSampler[2];		// Opt. depth, Density
layout(set = 0, binding = 0) uniform sampler2D inputAttachments[2];	// Color (sampler2D for single-sample | sampler2DMS for multisampling)

// Input
layout(location = 0) in vec2 inUVs;

// Output
layout(location = 0) out vec4 outColor;

// Functions
vec3 getInversion();
vec3 getGreyScale();
vec3 getSharp();
vec3 getBlur();
vec3 getEdge();

void main()
{
	//outColor = vec4(getBlur(), 1.0);
	outColor = vec4(texture(inputAttachments[0], inUVs).rgb, 1.0);
	//outColor = texelFetch(inputAttachments[0], ivec2(inUVs * textureSize(inputAttachments[0])), gl_SampleID);
	//outColor = vec4(0,0,1,1);
	//outColor = vec4(gl_FragCoord.x, gl_FragCoord.x, gl_FragCoord.x, 1);
	//outColor = vec4(1, 0, 0, 1);
}


vec3 getInversion() { return vec3(1.f - texture(inputAttachments[0], inUVs).rgb); }

vec3 getGreyScale() 
{ 
	vec3 fragColor = texture(inputAttachments[0], inUVs).rgb;
	float greyColor = 0.2126 * fragColor.r + 0.7152 * fragColor.g + 0.0722 * fragColor.b;
	return vec3(greyColor, greyColor, greyColor); 
}

// Kernels:
const float sharpen[9] = { -1, -1, -1, -1,  9, -1, -1, -1, -1 };
const float blur[9] = { 1./16, 2./16, 1./16, 2./16, 4./16, 2./16, 1./16, 2./16, 1./16 };
const float edgeDetection[9] = { 1, 1, 1, 1,-8, 1, 1, 1, 1 };

const float offset = 1.f / 1500.f;
const vec2 kernelOffsets[9] = vec2[](
	vec2(-offset,  offset), 	// top-left
	vec2( 0.f,     offset), 	// top-center
	vec2( offset,  offset), 	// top-right
	vec2(-offset,  0.f   ), 	// center-left
	vec2( 0.f,     0.f   ), 	// center-center
	vec2( offset,  0.f   ), 	// center-right
	vec2(-offset, -offset), 	// bottom-left
	vec2( 0.f,    -offset), 	// bottom-center
	vec2( offset, -offset) 		// bottom-right
);

vec3 applyKernel(float kernel[9])
{
	vec3 colors[9];
	for(int i = 0; i < 9; i++)
		colors[i] = vec3(texture(inputAttachments[0], inUVs + kernelOffsets[i]).rgb);
	
	vec3 finalColor = vec3(0.f);
	for(int i = 0; i < 9; i++) 
		finalColor += colors[i] * kernel[i];
	
	return finalColor;
}

vec3 getSharp() { return applyKernel(sharpen); }
vec3 getBlur()  { return applyKernel(blur); }
vec3 getEdge()  { return applyKernel(edgeDetection); }