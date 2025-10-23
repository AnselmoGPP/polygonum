#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(fragment)

#include "..\..\extern\polygonum\resources\shaders\fragTools.vert"

layout(early_fragment_tests) in;

#define RADIUS 2000
#define SEALEVEL 2000
#define DIST_1 5
#define DIST_2 8

#define GRAS 0		// grass
#define ROCK 1		// rock
#define PSNW 2		// plain snow
#define RSNW 3		// rough snow
#define SAND 4		// sand

layout(set = 0, binding = 2) uniform globalUbo {
    vec4 camPos_t;
    Light light[NUMLIGHTS];
} gUbo;

//layout(set = 0, binding = 3) uniform ubobject		// https://www.reddit.com/r/vulkan/comments/7te7ac/question_uniforms_in_glsl_under_vulkan_semantics/
//{
//	vec4 test;
//} ubo;

layout(set = 0, binding  = 3) uniform sampler2D texSampler[34];		// sampler1D, sampler2D, sampler3D

layout(location = 0)  		in vec3 	inPos;
layout(location = 1)  		in vec3 	inNormal;
layout(location = 2)  		in float	inSlope;
layout(location = 3)  		in float	inDist;
layout(location = 4)  flat	in float	inCamSqrHeight;
layout(location = 5)		in float	inGroundHeight;
layout(location = 6)  		in TB3	 	inTB3;

//layout(location = 0) out vec4 outColor;					// layout(location=0) specifies the index of the framebuffer (usually, there's only one).
layout (location = 0) out vec4 gPos;
layout (location = 1) out vec4 gAlbedo;
layout (location = 2) out vec4 gNormal;
layout (location = 3) out vec4 gSpecRoug;

// Declarations:

void applyBlackBottom(float minHeight, float maxHeight);
void getTexture_Sand(inout vec3 result);
void setData_grassRock();
vec3 heatMap(float seaLevel, float maxHeight);
vec3 naturalMap(float seaLevel, float maxHeight);

// Definitions:

const int iGrass = 0, iRock = 1, iPSnow = 2, iRSnow = 3, iSand = 4;		// indices

void main()
{
	setData_grassRock();
	applyBlackBottom(SEALEVEL-2, SEALEVEL-0);
	
	//gPos.xyz = inPos;
	//gAlbedo = vec4(0.2, 0.8, 0.2, 1.f);
	//gNormal.xyz = normalize(inNormal);
	//gSpecRoug.xyz = vec3(0.5, 0.5, 0.5);
	//gSpecRoug.w = 100;
	
	////float blackRatio = 0;
	//float blackRatio = getBlackRatio(1990, 2000);
	//if(blackRatio == 1) { outColor = vec4(0,0,0,1); return; }
	
	////vec3 color = naturalMap(RADIUS, RADIUS + 150);
	////vec3 color = heatMap(RADIUS, RADIUS + 150);
	//outColor = vec4(color, 1.0);
}


void applyBlackBottom(float minHeight, float maxHeight)
{
	float ratio = getRatio(inGroundHeight, minHeight, maxHeight);
	
	gAlbedo.xyz *= ratio;
	gSpecRoug.xyz *= ratio;
}

// Get ratio of rock given a slope threshold (slopeThreshold) and a slope mixing range (mixRange).
float getRockRatio(float slopeThreshold, float mixRange)
{
	return clamp((inSlope - (slopeThreshold - mixRange)) / (2 * mixRange), 0.f, 1.f);
}

// Get ratio of snow given a slope threshold mixing range (mixRange), and a snow height range at equator (minHeight, maxHeight).
float getSnowRatio_Poles(float mixRange, float minHeight, float maxHeight)
{
	float lat    = atan(abs(inPos.z) / sqrt(inPos.x * inPos.x + inPos.y * inPos.y));
	float decrement = maxHeight * (lat / (PI/2.f));				// height range decreases with latitude
	float slopeThreshold = ((inGroundHeight-RADIUS) - (minHeight-decrement)) / ((maxHeight-decrement) - (minHeight-decrement));	// Height ratio == Slope threshold
	
	return getRatio(inSlope, slopeThreshold + mixRange, slopeThreshold - mixRange);
}

float getSnowRatio_Height(float mixRange, float minHeight, float maxHeight, float slope)
{
	float slopeRatio = getRatio(inGroundHeight - RADIUS, minHeight, maxHeight);
	return getRatio(slope, slopeRatio, slopeRatio - mixRange);
}


