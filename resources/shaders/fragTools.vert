
// Index ------------------------------------------------------------------------

//	Constants;
//		NUMLIGHTS
//		PI
//		PI2
//		PI05
//      E
//      SR05
//      GAMMA
//	Data structures:
//		Light
//		PreCalcValues
//		TB3
//		uvGradient
//	Graphics:
//		toRGB
//		toSRGB
//		unpackUV
//		unpackUVmirror
//		unpackNormal
//		mixByHeight
//	Save:
//		savePrecalcLightValues
//	Math:
//		getDist
//		getSqrDist
//		getLength
//		getSqrLength
//		getRatio
//		getDirection
//		getAngle
//		getModulus
//		lerp
//		reflectRay
//	Lighting:
//		directionalLightColor
//		PointLightColor
//		SpotLightColor
//		getFragColor
//	Planar texture:
//		planarNormal
//		cubemapTex
//		getTex   (example)
//	Triplanar texture:
//		triplanarTexture
//		triplanarNoColor
//		triplanarNormal
//		getGradients
//		triplanarTexture_Sea
//		triplanarNoColor_Sea
//		triplanarNormal_Sea
//	Postprocessing:
		vec3 getInversion(vec3 fragColor);
		vec3 getGreyScale(vec3 fragColor);
		//vec3 applyKernel(float kernel[9], sampler2D sampler, vec2 uv);
//	Others:
//		getTexScaling
//		getLowResDist
//		applyParabolicFog
//		rand
//		applyOrderedDithering
//		getDryColor


// Constants ------------------------------------------------------------------------

#define NUMLIGHTS 3
#define PI 3.141592653589793238462
#define PI2 6.283185307179586476924
#define PI05 1.570796326794896619231
#define E  2.718281828459045235360
#define SR05 0.707106781	// == sqrt(0.5)     (vec2(SR05, SR05) has module == 1)
#define GAMMA 2.2


// Data structures (& global variables) ------------------------------------------------------------------------

struct Light
{
	int type;			// int   0: no light   1: directional   2: point   3: spot

    vec3 position;		// vec3
    vec3 direction;		// vec3

    vec3 ambient;		// vec3
    vec3 diffuse;		// vec3
    vec3 specular;		// vec3

    vec3 degree;		// vec3	(constant, linear, quadratic) (for attenuation)
    vec2 cutOff;		// vec2 (cuttOff, outerCutOff)
} light[NUMLIGHTS];

// Variables used for calculating light that should be precalculated.
struct PreCalcValues
{
	vec3 halfwayDir[NUMLIGHTS];		// Bisector of the angle viewDir-lightDir
	vec3 lightDirFrag[NUMLIGHTS];	// Light direction from lightSource to fragment
	float attenuation[NUMLIGHTS];	// How light attenuates with distance (ratio)
	float intensity[NUMLIGHTS];		//
} pre;

// Tangent (T) and Bitangent (B)
struct TB
{
	vec3 tan;		// U ( x) direction in bump map
	vec3 bTan;		// V (-y) direction in bump map
	//vec3 normal;	// Z ( z) direction in tangent space
};

// Tangent (T) and Bitangent (B) of each dimension (3)
struct TB3
{
	vec3 tanX;
	vec3 bTanX;
	vec3 tanY;
	vec3 bTanY;
	vec3 tanZ;
	vec3 bTanZ;
};

// Gradients for the X and Y texture coordinates can be used for fetching the textures when non-uniform flow control exists (https://www.khronos.org/opengl/wiki/Sampler_(GLSL)#Non-uniform_flow_control).  
struct uvGradient
{
	vec2 uv;		// xy texture coords
	vec2 dFdx;		// Gradient of x coords
	vec2 dFdy;		// Gradient of y coords
};

// Material components
struct Material
{
	vec3 albedo;
	vec3 normal;
	vec3 spec;
	float rough;
	float height;
};

// Math functions ------------------------------------------------------------------------

// Get distance between two 3D points
float getDist(vec3 a, vec3 b) 
{
	vec3 diff = a - b;
	return sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z); 
}

// Get square distance between two 3D points
float getSqrDist(vec3 a, vec3 b) 
{
	vec3 diff = a - b;
	return diff.x * diff.x + diff.y * diff.y + diff.z * diff.z; 
}

float getLength(vec3 a)
{
	//return sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
	return length(a);
}

float getSqrLength(vec3 a)
{
	//return a.x * a.x + a.y * a.y + a.z * a.z;
	return dot(a, a);
}

// Get the ratio for a given "value" within a range [min, max]. Result's range: [0, 1].
float getRatio(float value, float min, float max)
{
	return clamp((value - min) / (max - min), 0, 1);
}

// Get the ratio for a given "value" within a range [min, max]. Result's range: [minR, maxR].
float getRatio(float value, float min, float max, float minR, float maxR)
{
	return minR + clamp((value - min) / (max - min), 0, 1) * (maxR - minR);
}

// Get a direction given 2 points.
vec3 getDirection(vec3 origin, vec3 end)
{
	return normalize(end - origin);
}

// Get angle between 2 unit vectors (directions).
float getAngle(vec3 a, vec3 b)
{
	//return acos( dot(a, b) / ((length(a) * length(b)) );	// Non-unit vectors
	return acos(dot(a, b));									// Unit vectors
}

// Get modulus(%) = a - (b * floor(a/b)) (https://registry.khronos.org/OpenGL-Refpages/gl4/html/mod.xhtml)
float getModulus(float dividend, float divider) { return dividend - (divider * floor(dividend/divider)); }

// Linear interpolation. Position between a and b located at a given ratio [0,1]
float lerp(float a, float b, float ratio) { return a + (b - a) * ratio; }

