#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(vertex)

#include "..\..\extern\polygonum\resources\shaders\vertexTools.vert"

#define RADIUS 2000
#define MIN 100			// min. dist.
#define MAX 200			// max. dist.
#define WAVES 6
#define COUNT 6

vec3  dir      [WAVES] = { vec3(1,0,0), vec3(0, SR05, -SR05), vec3(0,0,1), vec3(SR05, SR05, 0), vec3(0,1,0), vec3(-SR05, 0, SR05) };// Set of unit vectors
float speed    [WAVES] = { 0.8,  0.7,  0.6, 0.5,  0.4, 0.3 };
float w        [WAVES] = { 0.05, 0.06, 0.1, 0.15, 0.2, 0.25 };	// Frequency (number of cycles in 2π)
float A        [WAVES] = { 0.1,  0.1,  0.1, 0.1,  0.1, 0.1 };	// Amplitude
float steepness[WAVES] = { 0.1,  0.1,  0.2, 0.2,  0.3, 0.3 };	// [0,1]
float Q(int i) { return steepness[i] * 1 / (w[i] * A[i]); }		// Steepness [0, 1/(w·A)] (bigger values produce loops)

layout(set = 0, binding = 0) uniform globalUbo {
    mat4 view;
    mat4 proj;
    vec4 camPos_t;
} gUbo;

layout(set = 0, binding = 1) uniform ubobject {
    mat4 model;					// mat4
    mat4 normalMatrix;			// mat3
	vec4 sideDepthsDiff;
} ubo;

layout(location = 0) in vec3 inPos;					// Each location has 16 bytes
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inGapFix;

layout(location = 0)  		out vec3	outPos;			// Vertex position.
layout(location = 1)  		out vec3	outNormal;		// Ground normal
layout(location = 2)  		out float	outDist;		// Distace vertex-camera
layout(location = 3)		out float	outGroundHeight;// Ground height over nucleus
layout(location = 4)  		out TB3		outTB3;			// Tangents & Bitangents

void adjustWavesAmplitude(float maxDepth, float minDepth, float minAmplitude);	// Adjust amplitude based on soil depth under camera. Waves are max. when soilHeight < (RADIUS - maxDepth) and min. when soilHeight > (RADIUS - minDepth)
vec3 getSeaOptimized(inout vec3 normal, float min, float max);
vec3 getUpDownSea(float radius, float speed, float maxCamDist);
vec3 GerstnerWaves(vec3 pos, inout vec3 normal);			// Gerstner waves standard (https://developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-1-effective-water-simulation-physical-models)
vec3 GerstnerWaves_sphere(vec3 pos, inout vec3 normal);		// Gerstner waves projected along the sphere surface.
vec3 GerstnerWaves_sphere2(vec3 pos, inout vec3 normal);	// Gerstner waves projected perpendicularly from the direction line to the sphere

void main()
{
	//adjustWavesAmplitude(15, 1, 0.1);
	//for(int i = 0; i < WAVES; i++) A[i] *= 0.3;
	
	vec3 normal     = inNormal;
	vec3 pos;
	//pos = inPos;								// No waves
	pos = getUpDownSea(0.25, 0.6, 300);			// Up & Down
	//pos = getSeaOptimized(normal, MIN, MAX);	// Waves
				    
	outPos          = pos;
	outNormal       = mat3(ubo.normalMatrix) * normal;
	//if(inGapFix.x != 0) outNormal *= -1;					// show chunk limits
	outDist         = getDist(pos, gUbo.camPos_t.xyz);		// Dist. to wavy geoid
	outGroundHeight = sqrt(pos.x * pos.x + pos.y * pos.y + pos.z * pos.z);
	gl_Position		= gUbo.proj * gUbo.view * ubo.model * vec4(pos, 1);
	
	outTB3 = getTB3(normal);
}

void adjustWavesAmplitude(float maxDepth, float minDepth, float minAmplitude)
{
	float ratio = minAmplitude + 1.f - getRatio(gUbo.camPos_t.w, RADIUS - maxDepth, RADIUS - minDepth);
	for(int i = 0; i < WAVES; i++) A[i] *= ratio;
}

