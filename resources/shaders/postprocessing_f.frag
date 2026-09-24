#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(fragment)

// Samplers
layout(set = 0, binding = 0) uniform sampler2D inputAttachments[2];	// Color (sampler2D for single-sample | sampler2DMS for multisampling)

// Input
layout(location = 0) in vec2 inUV;

// Output
layout(location = 0) out vec4 outColor;

// Functions

void main()
{
	//outColor = vec4(getBlur(), 1.0);
	outColor = vec4(texture(inputAttachments[0], inUV).rgb, 1.0);
	//outColor = texelFetch(inputAttachments[0], ivec2(inUV * textureSize(inputAttachments[0])), gl_SampleID);
	//outColor = vec4(0,0,1,1);
	//outColor = vec4(gl_FragCoord.x, gl_FragCoord.x, gl_FragCoord.x, 1);
	//outColor = vec4(1, 0, 0, 1);
	//if(texture(inputAttachments[1], inUV).x > 0.9999) outColor = vec4(0.11, 0.32, 0.65, 1.0);
}