vec3 lerp(vec3 a, vec3 b, float t) { return a + (b - a) * t; }

vec3 reflectRay(vec3 camPos, vec3 fragPos, vec3 normal)
{
	return reflect(
		normalize(fragPos - camPos),	// incident ray
		normal);
}

// Graphic functions ------------------------------------------------------------------------

// Transforms non-linear sRGB color to linear RGB. Note: Usually, input is non-linear sRGB, but it's automatically converted to linear RGB in the shader, and output later in sRGB.
vec3 toRGB(vec3 vec)
{
	//return pow(vec, vec3(1.0/GAMMA));
	
	vec3 linear;
	
	if (vec.x <= 0.04045) linear.x = vec.x / 12.92;
	else linear.x = pow((vec.x + 0.055) / 1.055, 2.4);
	
	if (vec.y <= 0.04045) linear.y = vec.y / 12.92;
	else linear.y = pow((vec.y + 0.055) / 1.055, 2.4);
	
	if (vec.z <= 0.04045) linear.z = vec.z / 12.92;
	else linear.z = pow((vec.z + 0.055) / 1.055, 2.4);
	
	return linear;
}

vec4 toRGB(vec4 vec)
{
	//return pow(vec, vec3(1.0/GAMMA));
	
	vec4 linear;
	
	if (vec.x <= 0.04045) linear.x = vec.x / 12.92;
	else linear.x = pow((vec.x + 0.055) / 1.055, 2.4);
	
	if (vec.y <= 0.04045) linear.y = vec.y / 12.92;
	else linear.y = pow((vec.y + 0.055) / 1.055, 2.4);
	
	if (vec.z <= 0.04045) linear.z = vec.z / 12.92;
	else linear.z = pow((vec.z + 0.055) / 1.055, 2.4);
	
	linear.w = vec.w;
	
	return linear;
}

// Transforms linear RGB color to non-linear sRGB
vec3 toSRGB(vec3 vec)
{
	//return pow(vec, vec3(GAMMA));
	
	vec3 nonLinear;
	
	if (vec.x <= 0.0031308) nonLinear.x = vec.x * 12.92;
	else nonLinear.x = 1.055 * pow(vec.x, 1.0/2.4) - 0.055;
	
	if (vec.y <= 0.0031308) nonLinear.y = vec.y * 12.92;
	else nonLinear.y = 1.055 * pow(vec.y, 1.0/2.4) - 0.055;
	
	if (vec.z <= 0.0031308) nonLinear.z = vec.z * 12.92;
	else nonLinear.z = 1.055 * pow(vec.z, 1.0/2.4) - 0.055;
	
	return nonLinear;
}

vec4 toSRGB(vec4 vec)
{
	//return pow(vec, vec3(GAMMA));
	
	vec4 nonLinear;
	
	if (vec.x <= 0.0031308) nonLinear.x = vec.x * 12.92;
	else nonLinear.x = 1.055 * pow(vec.x, 1.0/2.4) - 0.055;
	
	if (vec.y <= 0.0031308) nonLinear.y = vec.y * 12.92;
	else nonLinear.y = 1.055 * pow(vec.y, 1.0/2.4) - 0.055;
	
	if (vec.z <= 0.0031308) nonLinear.z = vec.z * 12.92;
	else nonLinear.z = 1.055 * pow(vec.z, 1.0/2.4) - 0.055;
	
	nonLinear.w = vec.w;
	
	return nonLinear;
}

// Invert Y axis (for address mode == repeat) and apply scale
vec2 unpackUV(vec2 UV, float texFactor)
{
	return UV.xy * vec2(1, -1) / texFactor;
}

// Invert Y axis (for address mode == mirror) and apply scale
vec2 unpackUVmirror(vec2 UV, float texFactor)
{
	return (vec2(0, 1) + UV.xy * vec2(1, -1)) / texFactor;
}

// Correct color space (to linear), put in range [-1,1], and normalize
vec3 unpackNormal(vec3 normal)
{
	return normalize(toSRGB(normal) * 2.f - 1.f);		// Color space correction is required to counter gamma correction.
}

// Correct color space (to linear), put in range [-1,1], normalize, and adjust normal strength.
vec3 unpackNormal(vec3 normal, float multiplier)
{
	normal = normalize(toSRGB(normal) * 2.f - 1.f);		// Color space correction is required to counter gamma correction.

	normal.z -= (1 - normal.z) * multiplier;
	normal.z = clamp(normal.z, 0, 1);
	
	return normalize(normal);

	// ------------------

	normal = normalize(toSRGB(normal) * 2.f - 1.f);

	float phi   = atan(normal.y / normal.x);
	float theta = atan(sqrt(normal.x * normal.x + normal.y * normal.y), normal.z);

	//theta = clamp(theta * multiplier, 0, PI/2);
	
	//if(theta < 0.1) return normal;

	return normalize(
			vec3(
				sin(theta) * cos(phi),
				sin(theta) * sin(phi),
				cos(theta) ));
}

// Mix 2 textures based on their height maps.
vec3 mixByHeight(vec3 tex_A, vec3 tex_B, float height_A, float height_B, float ratio, float depth)
{
	float ma = max(height_B  + ratio, height_A + (1-ratio)) - depth;
	float b1 = max(height_B  + ratio     - ma, 0);
	float b2 = max(height_A + (1-ratio) - ma, 0);
	return (tex_B * b1 + tex_A * b2) / (b1 + b2);
}

// Mix 2 floats based on their height maps.
float mixByHeight(float f_A, float f_B, float height_A, float height_B, float ratio, float depth)
{
	float ma = max(height_B  + ratio, height_A + (1-ratio)) - depth;
	float b1 = max(height_B  + ratio     - ma, 0);
	float b2 = max(height_A + (1-ratio) - ma, 0);
	return (f_B * b1 + f_A * b2) / (b1 + b2);
}


