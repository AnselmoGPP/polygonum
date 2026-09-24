#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(vertex)

// Input
layout (location = 0) in vec3 inPos;				// NDC position. Since it's in NDCs, no MVP transformation is required

// Output
layout(location = 0) out vec2 outUV;				// UVs

// Functions
void main()
{
	//gl_Position.x = gl_Position.x * ubo.aspRatio.x;
	gl_Position = vec4(inPos, 1.0f);
    outUV = inPos.xy * 0.5 + 0.5;   // Convert clip space [-1.0, 1.0] -> texture space [0.0, 1.0]
}