vec3 getUpDownSea(float radius, float speed, float maxCamDist)
{
	float ratio = getRatio(getDist(inPos, gUbo.camPos_t.xyz), maxCamDist, 0);
	
	return (inPos + sin(speed * gUbo.camPos_t.w) * (inNormal * radius) * ratio);
}

vec3 getSeaOptimized(inout vec3 normal, float min, float max)
{
	float surfDist = getDist(inPos, gUbo.camPos_t.xyz);					// Dist. to the sphere, not the wavy geoid.
	vec3 pos_1      = fixedPos(inPos, inGapFix, ubo.sideDepthsDiff);
	vec3 norm_1     = normal;
	
	if(surfDist > max) return pos_1;
	else
	{
		vec3 pos_2  = GerstnerWaves_sphere(pos_1, normal);
		
		if(surfDist < min) return pos_2;
		else
		{
			float ratio = 1 - getRatio(surfDist, min, max);
			vec3 diff;
			
			diff = normal - norm_1;
			normal = norm_1 + diff * ratio;
			
			diff = pos_2 - pos_1;
			return pos_1 + diff * ratio;
		}
	}
	
	/*
	if(dist > MAX_RANGE) 
		return fixedPos(inPos, inGapFix, ubo.sideDepthsDiff);
	else if(dist > MIN_RANGE)
	{
		float ratio = 1 - (dist - MIN_RANGE) / (MAX_RANGE - MIN_RANGE);
		vec3 pos_0 = fixedPos(inPos, inGapFix, ubo.sideDepthsDiff);
		vec3 pos_1 = GerstnerWaves_sphere(pos_0, normal);
		vec3 diff = pos_1 - pos_0;
		
		return pos_0 + ratio * diff;
	}
	else 
		return GerstnerWaves_sphere(fixedPos(inPos, inGapFix, ubo.sideDepthsDiff), normal);
	*/
}

vec3 getSphereDir(vec3 planeDir, vec3 normal)
{
	vec3 right = cross(planeDir, normal);
	return cross(normal, right);				// front
}

// Gerstner waves applied over a sphere, perpendicular to normal
vec3 GerstnerWaves_sphere(vec3 pos, inout vec3 normal)
{
	vec3 newPos      = pos;
	vec3 newNormal   = normal;
	//float multiplier = 0.9;
	float horDisp, verDisp;			// Horizontal/Vertical displacement
	float arcDist;					// Arc distance (-direction-origin-vertex)
	float rotAng;
	float frontProj;
	vec3 rotAxis;
	//vec3 right, front;
	vec3 up = normalize(pos);
	mat3 rotMat;
	
	for(int i = 0; i < COUNT; i++)
	{
		arcDist = getAngle(-dir[i], up) * RADIUS;
		rotAxis = cross(dir[i], up);
		
		horDisp = cos(w[i] * arcDist + speed[i] * gUbo.camPos_t.w);
		verDisp = sin(w[i] * arcDist + speed[i] * gUbo.camPos_t.w);
		
		// Vertex
		rotAng  = (Q(i) * A[i]) * horDisp / RADIUS;		
		rotMat  = rotationMatrix(rotAxis, rotAng);
		newPos  = rotMat * newPos;		// <<< what if rotation axis is == 0?
		newPos += normalize(newPos) * A[i] * verDisp;
		
		// Normal
		rotAng = asin(w[i] * A[i] * horDisp);					// <<< correct? Dark shadows when A==2 or w==2
		newNormal = rotationMatrix(rotAxis, -rotAng) * newNormal;
		
		// Normal (alternative)
		//newNormal = rotMat * newNormal;												// Move normal according to vertex displacement
		//frontProj = dot(newNormal, normalize(cross(up, rotAxis)));					// Project normal on front
		//rotAng    = acos(frontProj - (w[i] * A[i] * horDisp)) - acos(frontProj);	// Angle (Alpha - Beta)
		//newNormal = rotationMatrix(rotAxis, rotAng) * newNormal;
	}

	normal = newNormal;
	return newPos;
}