// Save functions ------------------------------------------------------------------------
// They store shader variables in this library, making them global for this library and allowing it to use them.

// (UNOPTIMIZED) Precalculate (to avoid repeating calculations) and save (for making them global for this library) variables required for calculating light.
void savePrecalcLightValues(vec3 fragPos, vec3 camPos, Light inLight[NUMLIGHTS])
{
	vec3 viewDir = normalize(camPos - fragPos);	// Camera view direction
	float distFragLight;						// Distance fragment-lightSource
	float theta;								// The closer to 1, the more direct the light gets to fragment.
	float epsilon;								// Cutoff range
	
	for(int i = 0; i < NUMLIGHTS; i++)
	{
		light[i].type      = inLight[i].type;

		light[i].position  = inLight[i].position.xyz;
		light[i].direction = inLight[i].direction.xyz;
	
		light[i].ambient   = inLight[i].ambient.xyz;
		light[i].diffuse   = inLight[i].diffuse.xyz;
		light[i].specular  = inLight[i].specular.xyz;
	
		light[i].degree    = inLight[i].degree.xyz;
		light[i].cutOff    = inLight[i].cutOff.xy;
		
		switch(light[i].type)
		{
		case 1:		// directional
			pre.halfwayDir[i]	= normalize(-light[i].direction + viewDir);
			break;
		case 2:		// point
			distFragLight		= length(light[i].position - fragPos);
			pre.lightDirFrag[i]	= normalize(fragPos - light[i].position);
			pre.halfwayDir[i]   = normalize(-pre.lightDirFrag[i] + viewDir);
			pre.attenuation[i]  = 1.0 / (light[i].degree[0] + light[i].degree[1] * distFragLight + light[i].degree[2] * distFragLight * distFragLight);
			break;
		case 3:		// spot
			distFragLight		= length(light[i].position - fragPos);
			pre.lightDirFrag[i]	= normalize(fragPos - light[i].position);
			pre.halfwayDir[i]   = normalize(-pre.lightDirFrag[i] + viewDir);
			pre.attenuation[i]	= 1.0 / (light[i].degree[0] + light[i].degree[1] * distFragLight + light[i].degree[2] * distFragLight * distFragLight);
			theta				= dot(pre.lightDirFrag[i], light[i].direction);
			epsilon				= light[i].cutOff[0] - light[i].cutOff[1];
			pre.intensity[i]	= clamp((theta - light[i].cutOff[1]) / epsilon, 0.0, 1.0);
			break;
		default:
			break;
		}
	}
}


// Lightning functions ------------------------------------------------------------------------

// Directional light (sun)
vec3 directionalLightColor(vec3 albedo, vec3 normal, vec3 specularity, float roughness, Light light, vec3 fragDir)
{
	vec3 halfwayDir = normalize(-light.direction + fragDir);
	
	// ----- Ambient lighting -----
	vec3 ambient  = light.ambient * albedo;
	if(dot(light.direction, normal) > 0) return ambient;			// If light comes from below the tangent plane
	
	// ----- Diffuse lighting -----
	float diff    = max(dot(normal, -light.direction), 0.f);
	vec3 diffuse  = light.diffuse * albedo * diff;
	
	// ----- Specular lighting -----
	float spec    = pow(max(dot(normal, halfwayDir), 0.0), roughness * 4);
	vec3 specular = light.specular * specularity * spec;
	
	// ----- Result -----
	return vec3(ambient + diffuse + specular);
}

// Point light (bulb)
vec3 PointLightColor(vec3 albedo, vec3 normal, vec3 specularity, float roughness, Light light, vec3 fragDir, vec3 fragPos)
{
	float distFragLight = length(light.position - fragPos);
	vec3  lightDirFrag  = normalize(fragPos - light.position);
	vec3  halfwayDir    = normalize(-lightDirFrag + fragDir);
	float attenuation   = 1.0 / (light.degree[0] + light.degree[1] * distFragLight + light.degree[2] * distFragLight * distFragLight);
	
    // ----- Ambient lighting -----
    vec3 ambient  = light.ambient * albedo * attenuation;
	if(dot(lightDirFrag, normal) > 0) return ambient;				// If light comes from below the tangent plane

    // ----- Diffuse lighting -----
    float diff    = max(dot(normal, -lightDirFrag), 0.f);
    vec3 diffuse  = light.diffuse * albedo * diff * attenuation;
	
    // ----- Specular lighting -----
	float spec    = pow(max(dot(normal, halfwayDir), 0.0), roughness * 4);
	vec3 specular = light.specular * specularity * spec * attenuation;
	
    // ----- Result -----
    return vec3(ambient + diffuse + specular);	
}

// Spot light (spotlight)
vec3 SpotLightColor(vec3 albedo, vec3 normal, vec3 specularity, float roughness, Light light, vec3 fragDir, vec3 fragPos)
{
	float distFragLight	= length(light.position - fragPos);
	vec3  lightDirFrag  = normalize(fragPos - light.position);
	vec3  halfwayDir    = normalize(-lightDirFrag + fragDir);
	float attenuation   = 1.0 / (light.degree[0] + light.degree[1] * distFragLight + light.degree[2] * distFragLight * distFragLight);
	float theta		    = dot(lightDirFrag, light.direction);		// The closer to 1, the more direct the light gets to fragment.
	float epsilon	    = light.cutOff[0] - light.cutOff[1];		// Cutoff range
	float intensity     = clamp((theta - light.cutOff[1]) / epsilon, 0.0, 1.0);
		
    // ----- Ambient lighting -----
    vec3 ambient  = light.ambient * albedo * attenuation;
	if(dot(lightDirFrag, normal) > 0) return ambient;				// If light comes from below the tangent plane

    // ----- Diffuse lighting -----
	float diff    = max(dot(normal, -lightDirFrag), 0.f);
    vec3 diffuse  = light.diffuse * albedo * diff * attenuation * intensity;

    // ----- Specular lighting -----
	float spec    = pow(max(dot(normal, halfwayDir), 0.0), roughness * 4);
	vec3 specular = light.specular * specularity * spec * attenuation * intensity;
	
    // ----- Result -----
    return vec3(ambient + diffuse + specular);
}

