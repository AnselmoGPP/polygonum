#include<iostream>
#include <chrono>
#include <thread>

#include "polygonum/toolkit.hpp"


double pi = 3.141592653589793238462;
double e  = 2.718281828459045235360;

glm::vec3 xAxis(1, 0, 0);
glm::vec3 yAxis(0, 1, 0);
glm::vec3 zAxis(0, 0, 1);
glm::vec3 zero(0, 0, 0);

glm::vec4 noRotQuat = { 1, 0, 0, 0 };

void printArgs() { std::cout << std::endl; }

float getDist(const glm::vec3& a, const glm::vec3& b)
{
	glm::vec3 vec = a - b;
	return std::sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
}

float getSqrDist(const glm::vec3& a, const glm::vec3& b)
{
	glm::vec3 vec = a - b;
	return vec.x * vec.x + vec.y * vec.y + vec.z * vec.z;
}

glm::vec3 unitVec(glm::vec3& vec) { return vec / glm::length(vec); }

glm::vec3 reflect(const glm::vec3& lightRay, const glm::vec3& normal)
{
	//normal = glm::normalize(normal);
	return lightRay - 2 * glm::dot(lightRay, normal) * normal;
}

float lerp(float a, float b, float t) { return a + (b - a) * t; }

glm::vec3 lerp(glm::vec3 a, glm::vec3 b, float t) { return a + (b - a) * t; }

float ratio(float a, float b, float c)
{
	return (c - a) / (b - a);
}

float powLinInterp(float base, float exponent)
{
	float down = std::floor(exponent);
	float up = down + 1;
	float diff = exponent - down;

	up = std::pow(base, up);
	down = std::pow(base, down);

	return down + diff * (up - down);
}

int ipow(int base, unsigned exp)
{
	if (!exp) return 1;
	int result = base;
	while (1 < exp--) result *= base;
	return result;
}

int opposite01(int val) { return (val + 1) % 2; }

uint64_t appendInt(uint64_t first, uint64_t second)
{
	return std::pow(10, (int)std::log10(second) + 1) * first + second;
}

float getSphereArea(float radius) { return 4 * 3.141592653589793238462 * radius * radius; }

glm::mat4 getModelMatrix() { return glm::mat4(1.0f); }

glm::mat4 getModelMatrix(const glm::vec3& scale, const glm::vec4& rotQuat, const glm::vec3& translation)
{
	glm::mat4 mm(1.0f);

	// Execution order: Scale > Rotation > Translation
	mm = glm::translate(mm, translation);
	mm *= getRotationMatrix(rotQuat);
	mm = glm::scale(mm, scale);

	return mm;
}

glm::mat4 getModelMatrixForNormals(const glm::mat4& modelMatrix)
{
	//return toMat3(glm::transpose(glm::inverse(modelMatrix)));
	return glm::transpose(glm::inverse(modelMatrix));
}

glm::mat4 getViewMatrix(glm::vec3& camPos, glm::vec3& front, glm::vec3& camUp)
{
	return glm::lookAt(camPos, camPos + front, camUp);
}

glm::mat4 getProjMatrix(float fovy, float aspectRatio, float nearViewPlane, float farViewPlane)
{
	glm::mat4 proj = glm::perspective(fovy, aspectRatio, nearViewPlane, farViewPlane);
	proj[1][1] *= -1;       // GLM returns the Y clip coordinate inverted.
	return proj;
}

Plane::Plane() : normal(0, 0, 1), dist(0) { }

Plane::Plane(glm::vec3 normal, float distance) : normal(normal), dist(distance) { }

float Plane::distanceToPoint(const glm::vec3& point) const { return glm::dot(normal, point) + dist; }

Point::Point() : point(0) { }

Point::Point(glm::vec3 point) : point(point) { }

bool Point::isInFrustum(const Frustum& frustumPlanes) const
{
	for (auto& plane : frustumPlanes.planes)
		if (plane.distanceToPoint(point) < 0)
			return false;   // Point is outside this plane

	return true;   // Point is inside or intersects the frustum
}

void Point::setValues(glm::vec3 point) { this->point = point; }

Sphere::Sphere() : BoundingShape(), center(0), radius(0) { }

Sphere::Sphere(glm::vec3 center, float radius) : center(center), radius(radius) { }

bool Sphere::isInFrustum(const Frustum& frustumPlanes) const
{
	for (auto& plane : frustumPlanes.planes)
		if (plane.distanceToPoint(center) < -radius)
			return false;   // Sphere is completely outside this plane

	return true;   // Sphere is inside or intersects the frustum
}

void Sphere::setValues(glm::vec3 center, float radius)
{
	this->center = center;
	this->radius = radius;
}

AABB::AABB() : BoundingShape(), min(0), max(0), isInitialized(false) { }

AABB::AABB(glm::vec3 min, glm::vec3 max)
	: BoundingShape(), min(min), max(max), isInitialized(true)
{
	// Fix possible errors
	if (min.x > max.x)
	{
		this->max.x = min.x;
		this->min.x = max.x;
	}
	if (min.y > max.y)
	{
		this->max.y = min.y;
		this->min.y = max.y;
	}
	if (min.z > max.z)
	{
		this->max.z = min.z;
		this->min.z = max.z;
	}
}

