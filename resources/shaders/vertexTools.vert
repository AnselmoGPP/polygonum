
// Index ------------------------------------------------------------------------
/*
	Constants;
		NUMLIGHTS
		PI
		E
		SR05
	Data structures:
		LightPD
		TB
		TB3
	Graphics:
		fixedPos
		getTB
		getTB3
	Math:
		getDist
		getSqrDist
		getLength
		getRatio
		getScaleRatio
		getDirection
		getAngle
		getModulus
		geo2cart
		cart2geo
		rotationMatrix2
		rotationMatrix
		rotMatrix_X
		rotMatrix_Y
		rotMatrix_Z
		rotationMatrix
		rotationMatrix
*/


// Constants ------------------------------------

#define NUMLIGHTS 3
#define PI 3.141592653589793238462
#define E  2.718281828459045235360
#define SR05 0.707106781	// == sqrt(0.5)     (vec2(SR05, SR05) has module == 1)


// Data structures ------------------------------------

// Tangent (T) and Bitangent (B)
struct TB
{
	vec3 tan;		// U direction in bump map
	vec3 bTan;		// V direction in bump map
	//vec3 norm;	// Z direction in tangent space
};

// Tangent (T) and Bitangent (B) of each dimension (3) (for triplanar mapping)
struct TB3
{
	vec3 tanX;
	vec3 bTanX;
	vec3 tanY;
	vec3 bTanY;
	vec3 tanZ;
	vec3 bTanZ;
};

// Instance data.
struct InstanceData
{
	mat4 model;
	mat4 normalMat;
};


// Graphic functions ------------------------------------------------------------------------

// (UNOPTIMIZED) Fix existing gaps between chunks
vec3 fixedPos(vec3 pos, vec3 gapFix, vec4 sideDepthDiff) 
{
	float vertexType = gapFix[0];
	
	if(vertexType < 0.1f) return pos;		// Interior vertex
	else if(vertexType < 1.1f)				// Right
	{
		if(sideDepthDiff[0] < 0.1) return pos;
		if(sideDepthDiff[0] < 1.1) return pos + normalize(pos) * gapFix[1];
		else					   return pos + normalize(pos) * gapFix[2];
	}
	else if(vertexType < 2.1f)				// Left
	{
		if(sideDepthDiff[1] < 0.1) return pos;
		if(sideDepthDiff[1] < 1.1) return pos + normalize(pos) * gapFix[1];
		else					   return pos + normalize(pos) * gapFix[2];
	}
	else if(vertexType < 3.1f)				// Up
	{
		if(sideDepthDiff[2] < 0.1) return pos;
		if(sideDepthDiff[2] < 1.1) return pos + normalize(pos) * gapFix[1];
		else					   return pos + normalize(pos) * gapFix[2];
	}
	else									// Down
	{
		if(sideDepthDiff[3] < 0.1) return pos;
		if(sideDepthDiff[3] < 1.1) return pos + normalize(pos) * gapFix[1];
		else					   return pos + normalize(pos) * gapFix[2];
	}
}

// TBN matrices (Bitangent, Tangent, Normal)
TB getTB(vec3 normal, vec3 tangent)
{
	TB tb;
	
	tb.tan = tangent;
	tb.bTan = normalize(cross(normal, tangent));
	
	return tb;
}

// TBN matrices for triplanar shader (Bitangent, Tangent, Normal) (for triplanar mapping) <<< Maybe I can reduce X & Z plane projections into a single one
TB3 getTB3(vec3 normal)
{
	vec3 axis = sign(normal);	// sign() extracts sign of the parameter
	if(axis.x == 0) axis.x = 1;
	if(axis.y == 0) axis.y = 1;
	if(axis.z == 0) axis.z = 1;	// Avoid dark patches. Disadvantage: creates contrast.
	
	TB3 tb;
	
	tb.tanX = normalize(cross(normal, vec3(0., axis.x, 0.)));		// z,y
	tb.bTanX = normalize(cross(tb.tanX, normal)) * axis.x;
	
	tb.tanY = normalize(cross(normal, vec3(0., 0., axis.y)));		// x,z
	tb.bTanY = normalize(cross(tb.tanY, normal)) * axis.y;
	
	tb.tanZ = normalize(cross(normal, vec3(0., -axis.z, 0.)));		// x,y
	tb.bTanZ = normalize(-cross(tb.tanZ, normal)) * axis.z;
	
	return tb;
}


// Math functions ------------------------------------------------------------------------

// Get distance between two 3D points
float getDist(vec3 a, vec3 b) 
{
	vec3 diff = a - b;
	return sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z) ; 
}

// Get square distance between two 3D points
float getSqrDist(vec3 a, vec3 b) 
{
	vec3 diff = a - b;
	return diff.x * diff.x + diff.y * diff.y + diff.z * diff.z; 
}

float getLength(vec3 a)
{
	return sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
}

// Get the ratio of a given "value" within a range [min, max]. Result's range: [0, 1].
float getRatio(float value, float min, float max)
{
	return clamp((value - min) / (max - min), 0, 1);
}

// Get the ratio for a given "value" within a range [min, max]. Result's range: [minR, maxR].
float getRatio(float value, float min, float max, float minR, float maxR)
{
	float ratio = clamp((value - min) / (max - min), 0, 1);
	return minR + ratio * (maxR - minR);
}