// Apply the lights to a fragment
vec4 getFragColor(vec3 albedo, vec3 normal, vec3 specularity, float roughness, Light inLight[NUMLIGHTS], vec3 fragPos, vec3 camPos)
{
	//albedo      = applyLinearFog(albedo, vec3(.1,.1,.1), 100, 500);
	//specularity = applyLinearFog(specularity, vec3(0,0,0), 100, 500);
	//roughness   = applyLinearFog(roughness, 0, 100, 500);
	
	vec3 fragDir   = normalize(camPos - fragPos);
	vec4 result = vec4(0,0,0,1);

	for(int i = 0; i < NUMLIGHTS; i++)		// for each light source
	{
		if(inLight[i].type < 0.1) continue;
		else if(inLight[i].type < 1.1) result.xyz += directionalLightColor(albedo, normal, specularity, roughness, light[i], fragDir);
		else if(inLight[i].type < 2.1) result.xyz += PointLightColor      (albedo, normal, specularity, roughness, light[i], fragDir, fragPos);
		else if(inLight[i].type < 3.1) result.xyz += SpotLightColor       (albedo, normal, specularity, roughness, light[i], fragDir, fragPos);
	}
	
	return result;
	/*
	for(int i = 0; i < numLights; i++)		// for each light source
	{
		switch(light[i].type)
		{
		case 1:
			result.xyz += vec3(0,1,0);//directionalLightColor	(albedo, normal, specularity, roughness, light[i], fragDir);
			break;
		case 2:
			result.xyz += vec3(1,0,0);//PointLightColor		(albedo, normal, specularity, roughness, light[i], fragDir, fragPos);
			break;
		case 3:
			result.xyz += vec3(0,0,1);//SpotLightColor		(albedo, normal, specularity, roughness, light[i], fragDir, fragPos);
			break;
		default:
			break;
		}
	}
	
	return result;
	*/
}

// Apply the lights to a fragment
/*
vec3 getFragColor_Original(vec3 albedo, vec3 normal, vec3 specularity, float roughness)
{
	//albedo      = applyLinearFog(albedo, vec3(.1,.1,.1), 100, 500);
	//specularity = applyLinearFog(specularity, vec3(0,0,0), 100, 500);
	//roughness   = applyLinearFog(roughness, 0, 100, 500);

	vec3 result = vec3(0,0,0);

	for(int i = 0; i < NUMLIGHTS; i++)		// for each light source
	{
		switch(light[i].type)
		{
		case 1:
			result += directionalLightColor	(i, albedo, normal, specularity, roughness);
			break;
		case 2:
			result += PointLightColor		(i, albedo, normal, specularity, roughness);
			break;
		case 3:
			result += SpotLightColor		(i, albedo, normal, specularity, roughness);
			break;
		default:
			break;
		}
	}
	
	return result;
}
*/

// Get axis towards a direction vector is closer.
vec3 getMajorAxis(vec3 dir)
{
	vec3 mAxis = vec3(abs(dir.x), abs(dir.y), abs(dir.z));
	
	if(mAxis.x > mAxis.y) 
	{
		if(mAxis.x > mAxis.z) return normalize(vec3(dir.x, 0, 0));
		else return normalize(vec3(0, 0, dir.z));
	}
	else if(mAxis.y > mAxis.z) return normalize(vec3(0, dir.y, 0));
	else return normalize(vec3(0, 0, dir.z));
}

// Get axis towards a reflected direction vector is closer.
//vec3 getMajorAxis(vec3 fragPos, vec3 camPos, vec3 normal)
//{
//	vec3 I = normalize(fragPos - camPos);
//	vec3 R = reflect(I, normal);
//	return getMajorAxis(R);
//}

// Get UV coordinates from str coordinates (s, t, r).
vec2 getUVsFromCube(vec3 str)
{
	return vec2(
		0.5 + 0.5 * str.x / abs(str.z), 
		0.5 + 0.5 * str.y / abs(str.z)
	);
}

// Get color from a color texture
vec4 getColor(sampler2D colorMap, vec2 uv, float scale)
{
	return texture(colorMap, unpackUV(uv, scale));
}

// Get data from a texture
vec4 getData(sampler2D dataMap, vec2 uv, float scale)
{
	return toSRGB(texture(dataMap, unpackUV(uv, scale)));
}

// Return 1 if value is negative
float isNegative(float value) { return -min(sign(value), 0); }

// Return 1 if value is positive
float isPositive(float value) { return max(sign(value), 0); }

float isZero(float value) { return clamp(-2*abs(sign(value))+1, 0, 1); }

// Heaviside function is H(x)=0 when x<0, and H(x)=1 when x>=0;
float heaviside(float x) { return 0.5 * (1 + (x / (abs(x) + 0.01))); }

// X Inverted Heaviside function is H(x)=0 when x>0, and H(x)=1 when x<=0;
float invHeaviside(float x) { return abs(heaviside(x) - 1); }