bool AABB::isInFrustum(const Frustum& frustumPlanes) const
{
	if (isInitialized == false) return false;

	glm::vec3 point;

	for (const auto& plane : frustumPlanes.planes)
	{
		point = getMostNormalAlignedCorner(plane.normal);
		if (plane.distanceToPoint(point) < 0)
			return false;   // All corners are on the negative side of a plane, so AABB is completely outside.
	}

	return true;   // At least one corner is on the positive side of every plane, so AABB is visible/partially visible.
}

void AABB::setValues(glm::vec3 min, glm::vec3 max)
{
	this->min = min;
	this->max = max;

	isInitialized = true;

	//corners[0] = min;
	//corners[1] = glm::vec3(max.x, min.y, min.z);
	//corners[2] = glm::vec3(max.x, max.y, min.z);
	//corners[3] = glm::vec3(min.x, max.y, min.z);
	//corners[4] = max;
	//corners[5] = glm::vec3(min.x, max.y, max.z);
	//corners[6] = glm::vec3(min.x, min.y, max.z);
	//corners[7] = glm::vec3(max.x, min.y, max.z);
}

glm::vec3 AABB::getMostNormalAlignedCorner(const glm::vec3& planeNormal) const
{
	return glm::vec3(
		(planeNormal.x > 0) ? max.x : min.x,
		(planeNormal.y > 0) ? max.y : min.y,
		(planeNormal.z > 0) ? max.z : min.z);
}

void AABB::updateAABB(const std::vector<float>& pos, float stride)
{
	if (isInitialized == false && pos.size())
	{
		min = glm::vec3(pos[0], pos[1], pos[2]);
		max = glm::vec3(pos[0], pos[1], pos[2]);
		isInitialized = true;
	}

	for (size_t i = 0; i < pos.size(); i += stride)
	{
		if (pos[i + 0] > max.x) max.x = pos[i + 0];
		else if (pos[i + 0] < min.x) min.x = pos[i + 0];

		if (pos[i + 1] > max.y) max.y = pos[i + 1];
		else if (pos[i + 1] < min.y) min.y = pos[i + 1];

		if (pos[i + 2] > max.z) max.z = pos[i + 2];
		else if (pos[i + 2] < min.z) min.z = pos[i + 2];
	}
}

void Frustum::setPlanes(const glm::mat4& view, const glm::mat4& proj)
{
	// Get planes in camera space
	glm::mat4 viewProj = proj * view;

	// - Right plane (M3 - M0)
	planes[0].normal = glm::vec3(viewProj[0][3] - viewProj[0][0], viewProj[1][3] - viewProj[1][0], viewProj[2][3] - viewProj[2][0]);
	planes[0].dist = viewProj[3][3] - viewProj[3][0];

	// - Left plane (M3 + M0)
	planes[1].normal = glm::vec3(viewProj[0][3] + viewProj[0][0], viewProj[1][3] + viewProj[1][0], viewProj[2][3] + viewProj[2][0]);
	planes[1].dist = viewProj[3][3] + viewProj[3][0];

	// - Top plane (M3 - M1)
	planes[2].normal = glm::vec3(viewProj[0][3] - viewProj[0][1], viewProj[1][3] - viewProj[1][1], viewProj[2][3] - viewProj[2][1]);
	planes[2].dist = viewProj[3][3] - viewProj[3][1];

	// - Bottom plane (M3 + M1)
	planes[3].normal = glm::vec3(viewProj[0][3] + viewProj[0][1], viewProj[1][3] + viewProj[1][1], viewProj[2][3] + viewProj[2][1]);
	planes[3].dist = viewProj[3][3] + viewProj[3][1];

	// - Near plane (M3 + M2)
	planes[4].normal = glm::vec3(viewProj[0][3] + viewProj[0][2], viewProj[1][3] + viewProj[1][2], viewProj[2][3] + viewProj[2][2]);
	planes[4].dist = viewProj[3][3] + viewProj[3][2];

	// - Far plane (M3 - M2)
	planes[5].normal = glm::vec3(viewProj[0][3] - viewProj[0][2], viewProj[1][3] - viewProj[1][2], viewProj[2][3] - viewProj[2][2]);
	planes[5].dist = viewProj[3][3] - viewProj[3][2];

	// Normalize the planes
	float length;
	for (auto& plane : planes)
	{
		length = glm::length(plane.normal);
		plane.normal /= length;
		plane.dist /= length;
	}
}

bool Frustum::isInFrustum(const glm::vec3& point, float distBeyond) const
{
	for (auto& plane : planes)
		if (plane.distanceToPoint(point) < -distBeyond)
			return false;   // Point is outside this plane

	return true;   // Point is inside or intersects the frustum
}

bool Frustum::isInFrustum(const AABB& aabb) const
{
	glm::vec3 point;

	for (const auto& plane : planes)
	{
		point = aabb.getMostNormalAlignedCorner(plane.normal);
		if (plane.distanceToPoint(point) < 0)
			return false;   // All corners are on the negative side of a plane, so AABB is completely outside.
	}

	return true;   // At least one corner is on the positive side of every plane, so AABB is visible/partially visible.
}

