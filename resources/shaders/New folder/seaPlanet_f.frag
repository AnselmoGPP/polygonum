#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(fragment)

#include "..\..\extern\polygonum\resources\shaders\fragTools.vert"

layout(early_fragment_tests) in;

#define RADIUS      2000
#define SPEED_1     2
#define SPEED_2     5
#define DIST_1      300
#define DIST_2      600
#define SCALE_1     150
#define SCALE_2     750
#define FOAM_COL    vec3(0.98, 0.98, 0.98)
#define WATER_COL_1 vec3(0.02, 0.26, 0.45)	//https://www.color-hex.com/color-palette/3497	//vec3(0.14, 0.30, 0.36)	// https://colorswall.com/palette/63192
#define WATER_COL_2 vec3(0.11, 0.64, 0.85)	//vec3(0.17, 0.71, 0.61)
#define SPECULARITY vec3(0.7, 0.7, 0.7)
#define ROUGHNESS   30

layout(set = 0, binding = 2) uniform globalUbo {
    vec4 camPos_t;
    Light light[NUMLIGHTS];
} gUbo;

//layout(set = 0, binding = 3) uniform ubobject		// https://www.reddit.com/r/vulkan/comments/7te7ac/question_uniforms_in_glsl_under_vulkan_semantics/
//{
//	vec4 test;
//} ubo;

layout(set = 0, binding  = 3) uniform sampler2D texSampler[10];		// sampler1D, sampler2D, sampler3D

layout(location = 0) in vec3  inPos;
layout(location = 1) in vec3  inNormal;
layout(location = 2) in float inDist;
layout(location = 3) in float inGroundHeight;
layout(location = 4) in TB3	  inTB3;

layout(location = 0) out vec4 outColor;					// layout(location=0) specifies the index of the framebuffer (usually, there's only one).
//layout (location = 0) out vec4 gPos;
//layout (location = 1) out vec4 gAlbedo;
//layout (location = 2) out vec4 gNormal;
//layout (location = 3) out vec4 gSpecRoug;

// Declarations:

vec3 getTex_Sea();	// for FR (forward rendering)
void setData_Sea();	// for DR (deferred rendering)
float getTransparency(float minAlpha, float maxDist);

// Definitions:

void main()
{		
	// DR (Deferred Rendering)
	//setData_Sea();
	
	// FR (Forward Rendering)
	//savePrecalcLightValues(inPos, gUbo.camPos_t.xyz, gUbo.light);
	outColor = vec4(getTex_Sea(), getTransparency(0.4, 50));
}

vec3 getTex_Sea()
{	
	// Colors: https://colorswall.com/palette/63192
	// Colors: https://www.color-hex.com/color-palette/101255
	// Samplers: 0 (normal), 1 (height), 2 (foam) ...
	
	float time = gUbo.camPos_t.w;
	vec3 baseNormal = normalize(inNormal);
	
	// Water color
	vec3 waterColor;
	vec3 tex;
		
	// - Green water (height map)
	tex = triplanarNoColor_Sea(texSampler[1], SCALE_1, SPEED_1, time, inPos, baseNormal).rgb;
	waterColor = mix(WATER_COL_1, WATER_COL_2, getRatio(tex.x, 0.32, 0.70));
	
	// - Foam
	tex = triplanarTexture_Sea(texSampler[2], SCALE_1, SPEED_1, time, inPos, baseNormal).rgb;
	waterColor = mix(waterColor, FOAM_COL, getRatio(tex.x, 0.17, 0.25));
	
	// - Mix
	waterColor = mix(waterColor, WATER_COL_1, getRatio(inDist, DIST_1, DIST_2));
	
	// Normal	
	vec3 normal = mix(
		triplanarNormal_Sea(texSampler[0], SCALE_1, SPEED_1, time, inPos, baseNormal, inTB3),
		triplanarNormal_Sea(texSampler[0], SCALE_2, SPEED_2, time, inPos, baseNormal, inTB3),
		getRatio(inDist, DIST_1, DIST_2) );
	
	// Reflection
	vec4 reflection = cubemapTex(reflectRay(gUbo.camPos_t.xyz, inPos, normal), texSampler[4], texSampler[5], texSampler[6], texSampler[7], texSampler[8], texSampler[9]);
	
	// Final color
	float reflectRatio = 0.3;
	return 
		(1.0 - reflectRatio) * getFragColor(waterColor, normal, SPECULARITY, ROUGHNESS, gUbo.light, inPos, gUbo.camPos_t.xyz).xyz + 
		reflectRatio * reflection.xyz;
}