// Modified Heaviside function is H(x)=0 when x<a, and H(x)=1 when x>=a;
float modHeaviside(float x, float a) { return heaviside(x - a); }

// Inverted modified Heaviside function is H(x)=0 when x<a, and H(x)=1 when x>=a;
float modifiedHeavisideFunction(float x, float a) { return abs(heaviside(x - a) - 1.f); }

// Get final world space normal from a normal map.
vec3 planarNormal(sampler2D normalMap, vec3 baseNormal, TB tb, vec2 uv, float factor)
{
	vec3 tangentSpaceNormal = unpackNormal(texture(normalMap, unpackUV(uv, factor)).xyz);
	mat3 TBNmatrix = mat3(normalize(tb.tan), normalize(tb.bTan), normalize(baseNormal));
	return normalize(TBNmatrix * tangentSpaceNormal); // = world space normal
}

// Use a vector (fragment's position in a cube) to sample from a cubemap
vec4 cubemapTex(vec3 pos, sampler2D colorMap)
{
	vec3 dir = normalize(pos);
	dir.xy = normalize(dir.xy);
	
	vec2 geoCoords = vec2( 
		-asin(dir.y) / PI2,
		(asin(dir.z) + PI05) / PI );
	
	if(dir.x < 0) geoCoords.x = -geoCoords.x + 0.5;
	
	return getColor(colorMap, geoCoords, 1);
}

// Use a vector (fragment's position in a cube) to sample from a cubemap
vec4 cubemapTex(vec3 pos, sampler2D front, sampler2D back, sampler2D up, sampler2D down, sampler2D right, sampler2D left)
{
	pos = normalize(pos);
	vec3 majorAxis = getMajorAxis(pos);
	
	vec2 uv;
	
	if(majorAxis.x != 0.f)
	{
		if(majorAxis.x > 0.f) {
			uv = getUVsFromCube(vec3(-pos.z, -pos.y, pos.x));
			return texture(front, uv);
			}
		else {
			uv = getUVsFromCube(vec3(pos.z, -pos.y, pos.x));
			return texture(back, uv);
			}
	}
	else if(majorAxis.y != 0.f)
	{
		if(majorAxis.y > 0.f) {
			uv = getUVsFromCube(vec3(pos.x, pos.z, pos.y));
			return texture(left, uv);
			}
		else {
			uv = getUVsFromCube(vec3(pos.x, -pos.z, pos.y));
			return texture(right, uv);
			}
	}
	else if(majorAxis.z != 0.f)
	{
		if(majorAxis.z > 0.f) {
			uv = getUVsFromCube(vec3(pos.x, -pos.y, pos.z));
			return texture(up, uv);
			}
		else {
			uv = getUVsFromCube(vec3(-pos.x, -pos.y, pos.z));
			return texture(down, uv);
			}
	}
	
	return vec4(0,0,0,1);
}

// (EXAMPLE) Get fragment color given 4 texture maps (albedo, normal, specular, roughness).
/*
vec3 getTex(sampler2D albedo, sampler2D normal, sampler2D specular, sampler2D roughness, float scale, vec2 UV)
{
	return getFragColor(
				texture(albedo, unpackUV(UV, scale)).rgb,
				unpackNormal(texture(normal, unpackUV(UV, scale)).rgb),
				texture(specular, unpackUV(UV, scale)).rgb, 
				texture(roughness, unpackUV(UV, scale)).r * 255 );
}
*/

// Triplanar functions ------------------------------------------------------------------------

// Texture projected from 3 axes (x,y,z) and mixed.
vec4 triplanarTexture(sampler2D tex, float texFactor, vec3 fragPos, vec3 normal)
{
	vec4 dx = texture(tex, unpackUV(fragPos.zy, texFactor));
	vec4 dy = texture(tex, unpackUV(fragPos.xz, texFactor));
	vec4 dz = texture(tex, unpackUV(fragPos.xy, texFactor));
	
	vec3 weights = abs(normal);
	weights *= weights;
	weights = weights / (weights.x + weights.y + weights.z);

	return dx * weights.x + dy * weights.y + dz * weights.z;
}

// Non-coloring texture projected from 3 axes (x, y, z) and mixed. 
vec4 triplanarNoColor(sampler2D tex, float texFactor, vec3 fragPos, vec3 normal)
{
	vec4 dx = toSRGB(texture(tex, unpackUV(fragPos.zy, texFactor)));	// Color space correction (textures are converted from sRGB to RGB, but non-coloring textures are in RGB already).
	vec4 dy = toSRGB(texture(tex, unpackUV(fragPos.xz, texFactor)));
	vec4 dz = toSRGB(texture(tex, unpackUV(fragPos.xy, texFactor)));
	
	vec3 weights = abs(normal);
	weights *= weights;
	weights = weights / (weights.x + weights.y + weights.z);

	return dx * weights.x + dy * weights.y + dz * weights.z;
}