bool Frustum::isInFrustum(const Sphere& sphere) const
{
	for (auto& plane : planes)
		if (plane.distanceToPoint(sphere.center) < -sphere.radius)
			return false;   // Sphere is completely outside this plane

	return true;   // Sphere is inside or intersects the frustum
}

SqrMesh::SqrMesh(size_t sideCount, float sideLength) 
	: sideCount(sideCount), sideLength(sideLength), vertexCount(sideCount * sideCount)
{
	float step = sideLength / (sideCount - 1);
	glm::vec3 origin = { -sideLength / 2, -sideLength / 2, 0 };
	size_t pos = 0;
	
	vertices.resize(3 * sideCount * sideCount);
	for (size_t y = 0; y < sideCount; y++)
		for (size_t x = 0; x < sideCount; x++)
		{
			pos = y * sideCount * 3 + x * 3;			//  Y
			vertices[pos + 0] = origin.x + step * x;	//  7 8 9
			vertices[pos + 1] = origin.y + step * y;	//  4 5 6
			vertices[pos + 2] = 0.f;					//  1 2 3 X
		}
	
	indices.resize(6 * (sideCount - 1) * (sideCount - 1));
	size_t sqrPos[4] = { 0,0,0,0 };
	for (size_t y = 0; y < (sideCount-1); y++)
		for (size_t x = 0; x < (sideCount - 1); x++)
		{
			sqrPos[0] = y * sideCount + x;				//  (3)----(2)
			sqrPos[1] = y * sideCount + x + 1;			//   |    / |
			sqrPos[2] = (y+1) * sideCount + x + 1;		//   | /    |
			sqrPos[3] = (y+1) * sideCount + x;			//  (0)----(1)

			pos = y * (sideCount-1) * 6 + x * 6;
			indices[pos + 0] = sqrPos[0];
			indices[pos + 1] = sqrPos[2];
			indices[pos + 2] = sqrPos[3];
			indices[pos + 3] = sqrPos[0];
			indices[pos + 4] = sqrPos[1];
			indices[pos + 5] = sqrPos[2];
		}
}

float SqrMesh::sideFromRadius(float radius) { return std::sqrt(2 * std::pow(radius, 2)); }

size_t getAxis(std::vector<float>& vertexDestination, std::vector<uint16_t>& indicesDestination, float lengthFromCenter, float colorIntensity)
{
	// Vertex buffer
	vertexDestination = { 
		0, 0, 0,                  colorIntensity, 0, 0,
		lengthFromCenter, 0, 0,   colorIntensity, 0, 0,
		0, 0, 0,                  0, colorIntensity, 0,
		0, lengthFromCenter, 0,   0, colorIntensity, 0,
		0, 0, 0,                  0, 0, colorIntensity,
		0, 0, lengthFromCenter,   0, 0, colorIntensity };

	// Indices buffer
	indicesDestination = std::vector<uint16_t>{ 0, 1,  2, 3,  4, 5 };

	// Total number of vertex
	return 6;
}

size_t getLongAxis(std::vector<float>& vertexDestination, std::vector<uint16_t>& indicesDestination, float lengthFromCenter, float colorIntensity)
{
	// Vertex buffer
	vertexDestination = 
	{
		-lengthFromCenter, 0, 0,   colorIntensity, 0, 0,
		 lengthFromCenter, 0, 0,   colorIntensity, 0, 0,
		0, -lengthFromCenter, 0,   0, colorIntensity, 0,
		0,  lengthFromCenter, 0,   0, colorIntensity, 0,
		0, 0, -lengthFromCenter,   0, 0, colorIntensity,
		0, 0,  lengthFromCenter,   0, 0, colorIntensity 
	};

	// Indices buffer
	indicesDestination = std::vector<uint16_t>{ 0, 1,  2, 3,  4, 5 };

	// Total number of vertex
	return 6;
}


size_t getGrid(std::vector<float>& vertexDestination, std::vector<uint16_t>& indicesDestination, int stepSize, size_t stepsPerSide, float height, glm::vec3 color)
{
	// Vertex buffer
	int start = (stepSize * stepsPerSide) / 2;
	size_t numVertex = (stepsPerSide + 1) * 4;
	vertexDestination.resize(numVertex * 6);
	glm::vec3* vertexPtr = (glm::vec3*)vertexDestination.data();
	
	for (int i = 0; i <= stepsPerSide; i++)
	{
		*vertexPtr = glm::vec3(-start, -start + i * stepSize, height);
		vertexPtr++;
		*vertexPtr = color;
		vertexPtr++;
		*vertexPtr = glm::vec3( start, -start + i * stepSize, height);
		vertexPtr++;
		*vertexPtr = color;
		vertexPtr++;
	}

	for (int i = 0; i <= stepsPerSide; i++)
	{
		*vertexPtr = glm::vec3(-start + i * stepSize, -start, height);
		vertexPtr++;
		*vertexPtr = color;
		vertexPtr++;
		*vertexPtr = glm::vec3(-start + i * stepSize,  start, height);
		vertexPtr++;
		*vertexPtr = color;
		vertexPtr++;
	}
	
	// Indices buffer
	for (uint16_t i = 0; i < numVertex; i++)
		indicesDestination.push_back(i);

	// Total number of vertex
	return numVertex;
}

