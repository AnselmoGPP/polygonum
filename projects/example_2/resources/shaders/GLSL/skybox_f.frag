#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(fragment)

#include "..\..\..\resources\shaders\fragTools.vert"

layout(early_fragment_tests) in;

layout(set = 0, binding = 2) uniform globalUbo {
    vec4 camPos_t;
    Light light[1];
} gUbo;

//layout(set = 0, binding = 3) uniform ubobject
//{
//	vec4 test;
//} ubo;

layout(set = 0, binding  = 3) uniform sampler2D texSampler[6];

layout(location = 0) in vec3 inPos;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor = cubemapTex(inPos, texSampler[0], texSampler[1], texSampler[2], texSampler[3], texSampler[4], texSampler[5]);
	//gl_FragDepth = 1;
	
	//outColor = texture(texSampler, inUVs);
	
	//outColor = vec4(0, 0.5, 0.5, 1);
	//outColor = vec4(fragColor, 1.0);
	//outColor = vec4(texture(texSampler, fragTexCoord).rgb, 1.0);
	//outColor = vec4(fragColor * texture(texSampler, fragTexCoord).rgb, 1.0);
}