// Normal map projected from 3 axes (x,y,z) and mixed.
// https://bgolus.medium.com/normal-mapping-for-a-triplanar-shader-10bf39dca05a
vec3 triplanarNormal(sampler2D tex, float texFactor, vec3 fragPos, vec3 normal, TB3 tb3)
{
	// Tangent space normal maps (retrieved using triplanar UVs; i.e., 3 facing planes)
	vec3 tnormalX = unpackNormal(texture(tex, unpackUV(fragPos.zy, texFactor)).xyz);
	vec3 tnormalY = unpackNormal(texture(tex, unpackUV(fragPos.xz, texFactor)).xyz);
	vec3 tnormalZ = unpackNormal(texture(tex, unpackUV(fragPos.xy, texFactor)).xyz);
	
	// Fix X plane projection over positive X axis
	vec3 axis = sign(normal);// <<< SHOULD THIS BE DONE FOR normal OR THE normal map?
	tnormalX.x *= -axis.x;	// <<< DO FOR ALL PROJECTION, NOT ONLY X
	
	// World space normals
	tnormalX = mat3(tb3.tanX, tb3.bTanX, normal) * tnormalX;	// TBN_X * tnormalX
	tnormalY = mat3(tb3.tanY, tb3.bTanY, normal) * tnormalY;	// TBN_Y * tnormalY
	tnormalZ = mat3(tb3.tanZ, tb3.bTanZ, normal) * tnormalZ;	// TBN_Z * tnormalZ
	
	// Weighted average
	vec3 weights = abs(normal);
	weights *= weights;
	weights /= weights.x + weights.y + weights.z;
	
	return normalize(tnormalX * weights.x  +  tnormalY * weights.y  +  tnormalZ * weights.z);
}

// Dynamic texture map projected from 3 axes (x,y,z) and mixed. 
vec4 triplanarTexture_Sea(sampler2D tex, float texFactor, float speed, float t, vec3 fragPos, vec3 normal)
{	
	// Get normal map coordinates
	float time = t * speed;
	float offset = texFactor / 4;
	const int n = 3;
	
	vec2 coordsXY[n] = { 
		vec2(       fragPos.x, time + fragPos.y + offset),
		vec2(time + fragPos.x,        fragPos.y - offset),
		vec2(time * SR05 + fragPos.x, time * SR05 + fragPos.y) };

	vec2 coordsZY[n] = {
		vec2(       fragPos.z, time + fragPos.y + offset),
		vec2(time + fragPos.z,        fragPos.y - offset),
		vec2(time * SR05 + fragPos.z, time * SR05 + fragPos.y) };
		
	vec2 coordsXZ[n] = {
		vec2(       fragPos.x, time + fragPos.z + offset),
		vec2(time + fragPos.x,        fragPos.z - offset),
		vec2(time * SR05 + fragPos.x, time * SR05 + fragPos.z) };
		
	// Tangent space normal maps (retrieved using triplanar UVs; i.e., 3 facing planes)
	vec4 dx = {0,0,0,0};
	vec4 dy = {0,0,0,0};
	vec4 dz = {0,0,0,0};
	int i = 0;
		
	for(i = 0; i < n; i++) 
		dx += texture(tex, unpackUV(coordsZY[i], texFactor));	
	dx = normalize(dx);

	for(i = 0; i < n; i++) 
		dy += texture(tex, unpackUV(coordsXZ[i], texFactor));
	dy = normalize(dy);
		
	for(i = 0; i < n; i++)
		dz += texture(tex, unpackUV(coordsXY[i], texFactor));
	dz = normalize(dz);
	
	//vec4 dx = texture(tex, unpackUV(fragPos.zy, texFactor));
	//vec4 dy = texture(tex, unpackUV(fragPos.xz, texFactor));
	//vec4 dz = texture(tex, unpackUV(fragPos.xy, texFactor));
	
	// Weighted average
	vec3 weights = abs(normal);
	weights *= weights;
	weights = weights / (weights.x + weights.y + weights.z);

	return dx * weights.x + dy * weights.y + dz * weights.z;
}

// Dynamic texture map projected from 3 axes (x,y,z) and mixed. 
vec4 triplanarNoColor_Sea(sampler2D tex, float texFactor, float speed, float t, vec3 fragPos, vec3 normal)
{	
	// Get normal map coordinates
	float time = t * speed;
	float offset = texFactor / 4;
	const int n = 3;
	
	vec2 coordsXY[n] = { 
		vec2(       fragPos.x, time + fragPos.y + offset),
		vec2(time + fragPos.x,        fragPos.y - offset),
		vec2(time * SR05 + fragPos.x, time * SR05 + fragPos.y) };

	vec2 coordsZY[n] = {
		vec2(       fragPos.z, time + fragPos.y + offset),
		vec2(time + fragPos.z,        fragPos.y - offset),
		vec2(time * SR05 + fragPos.z, time * SR05 + fragPos.y) };
		
	vec2 coordsXZ[n] = {
		vec2(       fragPos.x, time + fragPos.z + offset),
		vec2(time + fragPos.x,        fragPos.z - offset),
		vec2(time * SR05 + fragPos.x, time * SR05 + fragPos.z) };
		
	// Tangent space normal maps (retrieved using triplanar UVs; i.e., 3 facing planes)
	vec4 dx = {0,0,0,0};
	vec4 dy = {0,0,0,0};
	vec4 dz = {0,0,0,0};
	vec4 temp;
	int i = 0;
		
	for(i = 0; i < n; i++) 
		dx += toSRGB(texture(tex, unpackUV(coordsZY[i], texFactor)));		// Color space correction (textures are converted from sRGB to RGB, but non-coloring textures are in RGB already).
	dx = normalize(dx);

	for(i = 0; i < n; i++) 
		dy += toSRGB(texture(tex, unpackUV(coordsXZ[i], texFactor)));
	dy = normalize(dy);
		
	for(i = 0; i < n; i++)
		dz += toSRGB(texture(tex, unpackUV(coordsXY[i], texFactor)));
	dz = normalize(dz);
	
	//vec4 dx = texture(tex, unpackUV(fragPos.zy, texFactor));
	//vec4 dy = texture(tex, unpackUV(fragPos.xz, texFactor));
	//vec4 dz = texture(tex, unpackUV(fragPos.xy, texFactor));
	
	// Weighted average
	vec3 weights = abs(normal);
	weights *= weights;
	weights = weights / (weights.x + weights.y + weights.z);

	return dx * weights.x + dy * weights.y + dz * weights.z;
}