size_t getQuad(std::vector<float>& destVertex, std::vector<uint16_t>& destIndices, float vertSize, float horSize, float zValue)
{
	// XY plane
	//destVertex = {
	//	-horSize/2,  vertSize/2, zValue,     0, 0,		// top left
	//	-horSize/2, -vertSize/2, zValue,     0, 1,		// low left
	//	 horSize/2, -vertSize/2, zValue,     1, 1,		// low right
	//	 horSize/2,  vertSize/2, zValue,     1, 0 };	// top right

	// XZ plane
	//destVertex = {
	//	-horSize / 2,  vertSize / 2, zValue,     0, 0,		// top left
	//	-horSize / 2, -vertSize / 2, zValue,     0, 1,		// low left
	//	 horSize / 2, -vertSize / 2, zValue,     1, 1,		// low right
	//	 horSize / 2,  vertSize / 2, zValue,     1, 0 };	// top right

	destVertex = {
	-1,  0, -1,     0, 1,		// low left
	 1,  0, -1,     1, 1,		// low right
	 1,  0,  1,     1, 0,		// top right
	-1,  0,  1,		0, 0 };		// top left


	destIndices = std::vector<uint16_t>{ 0, 1, 3,  1, 2, 3 };

	return 4;
}

size_t getPlaneNDC(std::vector<float>& vertexDestination, std::vector<uint16_t>& indicesDestination, float vertSize, float horSize)
{
	vertexDestination = { 
		-horSize/2, -vertSize/2, 0.f,     0, 0,
		-horSize/2,  vertSize/2, 0.f,     0, 1,
		 horSize/2,  vertSize/2, 0.f,     1, 1,
		 horSize/2, -vertSize/2, 0.f,     1, 0 };

	indicesDestination = std::vector<uint16_t>{ 0, 1, 3,  1, 2, 3 };

	return 4;
}

void getScreenQuad(std::vector<float>& vertices, std::vector<uint16_t>& indices, float radius, float zValue)
{
	vertices = {
		-radius,-radius, zValue,  0, 0,
		-radius, radius, zValue,  0, 1,
		 radius, radius, zValue,  1, 1,
		 radius,-radius, zValue,  1, 0 };

	indices = std::vector<uint16_t>{ 0,1,3, 1,2,3 };
}

std::vector<float> v_YZquad = {
	0,  1, -1,	0, 1,		// low left
	0, -1, -1,	1, 1,		// low right
	0, -1,  1,	1, 0,		// top right
	0,  1,  1,	0, 0 };		// top left

std::vector<uint16_t> i_quad = { 0, 1, 3,  1, 2, 3 };

// Skybox
std::vector<float> v_skybox =
{
	//Vertices    UV coords
	-1, -1,  1,    0., 1/3., 
	-1, -1, -1,    0., 2/3., 
	-1,  1,  1,   .25, 1/3.,
	-1,  1, -1,   .25, 2/3.,
	 1,  1,  1,    .5, 1/3., 
	 1,  1, -1,    .5, 2/3., 
	 1, -1,  1,   .75, 1/3.,
	 1, -1, -1,   .75, 2/3.,
	-1, -1,  1,    1., 1/3., 
	-1, -1, -1,    1., 2/3., 
	-1, -1,  1,   .25, 0.  ,  
	 1, -1,  1,    .5, 0.  ,   
	-1, -1, -1,   .25, 1.  ,  
	 1, -1, -1,    .5, 1.      
};

std::vector<uint16_t> i_skybox = { 
	0, 1, 2,    1, 3, 2,  
	2, 3, 4,    3, 5, 4,  
	4, 5, 6,    5, 7, 6,  
	6, 7, 8,    7, 9, 8,  
	10, 2, 11,  2, 4, 11,  
	3, 12, 5,  12, 13, 5
};

std::vector<float> v_skybox2 = {
	// front
	-1,  1, -1,	0, 1,
	 1,  1, -1, 1, 1,
	 1,  1,  1, 1, 0,
	-1,  1,  1, 0, 0,
	// back
	 1, -1, -1,	0, 1,
	-1, -1, -1, 1, 1,
	-1, -1,  1, 1, 0,
	 1, -1,  1, 0, 0,
	// up
	-1,  1,  1,	0, 1,
	 1,  1,  1, 1, 1,
	 1, -1,  1, 1, 0,
	-1, -1,  1, 0, 0,
	// down
	-1, -1, -1,	0, 1,
	 1, -1, -1, 1, 1,
	 1,  1, -1, 1, 0,
	-1,  1, -1, 0, 0,
	// right
	 1,  1, -1,	0, 1,
	 1, -1, -1, 1, 1,
	 1, -1,  1, 1, 0,
	 1,  1,  1, 0, 0,
	// left
	-1, -1, -1,	0, 1,
	-1,  1, -1, 1, 1,
	-1,  1,  1, 1, 0,
	-1, -1,  1, 0, 0
};

