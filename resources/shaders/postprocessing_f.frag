#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(fragment)

//#include "..\..\..\resources\shaders\fragTools.vert"			// modify this path according to your folder system

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

void main()
{
	//outColor = vec4(getBlur(), 1.0);
	outColor = vec4(texture(inputAttachments[0], inUVs).rgb, 1.0);
	//outColor = texelFetch(inputAttachments[0], ivec2(inUVs * textureSize(inputAttachments[0])), gl_SampleID);
	//outColor = vec4(0,0,1,1);
	//outColor = vec4(gl_FragCoord.x, gl_FragCoord.x, gl_FragCoord.x, 1);
	//outColor = vec4(1, 0, 0, 1);
}