/*
// Gerstner waves applied over a sphere, perpendicular to direction
vec3 GerstnerWaves_sphere2(vec3 pos, inout vec3 normal)
{
	float speed      = SPEED;
	float w          = FREQUENCY;				// Frequency (number of cycles in 2π)
	float A          = AMPLITUDE;				// Amplitude
	float steepness  = STEEPNESS;				// [0,1]
	float Q          = steepness * 1 / (w * A);	// Steepness [0, 1/(w·A)] (bigger values produce loops)
	const int count  = 6;
		
	vec3 dir[count]  = { vec3(1,0,0), vec3(SR05, SR05, 0), vec3(0,1,0), vec3(0, SR05, -SR05), vec3(0,0,1), vec3(-SR05, 0, SR05) };
	vec3 newPos      = pos;
	vec3 newNormal   = normal;
	vec3 unitPos     = normalize(pos);
	float multiplier = 0.95;
	float cosVal, sinVal;
	
	for(int i = 0; i < count; i++)
	{
		cosVal = cos(w * dot(dir[i], pos) + speed * ubo.time.x);
		sinVal = sin(w * dot(dir[i], pos) + speed * ubo.time.x);
		
		newPos.x += Q * A * dir[i].x * cosVal;
		newPos.y += Q * A * dir[i].y * cosVal;
		newPos.z += Q * A * dir[i].z * cosVal;
		newPos   += unitPos * A * sinVal;
		
		dir[i] = getSphereDir(dir[i], normal);
		
		newNormal.x -= w * A * dir[i].x * cosVal;
		newNormal.y -= w * A * dir[i].y * cosVal;
		newNormal.z -= w * A * dir[i].z * cosVal;
		//newNormal   -= w * A * Q * sinVal;		//<<< this is subtracting a float, but I use a vec3 (normal)
		
		speed *= multiplier;
		w /= multiplier;
		A *= multiplier;
		Q *= multiplier;
	}
	
	normal = normalize(newNormal);
	return newPos;
}

// Gerstner waves applied over a 2D surface
vec3 GerstnerWaves(vec3 pos, inout vec3 normal)
{
	float speed     = 3;
	float w         = 0.010;			// Frequency (number of cycles in 2π)
	float A         = 100;				// Amplitude
	float Q         = 0.5;				// Steepness [0, 1/(w·A)] (bigger values produce loops)
	const int count = 6;
	vec3 dir[count] = { vec3(1,0,0), vec3(SR05, SR05, 0), vec3(0,1,0), vec3(0, SR05, -SR05), vec3(0,0,1), vec3(-SR05, 0, SR05) };
	vec3 newPos     = { pos.x, pos.y, 0 };
	//normal        = { 0, 0, 1 };
	float cosVal, sinVal;	
	
	for(int i = 0; i < count; i++)
	{
		cosVal = cos(w * dot(dir[i], pos) + speed * ubo.time.x);
		sinVal = sin(w * dot(dir[i], pos) + speed * ubo.time.x);
		
		newPos.x += A * Q * dir[i].x * cosVal;
		newPos.y += A * Q * dir[i].y * cosVal;
		newPos.z += A * sinVal;
		
		normal.x -= w * A * dir[i].x * cosVal;
		normal.y -= w * A * dir[i].y * cosVal;
		normal.z -= w * A * Q * sinVal;
	}
	
	normalize(normal);
	return newPos;
}
*/

/*
	Notes:
		- gl_Position:    Contains the position of the current vertex (you have to pass the vertex value)
		- gl_VertexIndex: Index of the current vertex, usually from the vertex buffer.
		- (location = X): Indices for the inputs that we can use later to reference them. Note that some types, like dvec3 64 bit vectors, use multiple slots:
							layout(location = 0) in dvec3 inPosition;
							layout(location = 2) in vec3 inColor;
		- MVP transformations: They compute the final position in clip coordinates. Unlike 2D triangles, the last component of the clip coordinates may not be 1, which will result 
		in a division when converted to the final normalized device coordinates on the screen. This is used in perspective projection for making closer objects look larger than 
		objects that are further away.
		- Multiple descriptor sets: You can bind multiple descriptor sets simultaneously by specifying a descriptor layout for each descriptor set when creating the pipeline layout. 
		Shaders can then reference specific descriptor sets like this:  "layout(set = 0, binding = 0) uniform UniformBufferObject { ... }". This way you can put descriptors that 
		vary per-object and descriptors that are shared into separate descriptor sets, avoiding rebinding most of the descriptors across draw calls
*/