std::vector<uint16_t> i_skybox2 = { 
	 0,  1,  2,    0,  2,  3, 
	 4,  5,  6,    4,  6,  7,
	 8,  9, 10,    8, 10, 11,
	12, 13, 14,   12, 14, 15,
	16, 17, 18,   16, 18, 19,
	20, 21, 22,   20, 22, 23
};

std::vector<float> v_cube = {
	// front
	 1,-1, 1,   1, 0, 0,   0, 1, 0,   0, 1,
	 1, 1, 1,   1, 0, 0,   0, 1, 0,   1, 1,
	 1, 1,-1,   1, 0, 0,   0, 1, 0,   1, 0,
	 1,-1,-1,   1, 0, 0,   0, 1, 0,   0, 0,
	// back
	-1, 1, 1,  -1, 0, 0,   0,-1, 0,   0, 1,
	-1,-1, 1,  -1, 0, 0,   0,-1, 0,   1, 1,
	-1,-1,-1,  -1, 0, 0,   0,-1, 0,   1, 0,
	-1, 1,-1,  -1, 0, 0,   0,-1, 0,   0, 0,
	// up
	-1, 1, 1,   0, 0, 1,   1, 0, 0,   0, 1,
	 1, 1, 1,   0, 0, 1,   1, 0, 0,   1, 1,
	 1,-1, 1,   0, 0, 1,   1, 0, 0,   1, 0,
	-1,-1, 1,   0, 0, 1,   1, 0, 0,   0, 0,
	// down
	 1, 1,-1,   0, 0,-1,  -1, 0, 0,   0, 1,
	-1, 1,-1,   0, 0,-1,  -1, 0, 0,   1, 1,
	-1,-1,-1,   0, 0,-1,  -1, 0, 0,   1, 0,
	 1,-1,-1,   0, 0,-1,  -1, 0, 0,   0, 0,
	// right
	-1,-1, 1,   0,-1, 0,   1, 0, 0,   0, 1,
	 1,-1, 1,   0,-1, 0,   1, 0, 0,   1, 1,
	 1,-1,-1,   0,-1, 0,   1, 0, 0,   1, 0,
	-1,-1,-1,   0,-1, 0,   1, 0, 0,   0, 0,
	// left
	 1, 1, 1,   0, 1, 0,  -1, 0, 0,   0, 1,
	-1, 1, 1,   0, 1, 0,  -1, 0, 0,   1, 1,
	-1, 1,-1,   0, 1, 0,  -1, 0, 0,   1, 0,
	 1, 1,-1,   0, 1, 0,  -1, 0, 0,   0, 0
};

std::vector<uint16_t> i_cube = {
	 0, 2, 1,    0, 3, 2,
	 4, 6, 5,    4, 7, 6,
	 8,10, 9,    8,11,10,
	 12,14,13,  12,15,14,
	 16,18,17,  16,19,18,
	 20,22,21,  20,23,22
};


bool ifOnce::ifBigger(float a, float b)
{
	if (std::find(checked.begin(), checked.end(), b) != checked.end())
		return false;
	else if (a > b)
	{
		checked.push_back(b); 
		return true;
	}
	else 
		return false;
}

Icosahedron::Icosahedron(float multiplier)
{
	for (size_t i = 0; i < 12; i++)
	{
		icos[i * 6 + 0] *= multiplier;
		icos[i * 6 + 1] *= multiplier;
		icos[i * 6 + 2] *= multiplier;
	}
}

float Icosahedron::vertices[12 * 3] =
{
	 0.f,       -0.525731f,  0.850651f,
	 0.850651f,  0.f,        0.525731f,
	 0.850651f,  0.f,       -0.525731f,
	-0.850651f,  0.f,       -0.525731f,
	-0.850651f,  0.f,        0.525731f,
	-0.525731f,  0.850651f,  0.f,
	 0.525731f,  0.850651f,  0.f,
	 0.525731f, -0.850651f,  0.f,
	-0.525731f, -0.850651f,  0.f,
	 0.f,       -0.525731f, -0.850651f,
	 0.f,        0.525731f, -0.850651f,
	 0.f,        0.525731f,  0.850651f
};

float Icosahedron::colors[12 * 4] =
{
	1.0, 0.0, 0.0, 1.0,
	1.0, 0.5, 0.0, 1.0,
	1.0, 1.0, 0.0, 1.0,
	0.5, 1.0, 0.0, 1.0,
	0.0, 1.0, 0.0, 1.0,
	0.0, 1.0, 0.5, 1.0,
	0.0, 1.0, 1.0, 1.0,
	0.0, 0.5, 1.0, 1.0,
	0.0, 0.0, 1.0, 1.0,
	0.5, 0.0, 1.0, 1.0,
	1.0, 0.0, 1.0, 1.0,
	1.0, 0.0, 0.5, 1.0
};

unsigned indices[20 * 3] =
{
	1,  2,  6,
	1,  7,  2,
	3,  4,  5,
	4,  3,  8,
	6,  5,  11,
	5,  6,  10,
	9,  10, 2,
	10, 9,  3,
	7,  8,  9,
	8,  7,  0,
	11, 0,  1,
	0,  11, 4,
	6,  2,  10,
	1,  6,  11,
	3,  5,  10,
	5,  4,  11,
	2,  7,  9,
	7,  1,  0,
	3,  9,  8,
	4,  8,  0
};

