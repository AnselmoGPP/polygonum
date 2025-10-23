#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(vertex)

//#include "..\..\extern\polygonum\resources\shaders\vertexTools.vert"

layout(set = 0, binding = 0) uniform globalUbo {
    mat4 view;
    mat4 proj;
    vec4 camPos_t;
} gUbo;

layout(set = 0, binding = 1) uniform ubobject {
	vec4 fov;			// float
	vec4 aspRatio;		// float (height/width)
	vec4 camPos;		// vec3
	vec4 camDir;		// vec3
	vec4 camUp;			// vec3
	vec4 camRight;		// vec3
	vec4 lightDir;		// vec3
	vec4 clipPlanes;	// vec2
	vec4 screenSize;	// vec2
	mat4 view;			// mat4
	mat4 proj;			// mat4
} ubo;

layout (location = 0) in vec3 inPos;				// NDC position. Since it's in NDCs, no MVP transformation is required-
layout (location = 1) in vec2 inUVs;

layout(location = 0) out vec2 outUVs;				// UVs
layout(location = 1) out vec3 outPixPos;			// Pixel world position
layout(location = 2) flat out vec3 outCamPos;
layout(location = 3) flat out float outDotLimit;	// Used for rendering spheres
layout(location = 4) flat out vec3 outLightDir;
layout(location = 5) flat out vec2 outClipPlanes;
layout(location = 6) flat out vec2 outScreenSize;

vec4 ndc2world();		//!< Get vertex world space coordinates
float getDotLimit();

void main()
{	
	gl_Position = vec4(inPos, 1.0f);
	//gl_Position.x = gl_Position.x * ubo.aspRatio.x;
    outUVs = inUVs;
	outPixPos = ndc2world().xyz;
	outCamPos = ubo.camPos.xyz;
	outDotLimit = getDotLimit();
	outLightDir = normalize(ubo.lightDir.xyz);	
	outClipPlanes = ubo.clipPlanes.xy;
	outScreenSize = ubo.screenSize.xy;
}

//vec4 world2clip() { return ubo.proj * ubo.view * ubo.model * vec4(inPos, 1.0); }

vec4 ndc2world()
{
	float side = tan(ubo.fov.x / 2);
	
	vec3 world = ubo.camPos.xyz + ubo.camDir.xyz;
	world -= inPos.y * ubo.camUp.xyz * side;
	world += inPos.x * ubo.camRight.xyz * (side * ubo.aspRatio.x); 
	
	return vec4(world, 1.0f);
}

float getDotLimit()
{
	float radius = 1000;
	vec3 nuclPos = vec3 (0,0,0);
	vec3 vecDist = nuclPos - ubo.camPos.xyz;
	
	float distNucleus = sqrt(vecDist.x * vecDist.x + vecDist.y * vecDist.y + vecDist.z * vecDist.z);
	float angle = asin(radius / distNucleus);
	return cos(angle);
}