// Get the ratio of a given a ratio within a range. Result's range: [0, 1]. 
float getScaleRatio(float ratio, float min, float max)
{
	return (max - min) * ratio + min;
}

// Get a direction given 2 points.
vec3 getDirection(vec3 origin, vec3 end)
{
	return normalize(end - origin);
}

// Get an angle given 2 directions.
float getAngle(vec3 dir_1, vec3 dir_2)
{
	return acos(dot(dir_1, dir_2));
}

// Get modulus(%) = a - (b * floor(a/b))
float getModulus(float dividend, float divider) { return dividend - (divider * floor(dividend/divider)); }

// Given geographic coords (longitude (rads), latitude(rads), height), get cartesian coords (x,y,z).
vec3 geo2cart(vec3 g, float radius)
{
	return vec3(
		(radius + g.z) * cos(g.y) * cos(g.x),		// X = (radius + h) * cos(lat) * cos(lon)
		(radius + g.z) * cos(g.y) * sin(g.x),		// Y = (radius + h) * cos(lat) * sin(lon)
		(radius + g.z) * sin(g.y));					// Z = (radius + h) * sin(lat)
}

// Given cartesian coords (x,y,z), get geographic coords (longitude (rads), latitude(rads), height).
vec3 cart2geo(vec3 c, float radius)
{
	float length = sqrt(c.x * c.x + c.y * c.y + c.z * c.z);
	
	return vec3(
		atan(c.z / sqrt(c.x * c.x + c.y * c.y)),				// lat = atan(Z/sqrt(X^2 + Y^2))
		atan(c.y / c.x),										// lon = atan(Y/X)
		sqrt(c.x * c.x + c.y * c.y + c.z * c.z) - radius);		// h   = sqrt(X^2 + Y^2 + Z^2) - radius
}

// Rotation matrix of a rotation by angle rot (rads) around axis (unit vector). Result = rotMatrix * point
mat3 rotationMatrix2(vec3 axis, float rot)
{
	float cosRot = cos(rot);
	float sinRot = sin(rot);
	
	return mat3 (
		cosRot + axis.x * axis.x * (1 - cosRot),
		axis.x * axis.y * (1 - cosRot) - axis.z * sinRot, 
		axis.x * axis.z * (1 - cosRot) + axis.y * sinRot,
		
		axis.y * axis.x * (1 - cosRot) + axis.z * sinRot, 
		cosRot + axis.y * axis.y * (1 - cos(rot)), 
		axis.y * axis.z * (1 - cosRot) - axis.x * sinRot,
		
		axis.z * axis.x * (1 - cosRot) - axis.y * sinRot, 
		axis.z * axis.y * (1 - cosRot) + axis.x * sinRot, 
		cosRot + axis.z * axis.z * (1 - cosRot)
	);
}

/// Get rotation matrix. Use it to rotate a point (result = rotMatrix * point) (http://answers.google.com/answers/threadview/id/361441.html)
mat3 rotationMatrix(vec3 rotAxis, float angle)
{
	float cosVal = cos(angle / 2);
	float sinVal = sin(angle / 2);
	
	float q0 = cosVal;
	float q1 = sinVal * rotAxis.x;
	float q2 = sinVal * rotAxis.y;
	float q3 = sinVal * rotAxis.z;

	return mat3(
		q0 * q0 + q1 * q1 - q2 * q2 - q3 * q3,  
		2 * (q1 * q2 - q0 * q3),
		2 * (q1 * q3 + q0 * q2),

		2 * (q2 * q1 + q0 * q3),
		q0 * q0 - q1 * q1 + q2 * q2 - q3 * q3,
		2 * (q2 * q3 - q0 * q1),

		2 * (q3 * q1 - q0 * q2),
		2 * (q3 * q2 + q0 * q1),
		q0 * q0 - q1 * q1 - q2 * q2 + q3 * q3
	);
}

// Rotation matrix around X axis (counterclockwise when axis points the observer). Result = rotMatrix * point
mat3 rotMatrix_X(float angle)
{
	return mat3(
		1, 0, 0,
		0, cos(angle), -sin(angle),
		0, sin(angle),  cos(angle));
}

// Rotation matrix around Y axis (counterclockwise when axis points the observer). Result = rotMatrix * point
mat3 rotMatrix_Y(float angle)
{
	return mat3(
		 cos(angle), 0, sin(angle),
		 0, 1, 0,
		-sin(angle), 0, cos(angle));
}

// Rotation matrix around Z axis (counterclockwise when axis points the observer). Result = rotMatrix * point
mat3 rotMatrix_Z(float angle)
{
	return mat3(
		cos(angle), -sin(angle), 0,
		sin(angle),  cos(angle), 0,
		0, 0, 1);
}

// Get a rotation matrix that applies 2 consecutive rotations (counterclockwise when axis points the observer). Result = rotMatrix * point
mat3 rotationMatrix(mat3 firstRot, mat3 secondRot)
{
	return secondRot * firstRot;
}

// Get a rotation matrix that applies 3 consecutive rotations (counterclockwise when axis points the observer). Result = rotMatrix * point
mat3 rotationMatrix(mat3 firstRot, mat3 secondRot, mat3 thirdRot)
{
	return thirdRot * secondRot * firstRot;
}