float normals[12 * 3] =
{
	 0.000000f, -0.417775f,  0.675974f,
	 0.675973f,  0.000000f,  0.417775f,
	 0.675973f, -0.000000f, -0.417775f,
	-0.675973f,  0.000000f, -0.417775f,
	-0.675973f, -0.000000f,  0.417775f,
	-0.417775f,  0.675974f,  0.000000f,
	 0.417775f,  0.675973f, -0.000000f,
	 0.417775f, -0.675974f,  0.000000f,
	-0.417775f, -0.675974f,  0.000000f,
	 0.000000f, -0.417775f, -0.675973f,
	 0.000000f,  0.417775f, -0.675974f,
	 0.000000f,  0.417775f,  0.675973f
};

std::vector<float> Icosahedron::icos =
{
	 0.f,       -0.525731f,  0.850651f, 1.0, 0.0, 0.0,
	 0.850651f,  0.f,        0.525731f, 1.0, 0.5, 0.0,
	 0.850651f,  0.f,       -0.525731f, 1.0, 1.0, 0.0,
	-0.850651f,  0.f,       -0.525731f, 0.5, 1.0, 0.0,
	-0.850651f,  0.f,        0.525731f, 0.0, 1.0, 0.0,
	-0.525731f,  0.850651f,  0.f,	    0.0, 1.0, 0.5,
	 0.525731f,  0.850651f,  0.f,	    0.0, 1.0, 1.0,
	 0.525731f, -0.850651f,  0.f,	    0.0, 0.5, 1.0,
	-0.525731f, -0.850651f,  0.f,	    0.0, 0.0, 1.0,
	 0.f,       -0.525731f, -0.850651f, 0.5, 0.0, 1.0,
	 0.f,        0.525731f, -0.850651f, 1.0, 0.0, 1.0,
	 0.f,        0.525731f,  0.850651f, 1.0, 0.0, 0.5
};

std::vector<float> Icosahedron::index =
{
	1,  2,  6,
	1,  7,  2,
	3,  4,  5,
	4,  3,  8,
	6,  5,  11,
	5,  6,  10,
	9,  10, 2,
	10, 9,  3,
	7,  8,  9,
	8,  7,  0,
	11, 0,  1,
	0,  11, 4,
	6,  2,  10,
	1,  6,  11,
	3,  5,  10,
	5,  4,  11,
	2,  7,  9,
	7,  1,  0,
	3,  9,  8,
	4,  8,  0
};

bool isBigEndian()
{
	int n = 1;
	if (*(char*)&n == 1) return false;
	else return true;
}

void Quicksort_distVec3::sort(std::vector<glm::vec3>::iterator low, std::vector<glm::vec3>::iterator high, const glm::vec3& camPos)
{
	this->camPos = camPos;
	this->begin = low;
	quickSort(low, high);
}

void Quicksort_distVec3::quickSort(std::vector<glm::vec3>::iterator low, std::vector<glm::vec3>::iterator high)
{
	if (low < high)		//(low >= 0 && high >= 0 && low < high)
	{
		std::vector<glm::vec3>::iterator p = partition(low, high);
		quickSort(low, p);
		quickSort(p + 1, high);
	}
}

std::vector<glm::vec3>::iterator Quicksort_distVec3::partition(std::vector<glm::vec3>::iterator low, std::vector<glm::vec3>::iterator high)
{
	glm::vec3 pivot = *(low + ((high - low) / 2));

	std::vector<glm::vec3>::iterator i = low - 1;
	std::vector<glm::vec3>::iterator j = high + 1;

	while (1)
	{
		do i++;
		while (isFurther(*i, pivot));		// (arr[i] < pivot)

		do j--;
		while (isCloser(*j, pivot));		// (arr[j] > pivot)

		if (i >= j) return j;

		swap(*i, *j);
	}
}

void Quicksort_distVec3::swap(glm::vec3& a, glm::vec3& b)
{
	temp = a;
	a = b;
	b = temp;
}

bool Quicksort_distVec3::isFurther(const glm::vec3& pos1, const glm::vec3& pos2)
{
	vec1 = pos1 - camPos;
	vec2 = pos2 - camPos;
	sqrDist1 = vec1.x * vec1.x + vec1.y * vec1.y + vec1.z * vec1.z;
	sqrDist2 = vec2.x * vec2.x + vec2.y * vec2.y + vec2.z * vec2.z;

	if (sqrDist1 > sqrDist2) return true;
	else return false;
}

bool Quicksort_distVec3::isCloser(const glm::vec3& pos1, const glm::vec3& pos2)
{
	vec1 = pos1 - camPos;
	vec2 = pos2 - camPos;
	sqrDist1 = vec1.x * vec1.x + vec1.y * vec1.y + vec1.z * vec1.z;
	sqrDist2 = vec2.x * vec2.x + vec2.y * vec2.y + vec2.z * vec2.z;

	if (sqrDist1 < sqrDist2) return true;
	else return false;
}

int64_t Quicksort_distVec3::pos(std::vector<glm::vec3>::iterator iter) { return iter - begin; }


