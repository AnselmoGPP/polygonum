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
    mat4 model;					// mat4
    mat4 normalMatrix;			// mat3
} ubo;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUVs;

layout(location = 0) out vec3 outPos;

void main()
{
	//gl_Position  = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
	mat4 MV = mat4(mat3(gUbo.view * ubo.model));				// Take away translation and scaling from the MV matrix
	gl_Position  = (gUbo.proj * MV * vec4(inPos, 1.0)).xyww;	// this ensures gl_FragDepth == 1
	outPos = inPos;
}


/*
	Notes:
		- gl_Position:    Contains the position of the current vertex (you have to pass the vertex value)
		- gl_VertexIndex: Index of the current vertex, usually from the vertex buffer.
		- (location = X): Indices for the inputs that we can use later to reference them. Note that some types, like dvec3 64 bit vectors, use multiple slots:
							layout(location = 0) in dvec3 inPosition;
							layout(location = 2) in vec3 inColor;
		- MVP transformations: They compute the final position in clip coordinates. Unlike 2D triangles, the last component of the clip coordinates may not be 1, which will result in a division when converted to the final normalized device coordinates on the screen. This is used in perspective projection for making closer objects look larger than objects that are further away.
		- Multiple descriptor sets: You can bind multiple descriptor sets simultaneously by specifying a descriptor layout for each descriptor set when creating the pipeline layout. Shaders can then reference specific descriptor sets like this:  "layout(set = 0, binding = 0) uniform UniformBufferObject { ... }". This way you can put descriptors that vary per-object and descriptors that are shared into separate descriptor sets, avoiding rebinding most of the descriptors across draw calls
*/