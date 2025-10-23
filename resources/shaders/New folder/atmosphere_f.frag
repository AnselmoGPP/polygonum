#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(fragment)

#include "..\..\extern\polygonum\resources\shaders\fragTools.vert"

// layout(early_fragment_tests) in;

#define PLANET_CENTER vec3(0,0,0)
#define PLANET_RADIUS 1400
#define OCEAN_RADIUS 1			
#define ATM_RADIUS 2450
#define NUM_SCATT_POINTS 10			// number of scattering points
#define NUM_OPT_DEPTH_POINTS 10		// number of optical depth points
#define DENSITY_FALLOFF 10
#define SCATT_STRENGTH 10			// scattering strength
#define WAVELENGTHS vec3(700, 530, 440)
#define SCATT_COEFFICIENTS vec3(pow(400/WAVELENGTHS.x, 4)*SCATT_STRENGTH, pow(400/WAVELENGTHS.y, 4)*SCATT_STRENGTH, pow(400/WAVELENGTHS.z, 4)*SCATT_STRENGTH)
#define USE_LOOKUP_TABLE 0			// <<< Using lookup table is slightly slower (~52fps > ~50fps)

// Print distance to atmosphere for some view ray

#define FLT_MAX 3.402823466e+38
#define FLT_MIN 1.175494351e-38
#define DBL_MAX 1.7976931348623158e+308
#define DBL_MIN 2.2250738585072014e-308
#define PI 3.141592653589793238462

//layout(early_fragment_tests) in;

layout(set = 0, binding = 2) uniform globalUbo {
    vec4 camPos_t;
    Light light[NUMLIGHTS];
} gUbo;

//layout(set = 0, binding = 3) uniform ubobject		// https://www.reddit.com/r/vulkan/comments/7te7ac/question_uniforms_in_glsl_under_vulkan_semantics/
//{
//	vec4 test;
//} ubo;

layout(set = 0, binding = 3) uniform sampler2D texSampler[2];			// Opt. depth, Density
layout(set = 0, binding = 4) uniform sampler2D inputAttachments[2];		// Color, Depth (sampler2D for single-sample | sampler2DMS for multisampling)

layout(location = 0) in vec2 inUVs;
layout(location = 1) in vec3 inPixPos;
layout(location = 2) flat in vec3 inCamPos;
layout(location = 3) flat in float inDotLimit;
layout(location = 4) flat in vec3 inLightDir;
layout(location = 5) flat in vec2 inClipPlanes;
layout(location = 6) flat in vec2 inScreenSize;

layout(location = 0) out vec4 outColor;

vec4 originalColor();
vec4 originalColorMSAA();
float depth();				// Non-linear depth
float linearDepth();		// Linear depth
float depthDist();			// World depth (distance)
vec4 optDepthTexture();
vec4 sphere();
vec4 sea();
vec4 atmosphere();

void main()
{
	//vec2 NDCs = { gl_FragCoord.x / 960, gl_FragCoord.y / 540 };	// Get fragment Normalize Device Coordinates (NDC) [0,1] from its Window Coordinates (pixels)
	
	// https://www.reddit.com/r/vulkan/comments/mf1rb5/input_attachment_not_accessible_from_fragment/
	// https://stackoverflow.com/questions/45154213/is-there-a-way-to-address-subpassinput-in-vulkan
	// ChatGPT: The inputAttachment function is only available in GLSL shaders if the "GL_NV_shader_framebuffer_fetch" extension is enabled. This extension is not part of the core Vulkan specification and may not be available on all devices.
	//inputAttachment(0, vec2(NDCs.x, NDCs.y));
	//subpassLoad(inputAttachment, vec2(NDCs.x, NDCs.y));
	
	//outColor = originalColor();
	//outColor = originalColorMSAA();
	//outColor = vec4(1,0,0,1);
	//outColor = vec4(depth(), depth(), depth(), 1.f);
	//outColor = vec4(linearDepth(), linearDepth(), linearDepth(), 1.f);
	//outColor = texture(texSampler, inUVs * vec2(1, -1));
	//outColor = optDepthTexture();
	//outColor = sphere();
	//outColor = sea();			// <<< to do
	if(depth() < 0.9999999) { outColor = originalColor(); return; }
	outColor = atmosphere();	// <<< to optimize (less lookups)
	//if(depth() == 1) outColor = vec4(0,1,0,1);
}