void Quicksort_distVec3_index::sort(std::vector<glm::vec3>& positions, std::vector<int>& indexVals, const glm::vec3& camPos, int low, int high)
{
	// <<< what if high < low?

	//int size = high - low + 1;
	//indexVals.resize(size);
	//for (int i = 0; i < size; i++) indexVals[i] = i;

	this->camPos = camPos;
	this->pos = &positions;

	quickSort(indexVals, low, high);
}

void Quicksort_distVec3_index::quickSort(std::vector<int>& idx, int low, int high)
{
	if (low < high)		//(low >= 0 && high >= 0 && low < high)
	{
		int p = partition(idx, low, high);
		quickSort(idx, low, p);
		quickSort(idx, p + 1, high);
	}
}

int Quicksort_distVec3_index::partition(std::vector<int>& idx, int low, int high)
{
	int pivot = idx[low + (high - low) / 2];		// glm::vec3 pivot = *(low + ((high - low) / 2));

	int i = low - 1;
	int j = high + 1;

	while (1)
	{
		do i++;
		while (isFurther(idx[i], pivot));		// (arr[i] < pivot)

		do j--;
		while (isCloser(idx[j], pivot));		// (arr[j] > pivot)

		if (i >= j) return j;

		swap(idx[i], idx[j]);
	}
}

void Quicksort_distVec3_index::swap(int& a, int& b)
{
	temp = a;
	a = b;
	b = temp;
}

bool Quicksort_distVec3_index::isFurther(int idx1, int idx2)
{
	vec1 = (*pos)[idx1] - camPos;
	vec2 = (*pos)[idx2] - camPos;
	sqrDist1 = vec1.x * vec1.x + vec1.y * vec1.y + vec1.z * vec1.z;
	sqrDist2 = vec2.x * vec2.x + vec2.y * vec2.y + vec2.z * vec2.z;

	if (sqrDist1 > sqrDist2) return true;
	else return false;
}

bool Quicksort_distVec3_index::isCloser(int idx1, int idx2)
{
	vec1 = (*pos)[idx1] - camPos;
	vec2 = (*pos)[idx2] - camPos;
	sqrDist1 = vec1.x * vec1.x + vec1.y * vec1.y + vec1.z * vec1.z;
	sqrDist2 = vec2.x * vec2.x + vec2.y * vec2.y + vec2.z * vec2.z;

	if (sqrDist1 < sqrDist2) return true;
	else return false;
}

int safeMod(int a, int b)
{
	if (!b) return 0;	// division by 0 is not safe

	return a - (a / b) * b;
}

float safeMod(float a, float b)
{
	if (!b) return 0;	// division by 0 is not safe

	return a - ((int)a / (int)b) * (int)b;
}

glm::vec3 safeMod(const glm::vec3& a, float b)
{
	if (!b) return glm::vec3(0,0,0);	// division by 0 is not safe

	return glm::vec3(
		a.x - ((int)a.x / (int)b) * (int)b,
		a.y - ((int)a.y / (int)b) * (int)b,
		a.z - ((int)a.z / (int)b) * (int)b
	);
}

float getSlope(const glm::vec3& groundNormal, const glm::vec3& upNormal)
{
	return 1.f - glm::dot(groundNormal, upNormal);
}

void sleep(int milliseconds) { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); }

// Add some time to deltaTime to adjust the FPS (if FPS control is enabled)
void waitForFPS(Timer& timer, int maxFPS)
{
	if (maxFPS == 0) return;

	int waitTime = static_cast<int>((1.l / maxFPS - timer.getDeltaTime()) * 1000000);    // microseconds (for the sleep)
	if (waitTime > 0)
	{
		std::this_thread::sleep_for(std::chrono::microseconds(waitTime));
		timer.reUpdateTime();
	}
}

Timer::Timer()
	: deltaTime(0), totalDeltaTime(0)
{
	startTimer();
	currentTime = startTime;
	//currentTime = std::chrono::system_clock::duration::zero();
}

void Timer::startTimer()
{
	startTime = std::chrono::high_resolution_clock::now();
	prevTime = startTime;
	//std::this_thread::sleep_for(std::chrono::microseconds(1000));   // Avoids deltaTime == 0 (i.e. currentTime == lastTime)
}

long double Timer::updateTime()
{
	prevTime = currentTime;
	currentTime = std::chrono::high_resolution_clock::now();
	deltaTime = std::chrono::duration<long double, std::chrono::seconds::period>(currentTime - prevTime).count();
	totalDeltaTime = std::chrono::duration<long double, std::chrono::seconds::period>(currentTime - startTime).count();
	//   time = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - startTime).count() / 1000000.l;
	//   time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	return deltaTime;
}

long double Timer::reUpdateTime()
{
	currentTime = std::chrono::high_resolution_clock::now();
	deltaTime = std::chrono::duration<long double, std::chrono::seconds::period>(currentTime - prevTime).count();
	totalDeltaTime = std::chrono::duration<long double, std::chrono::seconds::period>(currentTime - startTime).count();

	return deltaTime;
}

long double Timer::getDeltaTime() const { return deltaTime; }