void setData_Sea()
{	
	// Colors: https://colorswall.com/palette/63192
	// Colors: https://www.color-hex.com/color-palette/101255

	vec4 gPos;
	vec4 gAlbedo;
	vec4 gNormal;
	vec4 gSpecRoug;

	vec3 baseNormal = normalize(inNormal);

	// GREEN & FOAM COLORS
	vec3 waterColor  = WATER_COL_1;
	float time = gUbo.camPos_t.w;
	float ratio;
		
	if(inDist < DIST_2)
	{
		//vec3 finalColor = waterColor;
		
		// Green water (depth map)
		vec3 depth = triplanarNoColor_Sea(texSampler[1], SCALE_1, SPEED_1, time, inPos, baseNormal).rgb;
		if(depth.x > 0.32)
		{
			ratio = getRatio(depth.x, 0.32, 0.70);			// Mix ratio
			waterColor = mix(waterColor, WATER_COL_2, ratio);
		}
		
		// Green water (height from nucleus)
		if(inGroundHeight > 2021) 
		{
			ratio = getRatio(inGroundHeight, 2021, 2027);	// Mix ratio
			waterColor = mix(waterColor, WATER_COL_2, ratio);
		}
	
		// Foam
		vec3 foam = triplanarTexture_Sea(texSampler[2], SCALE_1, SPEED_1, time, inPos, baseNormal).rgb;
		if(foam.x > 0.17) 
		{
			ratio = getRatio(foam.x, 0.17, 0.25);			// Mix ratio
			waterColor = mix(waterColor, FOAM_COL, ratio);
		}
		
		// Mix area
		if(inDist > DIST_1)
		{
			ratio = getRatio(inDist, DIST_1, DIST_2);
			waterColor = mix(waterColor, WATER_COL_1, ratio);
		}
	}

	// NORMALS & LIGHT
	
	float reflectRatio = 0.3;
	vec3 normal;
	
	//    - Close normals
	if(inDist < DIST_1)
	{
		normal = triplanarNormal_Sea(texSampler[0], SCALE_1, SPEED_1, time, inPos, baseNormal, inTB3);
		
		gPos = vec4(inPos, 1.0);
		gAlbedo = vec4( (1 - reflectRatio) * waterColor + reflectRatio * cubemapTex(reflectRay(gUbo.camPos_t.xyz, inPos, normal), texSampler[4], texSampler[5], texSampler[6], texSampler[7], texSampler[8], texSampler[9]).xyz, 1.0 );
		gNormal = vec4(normal, 1.0);
		gSpecRoug = vec4(SPECULARITY, ROUGHNESS);
		return;
	}
	
	//    - Mix area (close and far normals)
	if(inDist < DIST_2)
	{
		normal		 = triplanarNormal_Sea(texSampler[0], SCALE_1, SPEED_1, time, inPos, baseNormal, inTB3);
		vec3 normal2 = triplanarNormal_Sea(texSampler[0], SCALE_2, SPEED_2, time, inPos, baseNormal, inTB3);
		ratio        = getRatio(inDist, 150, 200);
		normal       = mix(normal, normal2, ratio);
		
		gPos = vec4(inPos, 1.0);
		gAlbedo = vec4( (1 - reflectRatio) * waterColor + reflectRatio * cubemapTex(reflectRay(gUbo.camPos_t.xyz, inPos, normal), texSampler[4], texSampler[5], texSampler[6], texSampler[7], texSampler[8], texSampler[9]).xyz, 1.0);
		gNormal = vec4(normal, 1.0);
		gSpecRoug = vec4(SPECULARITY, ROUGHNESS);
		return;
	}
	
	//    - Far normals
	normal = triplanarNormal_Sea(texSampler[0], SCALE_2, SPEED_2, time, inPos, baseNormal, inTB3);
	
	gPos      = vec4(inPos, 1.0);
	gAlbedo   = vec4( (1 - reflectRatio) * waterColor + reflectRatio * cubemapTex(reflectRay(gUbo.camPos_t.xyz, inPos, normal), texSampler[4], texSampler[5], texSampler[6], texSampler[7], texSampler[8], texSampler[9]).xyz, 1.0);
	gNormal   = vec4(normal, 1.0);
	gSpecRoug = vec4(SPECULARITY, ROUGHNESS);
}

float getTransparency(float minAlpha, float maxDist)
{
	float ratio = getRatio(inDist, 0, maxDist);
	return lerp(minAlpha, 1, ratio);
}