void setData_grassRock()
{
	vec3 baseNormal = normalize(inNormal);
	
	// Texture resolution and Ratios.
	float tf[2];														// texture factors
	float ratioMix  = getTexScaling(inDist, 10, 40, 0.2, tf[0], tf[1]);	// params: fragDist, initialTexFactor, baseDist, mixRange.

	// Get all necessary textures
	Material grassPar[2];
	Material rockPar [2];
	Material pSnowPar[2];
	//Material rSnowPar[2];
	Material sandPar [2];
	
	for(int i = 0; i < 2; i++)
	{
		grassPar[i].albedo = triplanarTexture(texSampler[0],  tf[i], inPos, baseNormal).rgb;
		grassPar[i].normal = triplanarNormal (texSampler[1],  tf[i], inPos, baseNormal, inTB3);
		grassPar[i].spec   = triplanarNoColor(texSampler[2],  tf[i], inPos, baseNormal).rgb;
		grassPar[i].rough  = triplanarNoColor(texSampler[3],  tf[i], inPos, baseNormal).r;
		grassPar[i].height = triplanarNoColor(texSampler[4],  tf[i], inPos, baseNormal).r;
		
		rockPar[i].albedo  = triplanarTexture(texSampler[5],  tf[i], inPos, baseNormal).rgb;
		rockPar[i].normal  = triplanarNormal (texSampler[6],  tf[i], inPos, baseNormal, inTB3);
		rockPar[i].spec    = triplanarNoColor(texSampler[7],  tf[i], inPos, baseNormal).rgb;
		rockPar[i].rough   = triplanarNoColor(texSampler[8],  tf[i], inPos, baseNormal).r;
		rockPar[i].height  = triplanarNoColor(texSampler[9],  tf[i], inPos, baseNormal).r;
		
		pSnowPar[i].albedo = triplanarTexture(texSampler[15], tf[i], inPos, baseNormal).rgb;
		pSnowPar[i].normal = triplanarNormal (texSampler[16], tf[i], inPos, baseNormal, inTB3);
		pSnowPar[i].spec   = triplanarNoColor(texSampler[17], tf[i], inPos, baseNormal).rgb;
		pSnowPar[i].rough  = triplanarNoColor(texSampler[18], tf[i], inPos, baseNormal).r;
		
		//rSnowPar[i].albedo = triplanarTexture(texSampler[10], tf[i], inPos, baseNormal).rgb;
		//rSnowPar[i].normal = triplanarNormal (texSampler[11], tf[i], inPos, baseNormal, inTB3);
		//rSnowPar[i].spec   = triplanarNoColor(texSampler[12], tf[i], inPos, baseNormal).rgb;
		//rSnowPar[i].rough  = triplanarNoColor(texSampler[13], tf[i], inPos, baseNormal).r;
		
		sandPar[i].albedo  = triplanarTexture(texSampler[25], tf[i], inPos, baseNormal).rgb;
		sandPar[i].normal  = triplanarNormal (texSampler[26], tf[i], inPos, baseNormal, inTB3);
		sandPar[i].spec    = triplanarNoColor(texSampler[27], tf[i], inPos, baseNormal).rgb;
		sandPar[i].rough   = triplanarNoColor(texSampler[28], tf[i], inPos, baseNormal).r;
		
		grassPar[i].normal = mix(grassPar[i].normal, pSnowPar[i].normal, getRatio(inDist, 0.2 * DIST_2, DIST_2, 0.2, 1.0));
	}
	
	// Mix data depending upon distance
	Material grass;
	Material rock; 
	//Material pSnow;
	//Material rSnow;
	Material sand;
	
	grass.albedo = mix(grassPar[1].albedo, grassPar[0].albedo, ratioMix);
	grass.normal = mix(grassPar[1].normal, grassPar[0].normal, ratioMix);
	grass.spec   = mix(grassPar[1].spec,   grassPar[0].spec,   ratioMix);
	grass.rough  = mix(grassPar[1].rough,  grassPar[0].rough,  ratioMix);
	grass.height = mix(grassPar[1].height, grassPar[0].height, ratioMix);
	
	rock.albedo  = mix(rockPar[1].albedo,  rockPar[0].albedo,  ratioMix);
	rock.normal  = mix(rockPar[1].normal,  rockPar[0].normal,  ratioMix);
	rock.spec    = mix(rockPar[1].spec,    rockPar[0].spec,    ratioMix);
	rock.rough   = mix(rockPar[1].rough,   rockPar[0].rough,   ratioMix);
	rock.height  = mix(rockPar[1].height,  rockPar[0].height,  ratioMix);
	
	//pSnow.albedo = mix(pSnowPar[1].albedo, pSnowPar[0].albedo, ratioMix);
	//pSnow.normal = mix(pSnowPar[1].normal, pSnowPar[0].normal, ratioMix);
	//pSnow.spec   = mix(pSnowPar[1].spec,   pSnowPar[0].spec,   ratioMix);
	//pSnow.rough  = mix(pSnowPar[1].rough,  pSnowPar[0].rough,  ratioMix);
	
	//rSnow.albedo = mix(rSnowPar[1].albedo, rSnowPar[0].albedo, ratioMix);
	//rSnow.normal = mix(rSnowPar[1].normal, rSnowPar[0].normal, ratioMix);
	//rSnow.spec   = mix(rSnowPar[1].spec,   rSnowPar[0].spec,   ratioMix);
	//rSnow.rough  = mix(rSnowPar[1].rough,  rSnowPar[0].rough,  ratioMix);
	
	sand.albedo  = mix(sandPar[1].albedo,  sandPar[0].albedo,  ratioMix);
	sand.normal  = mix(sandPar[1].normal,  sandPar[0].normal,  ratioMix);
	sand.spec    = mix(sandPar[1].spec,    sandPar[0].spec,    ratioMix);
	sand.rough   = mix(sandPar[1].rough,   sandPar[0].rough,   ratioMix);
	
	// Grass dry color
	vec3 dryColor = getDryColor(vec3(0.9, 0.6, 0), RADIUS + 15, RADIUS + 50, inGroundHeight);
	grass.albedo *= dryColor;
	
	// Mix: Grass + Rock
	Material result;
	
	// - Grass & rock
	float ratioRock = getRockRatio(0.22, 0.02);   // params: slopeThreshold, mixRange

	result.albedo = mixByHeight(grass.albedo, rock.albedo, grass.height, rock.height, ratioRock, 0.5);
	result.normal = mixByHeight(grass.normal, rock.normal, grass.height, rock.height, ratioRock, 0.5);
	result.spec   = mixByHeight(grass.spec,   rock.spec,   grass.height, rock.height, ratioRock, 0.5);
	result.rough  = mixByHeight(grass.rough,  rock.rough,  grass.height, rock.height, ratioRock, 0.5);
	
	// - Coast level (rocky coast)
	float ratioCoast = 1.f - getRatio(inGroundHeight, SEALEVEL, SEALEVEL + 1);

	result.albedo = mixByHeight(result.albedo, rock.albedo, grass.height, rock.height, ratioCoast, 0.1);
	result.normal = mixByHeight(result.normal, rock.normal, grass.height, rock.height, ratioCoast, 0.1);
	result.spec   = mixByHeight(result.spec,   rock.spec,   grass.height, rock.height, ratioCoast, 0.1);
	result.rough  = mixByHeight(result.rough,  rock.rough,  grass.height, rock.height, ratioCoast, 0.1);
	
	// Sand
	float maxSlope = 1.f - getRatio(inGroundHeight, SEALEVEL - 60, SEALEVEL + 5);
	float sandRatio = 1.f - getRatio(inSlope, maxSlope - 0.05, maxSlope);
	
	result.albedo = mix(result.albedo, sand.albedo, sandRatio);
	result.normal = mix(result.normal, sand.normal, sandRatio);
	result.spec   = mix(result.spec,   sand.spec,   sandRatio);
	result.rough  = mix(result.rough,  sand.rough,  sandRatio);
	
	// Snow
	//Material snow;
	//snow.albedo  = mix(pSnow.albedo, rSnow.albedo, ratioRock);		// mix plain snow and rough snow
	//snow.normal  = mix(pSnow.normal, rSnow.normal, ratioRock);
	//snow.spec    = mix(pSnow.spec,   rSnow.spec,   ratioRock);
	//snow.rough   = mix(pSnow.rough,  rSnow.rough,  ratioRock);
	
	////float ratioSnow = getSnowRatio_Poles(0.1, 100, 140);			// params: mixRange, minHeight, maxHeight
	//float ratioSnow = getSnowRatio_Height(0.5, 100, 120, inSlope);
	//
	//result.albedo = mix(result.albedo, snow.albedo, ratioSnow);	// mix snow with soil
	//result.normal = mix(result.normal, snow.normal, ratioSnow);
	//result.spec   = mix(result.spec,   snow.spec,   ratioSnow);
	//result.rough  = mix(result.rough,  snow.rough,  ratioSnow);
	
	// Set g-buffer
	gPos = vec4(inPos, 1.0);
	gAlbedo = vec4(result.albedo, 1.0);
	gNormal = vec4(normalize(result.normal), 1.0);
	gSpecRoug = vec4(result.spec, result.rough);
}