vec4 getTexture(sampler2D   tex, vec2  uv) { return texture(tex, uv); }
vec4 getTexture(sampler2DMS tex, vec2  uv) { return texelFetch(tex, ivec2(uv * textureSize(tex)), gl_SampleID); }

// No post processing. Get the original color.
vec4 originalColor() { return getTexture(inputAttachments[0], inUVs); }

vec4 originalColorMSAA() { return getTexture(inputAttachments[0], inUVs); }

// Get non linear depth. Range: [0, 1]
float depth() {	return getTexture(inputAttachments[1], inUVs).x; }

// Get linear depth. Range: [0, 1]. Link: https://stackoverflow.com/questions/51108596/linearize-depth
float linearDepth()
{
	float zNear = inClipPlanes[0];
	float zFar = inClipPlanes[1];
	
	return (zNear * zFar / (zFar + depth() * (zNear - zFar))) / (zFar - zNear);
}

// Get dist from near plane to fragment.
float depthDist()
{
	float zNear = inClipPlanes[0];
	float zFar = inClipPlanes[1];
	
	return zNear * zFar / (zFar + depth() * (zNear - zFar));
}

// Draw sphere
vec4 sphere() 
{ 
	vec4 color   = vec4(0,0,1,1);
	vec3 nuclPos = vec3(0,0,0);
	vec3 pixDir  = normalize(inPixPos - inCamPos);
	vec3 nuclDir = normalize(nuclPos  - inCamPos); 
	
	if(dot(pixDir, nuclDir) > inDotLimit) return color;
	else return originalColor();
}

// Draw sea
vec4 sea()
{
	vec4 color   = vec4(0,0,1,1);
	vec3 nuclPos = vec3(0,0,0);
	vec3 pixDir  = normalize(inPixPos - inCamPos);
	vec3 nuclDir = normalize(nuclPos  - inCamPos); 
	
	if(dot(pixDir, nuclDir) > inDotLimit) 
	{
		if(depthDist() > 0.000000) return color;
	}
	
	return originalColor();
}

// Returns vector(distToSphere, distThroughSphere). 
//		If rayOrigin is inside sphere, distToSphere = 0. 
//		If ray misses sphere, distToSphere = maxValue; distThroughSphere = 0.
vec2 raySphere(vec3 planetCentre, float atmRadius, vec3 rayOrigin, vec3 rayDir)
{
	// Number of intersections
	vec3 offset = rayOrigin - planetCentre;
	float a = 1;						// Set to dot(rayDir, rayDir) if rayDir might not be normalized
	float b = 2 * dot(offset, rayDir);
	float c = dot(offset, offset) - atmRadius * atmRadius;
	float d = b * b - 4 * a * c;		// Discriminant of quadratic formula (sqrt has 2 solutions/intersections when positive)
	
	// Two intersections (d > 0)
	if(d > 0)	
	{
		float s = sqrt(d);
		float distToSphereNear = max(0, (-b - s) / (2 * a));
		float distToSphereFar = (-b + s) / (2 * a);
		
		if(distToSphereFar >= 0)		// Ignore intersections that occur behind the ray
			return vec2(distToSphereNear, distToSphereFar - distToSphereNear);
	}
	
	// No intersection (d < 0) or one (d = 0)
	return vec2(FLT_MAX, 0);			// https://stackoverflow.com/questions/16069959/glsl-how-to-ensure-largest-possible-float-value-without-overflow
}

// Atmosphere: 
//		https://developer.nvidia.com/gpugems/gpugems2/part-ii-shading-lighting-and-shadows/chapter-16-accurate-atmospheric-scattering
//		https://github.com/SebLague/Solar-System/blob/Development/Assets/Scripts/Celestial/Shaders/PostProcessing/Atmosphere.shader
//		https://www.youtube.com/watch?v=DxfEbulyFcY

// Atmosphere's density at one point. The closer to the surface, the denser it is.
float densityAtPoint(vec3 point)
{
	float heightAboveSurface = length(point - PLANET_CENTER) - PLANET_RADIUS;
	float height01 = heightAboveSurface / (ATM_RADIUS - PLANET_RADIUS);
	
	//return exp(-height01 * DENSITY_FALLOFF);					// There is always some density
	return exp(-height01 * DENSITY_FALLOFF) * (1 - height01);	// Density ends at some distance
}