long double Timer::getTotalDeltaTime() const { return totalDeltaTime; }

std::string Timer::getDate()
{
	std::chrono::system_clock::time_point timePoint = std::chrono::system_clock::now();
	std::time_t date = std::chrono::system_clock::to_time_t(timePoint);
	return std::string(std::ctime(&date));
}

float angleBetween(glm::vec3 a, glm::vec3 b, glm::vec3 origin)
{
	// A·B = |A|*|B|*cos(θ);   θ = acos( (A·B)/(|A|*|B|) )     (|X| is the length of vector X)

	float dotP = glm::dot(glm::normalize(a - origin), glm::normalize(b - origin));
	dotP = glm::clamp(dotP, -1.f, 1.f);			// if (dotP > 1) dotP =  1; else if (dotP < -1) dotP = -1;

	return glm::acos(dotP);
}

float angleBetween(glm::vec3 a, glm::vec3 b)
{
	// A·B = |A|*|B|*cos(θ);   θ = acos( (A·B)/(|A|*|B|) )     (|X| is the length of vector X)

	float adjacent = glm::dot(glm::normalize(a), glm::normalize(b));
	adjacent = glm::clamp(adjacent, -1.f, 1.f);			// if (dotP > 1) dotP =  1; else if (dotP < -1) dotP = -1;

	return glm::acos(adjacent);
}

glm::vec4 getRotQuat(glm::vec3 rotAxis, float angle)
{
	float cosOp = cos(angle / 2);
	float sinOp = sin(angle / 2);
	return glm::vec4(cosOp, sinOp * rotAxis.x, sinOp * rotAxis.y, sinOp * rotAxis.z);
}

glm::vec3 rotatePoint(const glm::vec4& rotQuat, const glm::vec3& point)
{
	glm::vec4 inverseRotQuat(rotQuat.x, -rotQuat.y, -rotQuat.z, -rotQuat.w);
	glm::vec4 pointQuat(0.f, point);
	pointQuat = productQuat(productQuat(inverseRotQuat, pointQuat), rotQuat);	//pointQuat = inverseRotQuat * pointQuat * rotQuat;
	return glm::vec3(pointQuat.y, pointQuat.z, pointQuat.w);
}

glm::vec4 productQuat(const glm::vec4& q1, const glm::vec4& q2)
{
	return glm::vec4(	// q1 * q2
		q1[0] * q2[0] - q1[1] * q2[1] - q1[2] * q2[2] - q1[3] * q2[3],
		q1[0] * q2[1] + q1[1] * q2[0] - q1[2] * q2[3] + q1[3] * q2[2],
		q1[0] * q2[2] + q1[1] * q2[3] + q1[2] * q2[0] - q1[3] * q2[1],
		q1[0] * q2[3] - q1[1] * q2[2] + q1[2] * q2[1] + q1[3] * q2[0]
	);
}

glm::vec4 productQuat(const glm::vec4& q1, const glm::vec4& q2, const glm::vec4& q3)
{
	return productQuat(productQuat(q1, q2), q3);	// q1 * q2 * q3
}

glm::mat3 getRotationMatrix(glm::vec3 rotAxis, float angle)
{
	float q0 = cos(angle / 2);
	float q1 = sin(angle / 2) * rotAxis.x;
	float q2 = sin(angle / 2) * rotAxis.y;
	float q3 = sin(angle / 2) * rotAxis.z;

	return glm::mat3(
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

glm::mat4 getRotationMatrix(glm::vec4 quat)
{
	// Frame rotation matrix
	return glm::mat4(
		2 * (quat[0] * quat[0] + quat[1] * quat[1]) - 1,
		2 * (quat[1] * quat[2] + quat[0] * quat[3]),
		2 * (quat[1] * quat[3] - quat[0] * quat[2]),
		0,

		2 * (quat[1] * quat[2] - quat[0] * quat[3]),
		2 * (quat[0] * quat[0] + quat[2] * quat[2]) - 1,
		2 * (quat[2] * quat[3] + quat[0] * quat[1]),
		0,

		2 * (quat[1] * quat[3] + quat[0] * quat[2]),
		2 * (quat[2] * quat[3] - quat[0] * quat[1]),
		2 * (quat[0] * quat[0] + quat[3] * quat[3]) - 1,
		0,

		0, 0, 0, 1);

	// Point rotation matrix (transpose of frame rotation matrix)
	return glm::mat4(
		2 * (quat[0] * quat[0] + quat[1] * quat[1]) - 1,
		2 * (quat[1] * quat[2] - quat[0] * quat[3]),
		2 * (quat[1] * quat[3] + quat[0] * quat[2]),
		0,

		2 * (quat[1] * quat[2] + quat[0] * quat[3]),
		2 * (quat[0] * quat[0] + quat[2] * quat[2]) - 1,
		2 * (quat[2] * quat[3] - quat[0] * quat[1]),
		0,

		2 * (quat[1] * quat[3] - quat[0] * quat[2]),
		2 * (quat[2] * quat[3] + quat[0] * quat[1]),
		2 * (quat[0] * quat[0] + quat[3] * quat[3]) - 1,
		0,

		0, 0, 0, 1);
}