void getTexture_Sand(inout vec3 result)
{
    float slopeThreshold = 0.04;          // sand-plainSand slope threshold
    float mixRange       = 0.02;          // threshold mixing range (slope range)
    float tf             = 50;            // texture factor
	vec3 baseNormal      = normalize(inNormal);
	
	float ratio = clamp((inSlope - slopeThreshold) / (2 * mixRange), 0.f, 1.f);
	
	vec3 dunes  = getFragColor(
						triplanarTexture(texSampler[17], tf, inPos, baseNormal).rgb,
						triplanarNormal(texSampler [18], tf, inPos, baseNormal, inTB3).rgb,
						triplanarNoColor(texSampler[19], tf, inPos, baseNormal).rgb,
						triplanarNoColor(texSampler[20], tf, inPos, baseNormal).r * 255,
						gUbo.light, inPos, gUbo.camPos_t.xyz ).xyz;
	
	vec3 plains = getFragColor(
						triplanarTexture(texSampler[21], tf, inPos, baseNormal).rgb,
						triplanarNormal(texSampler [22], tf, inPos, baseNormal, inTB3).rgb,
						triplanarNoColor(texSampler[23], tf, inPos, baseNormal).rgb,
						triplanarNoColor(texSampler[24], tf, inPos, baseNormal).r * 255,
						gUbo.light, inPos, gUbo.camPos_t.xyz ).xyz;

	result = mix(dunes, plains, ratio);
}