float densityAtPointBaked(vec3 point)
{
	float height01 = (length(point - PLANET_CENTER) - PLANET_RADIUS) / (ATM_RADIUS - PLANET_RADIUS);
	
	return texture(texSampler[1], vec2(0, height01)).x;
}

// Average atmosphere density along a ray.
float opticalDepth(vec3 rayOrigin, vec3 rayDir, float rayLength)
{
	float opticalDepth = 0;
	vec3 point = rayOrigin;
	float stepSize = rayLength / (NUM_OPT_DEPTH_POINTS - 1);
	
	for(int i = 0; i < NUM_OPT_DEPTH_POINTS; i++)		// <<< Bug? It has one more level
	{
		opticalDepth += densityAtPoint(point) * stepSize;
		point += rayDir * stepSize;
	}
	
	return opticalDepth;
}

// Optical depth for a complete ray (from camera or atmosphere horizon, to the other side of the atmosphere horizon).
float rayOpticalDepthBaked(vec3 rayOrigin, vec3 rayDir)
{
	//float angle01  = 1 - (dot(normalize(rayOrigin - PLANET_CENTER), rayDir) * 0.5 + 0.5);					// Non-linear [0, 1] = [0 rad, 3.14 rad]
	float angle01  = getAngle(normalize(rayOrigin - PLANET_CENTER), rayDir) / PI;							// Linear [0, 1] = [0 rad, 3.14 rad]
	float height01 = (length(rayOrigin - PLANET_CENTER) - PLANET_RADIUS) / (ATM_RADIUS - PLANET_RADIUS);	// [0, 1] = [floor, top]

	return texture(texSampler[0], vec2(angle01, height01)).x;
}

// Optical depth for a segment of ray.
float segmentOpticalDepthBaked(vec3 rayOrigin, vec3 rayDir, float rayLength) 
{
	if(length(inCamPos - PLANET_CENTER) > ATM_RADIUS)
		return rayOpticalDepthBaked(rayOrigin, rayDir);
	
	//vec3 endPoint = inCamPos;
	vec3 endPoint = rayOrigin + rayDir * rayLength;	// == inCamPos;
	
	float od1 = rayOpticalDepthBaked(rayOrigin, rayDir) - rayOpticalDepthBaked(endPoint, rayDir);	// Down side
	float od2 = rayOpticalDepthBaked(endPoint, -rayDir) - rayOpticalDepthBaked(rayOrigin, -rayDir);	// Up side
	//if(od1 < 0) od1 = 0;
	//if(od2 < 0) od2 = 0;	// <<< BUG (negative borders appear, so I have to fix them this way)
		
	float deviation = dot(rayDir, normalize(inCamPos - PLANET_CENTER));	// [-1, 1]
		
	float range = 0.1;
	float ratio = clamp((deviation + range) / (2 * range), 0, 1);	// [0, 1]
	return ratio * od1 + (1 - ratio) * od2;
	
	if(deviation > 0) return od1;
	else return od2;
}