// Dynamic normal map projected from 3 axes (x,y,z) and mixed. 
vec3 triplanarNormal_Sea(sampler2D tex, float texFactor, float speed, float t, vec3 fragPos, vec3 normal, TB3 tb3)
{	
	// Get normal map coordinates
	float time = t * speed;
	float offset = texFactor / 4;
	const int n = 3;
	
	vec2 coordsXY[n] = { 
		vec2(       fragPos.x, time + fragPos.y + offset),
		vec2(time + fragPos.x,        fragPos.y - offset),
		vec2(time * SR05 + fragPos.x, time * SR05 + fragPos.y) };

	vec2 coordsZY[n] = {
		vec2(       fragPos.z, time + fragPos.y + offset),
		vec2(time + fragPos.z,        fragPos.y - offset),
		vec2(time * SR05 + fragPos.z, time * SR05 + fragPos.y) };
		
	vec2 coordsXZ[n] = {
		vec2(       fragPos.x, time + fragPos.z + offset),
		vec2(time + fragPos.x,        fragPos.z - offset),
		vec2(time * SR05 + fragPos.x, time * SR05 + fragPos.z) };
		
	// Tangent space normal maps (retrieved using triplanar UVs; i.e., 3 facing planes)
	vec3 tnormalX = {0,0,0};
	vec3 tnormalY = {0,0,0};
	vec3 tnormalZ = {0,0,0};
	int i = 0;
		
	for(i = 0; i < n; i++) 
		tnormalX += unpackNormal(texture(tex, unpackUV(coordsZY[i], texFactor)).xyz, 7);
	tnormalX = normalize(tnormalX);

	for(i = 0; i < n; i++) 
		tnormalY += unpackNormal(texture(tex, unpackUV(coordsXZ[i], texFactor)).xyz, 7);
	tnormalY = normalize(tnormalY);
		
	for(i = 0; i < n; i++)
		tnormalZ += unpackNormal(texture(tex, unpackUV(coordsXY[i], texFactor)).xyz, 7);
	tnormalZ = normalize(tnormalZ);
	
	// Tangent space normal maps (retrieved using triplanar UVs; i.e., 3 facing planes)
	//vec3 tnormalX = unpackNormal(texture(tex, unpackUV(inPos.zy, texFactor)).xyz);
	//vec3 tnormalY = unpackNormal(texture(tex, unpackUV(inPos.xz, texFactor)).xyz);
	//vec3 tnormalZ = unpackNormal(texture(tex, unpackUV(inPos.xy, texFactor)).xyz);
	
	// Fix X plane projection over positive X axis
	vec3 axis = sign(normal);
	tnormalX.x *= -axis.x;	
	
	// World space normals
	tnormalX = mat3(tb3.tanX, tb3.bTanX, normal) * tnormalX;	// TBN_X * tnormalX
	tnormalY = mat3(tb3.tanY, tb3.bTanY, normal) * tnormalY;	// TBN_Y * tnormalY
	tnormalZ = mat3(tb3.tanZ, tb3.bTanZ, normal) * tnormalZ;	// TBN_Z * tnormalZ
	
	// Weighted average
	vec3 weights = abs(normal);
	weights *= weights;
	weights /= weights.x + weights.y + weights.z;
	
	return normalize(tnormalX * weights.x  +  tnormalY * weights.y  +  tnormalZ * weights.z);
}

// (EXAMPLE) Get triplanar fragment color given 4 texture maps (albedo, normal, specular, roughness).
/*
vec3 getTriTex(sampler2D albedo, sampler2D normal, sampler2D specular, sampler2D roughness, float scale, vec2 UV, vec3 fragPos, vec3 inNormal, TB3 tb3)
{
	return getFragColor(
				triplanarTexture(albedo, scale, fragPos, inNormal).rgb,
				triplanarNormal (normal, scale, fragPos, inNormal, tb3),
				triplanarNoColor(specular, scale, fragPos, inNormal).rgb,
				triplanarNoColor(roughness, scale, fragPos, inNormal).r * 255 );
}
*/


vec3 getInversion(vec3 fragColor) { return vec3(1.f - fragColor); }

vec3 getGreyScale(vec3 fragColor) 
{ 
	float greyColor = 0.2126 * fragColor.r + 0.7152 * fragColor.g + 0.0722 * fragColor.b;
	return vec3(greyColor, greyColor, greyColor); 
}

// Kernels:
const float sharpen[9] = { -1, -1, -1, -1,  9, -1, -1, -1, -1 };
const float blur[9] = { 1./16, 2./16, 1./16, 2./16, 4./16, 2./16, 1./16, 2./16, 1./16 };
const float edgeDetection[9] = { 1, 1, 1, 1,-8, 1, 1, 1, 1 };

const float offset = 1.f / 1500.f;
const vec2 kernelOffsets[9] = vec2[](
	vec2(-offset,  offset), 	// top-left
	vec2( 0.f,     offset), 	// top-center
	vec2( offset,  offset), 	// top-right
	vec2(-offset,  0.f   ), 	// center-left
	vec2( 0.f,     0.f   ), 	// center-center
	vec2( offset,  0.f   ), 	// center-right
	vec2(-offset, -offset), 	// bottom-left
	vec2( 0.f,    -offset), 	// bottom-center
	vec2( offset, -offset) 		// bottom-right
);

vec3 applyKernel(float kernel[9], sampler2D texSampler, vec2 uv)
{
	// Use examples:  applyKernel(sharpen);  applyKernel(blur);  applyKernel(edgeDetection);
	
	vec3 colors[9];
	for(int i = 0; i < 9; i++)
		colors[i] = vec3(texture(texSampler, uv + kernelOffsets[i]).rgb);
	
	vec3 finalColor = vec3(0.f);
	for(int i = 0; i < 9; i++) 
		finalColor += colors[i] * kernel[i];
	
	return finalColor;
}