vec3 heatMap(float seaLevel, float maxHeight)
{
	// Fill arrays (heights & colors)
	const int size = 6;

	float heights[size] = { -0.1, -0.05, 0.05, 0.15, 0.5, 0.9 };
	for(int i = 0; i < size; i++) heights[i] = seaLevel + heights[i] * (maxHeight - seaLevel);
	
	vec3 colors[size] = { vec3(0.1,0.1,0.5), vec3(0.25,0.25,1), vec3(0.25,1,0.25), vec3(1,1,0.25), vec3(1,0.25,0.25), vec3(1,1,1) };
	
	// Colorize
	if(inGroundHeight < heights[0]) return colors[0];
	
	for(int i = 1; i < size; i++)
		if(inGroundHeight < heights[i])
			return lerp(colors[i-1], colors[i],	
						(inGroundHeight - heights[i-1]) / (heights[i] - heights[i-1]) );
	
	return colors[size -1];
}

vec3 naturalMap(float seaLevel, float maxHeight)
{
	// Fill arrays (heights & colors)
	const int size = 8;
	
	float heights[size] = { -0.1, -0.05, 0.05, 0.15, 0.3, 0.5, 0.7, 0.9 };
	for(int i = 0; i < size; i++) heights[i] = seaLevel + heights[i] * (maxHeight - seaLevel);
	
	vec3 colors[size] = { vec3(0.23, 0.43, 0.79), vec3(0.25, 0.44, 0.80), vec3(0.85, 0.84, 0.53), vec3(0.38, 0.64, 0.090), vec3(0.28, 0.47, 0.06), vec3(0.39, 0.31, 0.27), vec3(0.34, 0.27, 0.24), vec3(1, 1, 1) };
	
	// Colorize
	if(inGroundHeight < heights[0]) return colors[0];
	
	for(int i = 1; i < size; i++)
		if(inGroundHeight < heights[i])
			return lerp(colors[i-1], colors[i],	
						(inGroundHeight - heights[i-1]) / (heights[i] - heights[i-1]) );
	
	return colors[size -1];
}