// Describe the view ray of the camera through the atmosphere for the current pixel.
vec3 calculateLight(vec3 rayOrigin, vec3 rayDir, float rayLength, vec3 originalCol)
{
	vec3 inScatterPoint = rayOrigin;					// point to study
	vec3 inScatteredLight = vec3(0,0,0);				// light gathered
	float stepSize = rayLength / (NUM_SCATT_POINTS - 1);
	float viewRayOpticalDepth = 0;
	vec3 dirToSun = -inLightDir;
	
	for(int i = 0; i < NUM_SCATT_POINTS; i++)
	{
		#if (USE_LOOKUP_TABLE == 1)
			//float sunRayOpticalDepth = rayOpticalDepthBaked(inScatterPoint, dirToSun);
			//viewRayOpticalDepth = segmentOpticalDepthBaked(inScatterPoint, -rayDir, stepSize * i);
			float sunRayLength = raySphere(PLANET_CENTER, ATM_RADIUS, inScatterPoint, dirToSun).y;	// length point-sunHit
			float sunRayOpticalDepth = opticalDepth(inScatterPoint, dirToSun, sunRayLength);		// OD: point-sunHit
			viewRayOpticalDepth = opticalDepth(inScatterPoint, -rayDir, stepSize * i);				// OD: point-backHit

			float localDensity = densityAtPointBaked(inScatterPoint);			
		#else
			float sunRayLength = raySphere(PLANET_CENTER, ATM_RADIUS, inScatterPoint, dirToSun).y;	// length point-sunHit
			float sunRayOpticalDepth = opticalDepth(inScatterPoint, dirToSun, sunRayLength);		// OD: point-sunHit
			viewRayOpticalDepth = opticalDepth(inScatterPoint, -rayDir, stepSize * i);				// OD: point-backHit
			float localDensity = densityAtPoint(inScatterPoint);									// Atmosphere density at point
		#endif
		
		vec3 transmittance = exp(-(sunRayOpticalDepth + viewRayOpticalDepth) * SCATT_COEFFICIENTS);	// Inversely proportional to optical depths
		
		inScatteredLight += localDensity * transmittance * SCATT_COEFFICIENTS * stepSize;			// Light gathered for now <<< local density ?
		inScatterPoint   += rayDir * stepSize;														// Next point
	}
	
	//inScatteredLight *= SCATT_COEFFICIENTS * intensity * stepSize / planetRadius;
	
	float originalColTransmittance = exp(-viewRayOpticalDepth);
	
	vec3 finalColor;
	if(false)	//(depth() == 1.f)	// Obscure skybox during day
	{
		float range[2] = { 0.80, 0.90 };	// When transmittance < range[0], skybox is obscured. When transmittance > range[1], skybox is clear. Intermediate values are linearly mixed
		float ratio = clamp(originalColTransmittance, range[0], range[1]);
		ratio = (ratio - range[0]) / (range[1] - range[0]);
		originalCol = ratio * originalCol + (1-ratio) * vec3(0.005, 0.005, 0.005);
		finalColor =  originalCol * originalColTransmittance + inScatteredLight;
	}
	else
		finalColor = originalCol * originalColTransmittance + inScatteredLight;

	return finalColor;
}

// Final pixel color (atmosphere)
vec4 atmosphere()
{	
	// Camera ray
	vec3 rayOrigin = inCamPos;
	vec3 rayDir = normalize(inPixPos - inCamPos);	// normalize(inCamDir);
	
	// Distance to surface
	float distToOcean = raySphere(PLANET_CENTER, OCEAN_RADIUS, rayOrigin, rayDir).x;
	float distToSurface = min(depthDist(), distToOcean);
	
	// Distance to atmosphere and through it (to the ground or the other side)
	vec2 hitInfo = raySphere(PLANET_CENTER, ATM_RADIUS, rayOrigin, rayDir);
	float distToAtmosphere = hitInfo.x;
	float distThroughAtmosphere = min(hitInfo.y, distToSurface - distToAtmosphere);
	
	// Get fragment light
	vec4 originalCol = originalColor();
	if(distThroughAtmosphere > 0)
	{
		const float epsilon = 0.001;
		vec3 firstPoint = rayOrigin + rayDir * (distToAtmosphere + epsilon);
		vec3 light = calculateLight(firstPoint, rayDir, distThroughAtmosphere - 2 * epsilon, originalCol.xyz);
		return vec4(light, 1);
	}
	
	return originalCol;
}