// Get gradients
uvGradient getGradients(vec2 uvs)
{
	uvGradient result;
	result.uv = uvs;
	result.dFdx = dFdx(uvs);
	result.dFdy = dFdy(uvs);
	return result;
}

// Triplanar texture using gradients
vec4 triplanarTexture(sampler2D tex, uvGradient dzy, uvGradient dxz, uvGradient dxy, vec3 normal)
{
	vec4 dx = textureGrad(tex, dzy.uv, dzy.dFdx, dzy.dFdy);
	vec4 dy = textureGrad(tex, dxz.uv, dxz.dFdx, dxz.dFdy);
	vec4 dz = textureGrad(tex, dxy.uv, dxy.dFdx, dxy.dFdy);
	
	vec3 weights = abs(normal);
	weights *= weights;
	weights = weights / (weights.x + weights.y + weights.z);

	return dx * weights.x + dy * weights.y + dz * weights.z;
}

// Triplanar normals using gradients
vec3 triplanarNormal(sampler2D tex, uvGradient dzy, uvGradient dxz, uvGradient dxy, vec3 normal, TB3 tb3)
{	
	// Tangent space normal maps (retrieved using triplanar UVs; i.e., 3 facing planes)
	vec3 tnormalX = unpackNormal(texture(tex, dzy.uv).xyz);
	vec3 tnormalY = unpackNormal(texture(tex, dxz.uv).xyz);
	vec3 tnormalZ = unpackNormal(texture(tex, dxy.uv).xyz);
	
	// Fix X plane projection over positive X axis
	vec3 axis = sign(normal);
	tnormalX.x *= -axis.x;	
	
	// World space normals
	tnormalX = mat3(tb3.tanX, tb3.bTanX, normal) * tnormalX;	// TBN_X * tnormalX
	tnormalY = mat3(tb3.tanY, tb3.bTanY, normal) * tnormalY;	// TBN_Y * tnormalY
	tnormalZ = mat3(tb3.tanZ, tb3.bTanZ, normal) * tnormalZ;	// TBN_Z * tnormalZ
	
	// Weighted average
	vec3 weights = abs(normal);
	weights *= weights;
	weights /= weights.x + weights.y + weights.z;
		
	return normalize(tnormalX * weights.x  +  tnormalY * weights.y  +  tnormalZ * weights.z);
}

// Others ------------------------------------------------------------------------

// Get ratio (return value) of 2 different texture resolutions (texFactor1, texFactor2). Used for getting textures of different resolutions depending upon distance to fragment, and for mixing them.
float getTexScaling(float fragDist, float initialTexFactor, float baseDist, float mixRange, inout float texFactor1, inout float texFactor2)
{
	// Compute current and next step
	float linearStep = 1 + floor(fragDist / baseDist);	// Linear step [1, inf)
	float quadraticStep = ceil(log(linearStep) / log(2));
	float step[2];
	step[0] = pow (2, quadraticStep);					// Exponential step [0, inf)
	step[1] = pow(2, quadraticStep + 1);				// Next exponential step
	
	// Get texture resolution for each section
	texFactor1 = step[0] * initialTexFactor;
	texFactor2 = step[1] * initialTexFactor;
	
	// Get mixing ratio
	float maxDist = baseDist * step[0];
	mixRange = mixRange * maxDist;						// mixRange is now an absolute value (not percentage)
	return clamp((maxDist - fragDist) / mixRange, 0.f, 1.f);
}

// Get a distance that increases with the distance between cam and center of a sphere. Useful for getting a distance from which non-visible things can be poorly rendered.
float getLowResDist(float camSqrHeight, float radius, float minDist)
{
	float lowResDist = sqrt(camSqrHeight - radius * radius);
	
	if(lowResDist > minDist) return lowResDist;
	else return minDist;
}

// Apply a fogColor to the fragment depending upon distance.
vec3 applyParabolicFog(vec3 fragColor, vec3 fogColor, float minDist, float maxDist, vec3 fragPos, vec3 camPos)
{
	float minSqrRadius = minDist * minDist;
	float maxSqrRadius = maxDist * maxDist;
	vec3 diff = fragPos - camPos;
	float sqrDist  = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;

	float ratio = getRatio(sqrDist, minSqrRadius, maxSqrRadius);
	//float attenuation = 1.0 / (coeff[0] + coeff[1] * sqrDist + coeff[2] * sqrDist * sqrDist);
		
	return fragColor * (1-ratio) + fogColor * ratio;
	//return fragColor * attenuation + fogColor * (1. - attenuation);
}

float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

const mat4 thresholdMap = {		// for dithering
	{0.,     8/16.,  2/16.,  10/16.}, 
	{12/16., 4/16.,  14/16., 6/16.}, 
	{3/16.,  11/16., 1/16.,  9/16.}, 
	{15/16., 7/16.,  13/16., 5/16.}
};

bool applyOrderedDithering(float value, float minValue, float maxValue)
{
	float ratio = getRatio(value, minValue, maxValue);
	ivec2 index = { int(mod(gl_FragCoord.x, 4)), int(mod(gl_FragCoord.y, 4)) };
	
	if(ratio > thresholdMap[index.x][index.y]) return true;
	return false;
}

vec3 getDryColor(vec3 color, float minHeight, float maxHeight, float groundHeight)
{
	vec3 increment = vec3(1, 1, 1) - color; 
	float ratio = 1 - clamp((groundHeight - minHeight) / (maxHeight - minHeight), 0, 1);
	return color + increment * ratio;
}