vec4 optDepthTexture()
{	
	// Test 1: Complete map
	if(true)
	{
		float heightRange = ATM_RADIUS - PLANET_RADIUS;
		float angleRange = PI;
			
		if(false)	// from texture
		{
			// Print texture directly:
			//vec2 UVs = vec2(inUVs.x, 1 - inUVs.y);
			//float color = texture(texSampler[0], UVs).x;
			
			// Compute texture:
			vec3 point = vec3(PLANET_RADIUS + heightRange * inUVs.y, 0, 0);
			float angle = angleRange * inUVs.x;
			vec3 rayDir = vec3(cos(angle), 0, sin(angle));
			
			float color = rayOpticalDepthBaked(point, rayDir);
			return vec4(color, color, color, 1.f); 
		}
		else		// from computation
		{
			vec2 UVs = vec2(gl_FragCoord.x / inScreenSize.x, gl_FragCoord.y / inScreenSize.y);
			vec3 point = vec3(PLANET_RADIUS + heightRange * (1 - UVs.y), 0, 0);
			float angle = UVs.x * angleRange;
			vec3 rayDir = vec3(cos(angle), sin(angle), 0);
			float rayLength = raySphere(PLANET_CENTER, ATM_RADIUS, point, rayDir).y;
			
			float color = opticalDepth(point, rayDir, rayLength);
			return vec4(color, color, color, 1.f);
		}
	}

	// Test 2: Single position, all rays
	if(false)
	{
		vec3 inScatterPoint = vec3(2200, 0, 0);
		float angleRange = 3.141592653589793238462;
		float angle = inUVs.y * angleRange;
		vec3 rayDir = vec3(cos(angle), 0, sin(angle));
		float viewRayOpticalDepth;
		
		inScatterPoint = vec3(0, 2400, 0);
		rayDir = vec3(0.9088, -0.417233, 0);
		
		if(gl_FragCoord.x < 500)
		{
			viewRayOpticalDepth = rayOpticalDepthBaked(inScatterPoint, rayDir);
			//viewRayOpticalDepth = segmentOpticalDepthBaked(inScatterPoint, -rayDir, stepSize * i);
		}
		else
		{
			float rayLength = raySphere(PLANET_CENTER, ATM_RADIUS, inScatterPoint, rayDir).y;
			viewRayOpticalDepth = opticalDepth(inScatterPoint, rayDir, rayLength);
		}
		
		return vec4(viewRayOpticalDepth, viewRayOpticalDepth, viewRayOpticalDepth, 1.f);
		// 0.1483 / 2117.25 / 2.00119 / (0.9088, -0.417233, 0)
		// 0.1470 > 0.1441
		// 0.4508 * 10 = 4.508
	}

	// Test 3: Density map
	if(false)
	{
		vec2 UVs = vec2(inUVs.x, 1 - inUVs.y);
		float color = texture(texSampler[1], UVs).x;
		return vec4(color, color, color, 1.f);
	}
	
	// Test 4: Density map comparison
	if(false)
	{
		float color;
		if(gl_FragCoord.x < 500)
			color = densityAtPoint(vec3(1440, 0, 0));
		else
			color = densityAtPointBaked(vec3(1440, 0, 0));
		
		return vec4(color, color, color, 1.f);
	}
}

/*
	Sean O'Neil (2005) Accurate atmospheric scattering. GPU gems 2
		
	- Sun light comes into atmosphere, scatters at some point, and is reflected toward the camera.
	- Two rays that join: camera ray to some atmospheric point, and sun ray to that atmospheric point.
	- Take camera ray through the atmosphere > Get some points across the ray > Calculate scattering on each point.
	- Along both rays, the atmosphere scatters some light away.
	- Scattering types:
		- Rayleigh: Light scatters on small molecules more light from shorter wavelengths (blue < green < red)
		- Mie: Light scatters on larger particles and scatters all wavelengths of light equally.
	- Functions:
		- Phase function: How much light is scattered toward the direction of the camera based on the angle both rays.
		- Out-scattering equation: Inner integral. Scattering through the camera ray.
		- In- scattering equation: How much light is added to a ray due to light scattering from sun.
		- Surface-scattering equation: Scattered light reflected from a surface
*/

// https://stackoverflow.com/questions/38938498/how-do-i-convert-gl-fragcoord-to-a-world-space-point-in-a-fragment-shader
vec4 ndc2world2(vec2 ndc)
{
	// Window-screen space
	//	gl_FragCoord.xy: Window-space position of the fragment
	//	gl_FragCoord.z: Depth value of the fragment in window space used for depth testing. Range: [0, 1] (0 = near clipping plane; 1 = far clipping plane).
	//	gl_FragCoord.w: Window-space position of the fragment in the homogeneous coordinate system. Used for scaling elements in perspective projections depending upon distance to camera. Calculated by the graphics pipeline based on the projection matrix and the viewport transformation.
	
	// Normalized Device Coordinates
	vec2 winSize = vec2(1920/2, 1080/2);
	vec2 depthRange = vec2(100, 5000);		// { near, far}
		
	vec4 NDC;
	NDC.xy = 2.f * (gl_FragCoord.xy / winSize.xy) - 1.f;
	NDC.z = (2.f * gl_FragCoord.z - depthRange.x - depthRange.y) / (depthRange.y - depthRange.x);
	NDC.w = 1.f;
	
	// Clip space > Eye space > World space
	vec4 clipPos  = NDC / gl_FragCoord.w;
	mat4 inProj;
	mat4 inView;
	vec4 eyePos   = inverse(inProj) * clipPos;
	vec4 worldPos = inverse(inView) * eyePos;
 	return worldPos;
}