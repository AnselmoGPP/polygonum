#include <iostream>

#include "polygonum/physics.hpp"


// BulletPhysics ----------------------------------------------------------
/*
PhysicsWorld::PhysicsWorld()
{
	// Initiate physics
	broadphase = new btDbvtBroadphase();
	collisionConfig = new btDefaultCollisionConfiguration();
	dispatcher = new btCollisionDispatcher(collisionConfig);
	solver = new btSequentialImpulseConstraintSolver();

	world = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfig);
	world->setGravity(btVector3(0, -9.8, 0));
}

PhysicsWorld::~PhysicsWorld()
{
	delete world;
	delete solver;
	delete collisionConfig;
	delete dispatcher;
	delete broadphase;
}

PhysicsObject::PhysicsObject() { }

void PhysicsObject::createShapeWithVertices(std::vector<float> &vertices, bool convex)
{
	this->convex = convex;

	// Create shape with vertices

	if (convex)	// Convex object
	{
		shape = new btConvexHullShape();		// btConvexHullShape allows to add all object's points and use them to automatically create the minimum convex hull for it
		for (int i = 0; i < vertices.size(); i += 3)
		{
			btVector3 btv = btVector3(vertices[i + 0], vertices[i + 1], vertices[i + 2]);
			((btConvexHullShape*)shape)->addPoint(btv);
		}
	}
	else		// Concave object
	{
		btTriangleMesh* mesh = new btTriangleMesh();
		for (int i = 0; i < vertices.size(); i += 9)
		{
			btVector3 bv1 = btVector3(vertices[i + 0], vertices[i + 1], vertices[i + 2]);
			btVector3 bv2 = btVector3(vertices[i + 3], vertices[i + 4], vertices[i + 5]);
			btVector3 bv3 = btVector3(vertices[i + 6], vertices[i + 7], vertices[i + 8]);

			mesh->addTriangle(bv1, bv2, bv3);
		}
		shape = new btBvhTriangleMeshShape(mesh, true);	// btBvhTriangleMeshShape requires the creation of a mesh object consisting of triangles. In this step, you gather triangles by grouping vertices from the list of vertices. Then you create a mesh and create a shape object from this mesh.
	}

	// delete mesh
}

void PhysicsObject::createBodyWithMass(float mass)
{
	//btQuaternion rotation;
	//rotation.setEulerZYX(self.rotationZ, self.rotationY, self.rotationX);
	//
	//btVector3 position = btVector3(self.position.x, self.position.y, self.position.z);
	//btDefaultMotionState* motionState = new btDefaultMotionState(btTransform(rotation, position));
	//
	//btScalar bodyMass = mass;
	//btVector3 bodyInertia;
	//shape->calculateLocalInertia(bodyMass, bodyInertia);
	//
	//btRigidBody::btRigidBodyConstructionInfo bodyCI = btRigidBody::btRigidBodyConstructionInfo(bodyMass, motionState, shape, bodyInertia);
	//
	//bodyCI.m_restitution = 1.0f;
	//bodyCI.m_friction = 0.5f;
	//
	//body = new btRigidBody(bodyCI);
	//body->setUserPointer((__bridge void*)self);
	//body->setLinearFactor(btVector3(1, 1, 0));
}
*/

// Particle ----------------------------------------------------------

float getFHeight(const glm::vec3& pos) { return 2000; }

Particle::Particle(glm::vec3 position, glm::vec3 direction, float speed, float gValue, glm::vec3 gDirection)
	: pos(position), speedVecNP(0,0,0), speedVecP(0,0,0), gVec(gDirection * gValue), onFloor(false), getFloorHeight(getFHeight) { }

Particle::~Particle() { }

glm::vec3 Particle::getPos() { return pos; }

bool Particle::isOnFloor() { return onFloor; }

void Particle::setPos(glm::vec3 position) { pos = position; }

void Particle::setSpeedNP(glm::vec3 speedVector) { speedVecNP = speedVector; }

void Particle::setSpeedP(glm::vec3 speedVector) { speedVecP += speedVector; }

void Particle::setCallback(float(*getFloorHeight)(const glm::vec3& pos)) { this->getFloorHeight = getFloorHeight; }

void Particle::updateState(float deltaTime)
{
	// Get new position
	// UARM: Uniformly Accelerated Rectilinear Motion ( s = 0.5 g t^2 + v t + s0 ). Links: https://www.youtube.com/watch?v=9NoHru1SlwQ, https://stackoverflow.com/questions/72686481/planet-position-by-time
	glm::vec3 newPos = pos + (speedVecNP + speedVecP) * deltaTime + 0.5f * gVec * (deltaTime * deltaTime);	

	// Adjust position to ground
	float floorHeight = getFloorHeight(newPos);
	if (pos.z < floorHeight) 
	{
		pos.z = floorHeight;
		speedVecP = { 0.f, 0.f, 0.f };
		onFloor = true;
	}
	else
	{
		pos = newPos;
		speedVecP += gVec * deltaTime;
		if(pos.z < (floorHeight + 0.15f)) onFloor = true;		// Allows to jump before touching ground
		else onFloor = false;
	}
}

// ----------------------------------------------------------

PlanetParticle::PlanetParticle(glm::vec3 position, glm::vec3 direction, float speed, float gValue, glm::vec3 nucleus)
	: Particle(position, direction, speed, gValue, glm::normalize(nucleus - position)), nucleus(nucleus), g(gValue) { }

void PlanetParticle::setPos(glm::vec3 position)
{
	pos = position;
	gVec = glm::normalize(nucleus - position) * g;
}

void PlanetParticle::updateState(float deltaTime)
{
	// Get new position
	// UARM: Uniformly Accelerated Rectilinear Motion ( s = 0.5 g t^2 + v t + s0 ). Links: https://www.youtube.com/watch?v=9NoHru1SlwQ, https://stackoverflow.com/questions/72686481/planet-position-by-time
	glm::vec3 newPos = pos + (speedVecNP + speedVecP) * deltaTime + 0.5f * gVec * (deltaTime * deltaTime);

	// Adjust position to ground
	float oldHeight = glm::distance(nucleus, pos);
	float newHeight = glm::distance(nucleus, newPos);
	glm::vec3 gDir = glm::normalize(nucleus - newPos);

	gVec = glm::normalize(nucleus - newPos) * g;
	float floorHeight = getFloorHeight(newPos);

	if (newHeight < floorHeight) 
	{
		pos = -gDir * floorHeight;	// newPos - gDir * (oldHeight - newHeight);
		speedVecP = { 0.f, 0.f, 0.f };
		onFloor = true;
	}
	else
	{
		pos = newPos;
		speedVecP += gVec * deltaTime;
		onFloor = false;				// if (newHeight < (floorHeight + 0.15f)) onFloor = true; else onFloor = false;	// Allows to jump before touching ground
	}

	speedVecNP = { 0,0,0 };
	gVec = gDir * g;
}

// OpticalDepthTable ----------------------------------------------------------

/*
	getOpticalDepthTable
		operator ()
			raySphere()
			opticalDepth()
				densityAtPoint()
*/

OpticalDepthTable::OpticalDepthTable(unsigned numOptDepthPoints, unsigned planetRadius, unsigned atmosphereRadius, float heightStep, float angleStep, float densityFallOff)
	: planetCenter(0, 0, 0), planetRadius(planetRadius), atmosphereRadius(atmosphereRadius), numOptDepthPoints(numOptDepthPoints), heightStep(heightStep), angleStep(angleStep), densityFallOff(densityFallOff)
{
	// Compute useful variables	
	heightSteps = std::ceil(1 + (atmosphereRadius - planetRadius) / heightStep);	// <<<
	angleSteps = std::ceil(1 + 3.141592653589793238462 / angleStep);
	bytes = 4 * heightSteps * angleSteps;	// sizeof(float) = 4

	// Get table
	table.resize(bytes);

	float rayLength, angle;
	glm::vec3 point, rayDir;
	float* optDepth = (float*)table.data();

	for (size_t i = 0; i < heightSteps; i++)
	{
		point = { 0, planetRadius + i * heightStep, 0 };	// rayOrigin

		for (size_t j = 0; j < angleSteps; j++)
		{
			angle = j * angleStep;
			rayDir = glm::vec3(sin(angle), cos(angle), 0);
			rayLength = raySphere(point, rayDir).y;

			optDepth[i * angleSteps + j] = opticalDepth(point, rayDir, rayLength);

			//if (point.y > 2399 && point.y < 2401 && angle > 1.84 && angle < 1.86)
			//	std::cout << "(" << i << ", " << j << ") / " << point.y << " / " << optDepth[i * angleSteps + j] << " / " << rayLength << " / " << angle << " / (" << rayDir.x << ", " << rayDir.y << ", " << rayDir.z << ")" << std::endl;
		}
	}

	// Compute
	float angleRange = 3.141592653589793238462;
	point = glm::vec3(2400, 0, 0);
	angle = angleRange / 1.7;
	rayDir = glm::vec3(cos(angle), 0, sin(angle));
	rayLength = raySphere(point, rayDir).y;

	// Look up
	float heightRatio = (2400.f - planetRadius) / (atmosphereRadius - planetRadius);
	float angleRatio = angle / angleRange;
	unsigned i = (heightSteps - 1) * heightRatio;
	unsigned j = (angleSteps - 1) * angleRatio;
}

float OpticalDepthTable::opticalDepth(glm::vec3 rayOrigin, glm::vec3 rayDir, float rayLength) const
{
	glm::vec3 point = rayOrigin;
	float stepSize = rayLength / (numOptDepthPoints - 1);
	float opticalDepth = 0;

	for (unsigned i = 0; i < numOptDepthPoints; i++)
	{
		opticalDepth += densityAtPoint(point) * stepSize;
		point += rayDir * stepSize;
	}

	return opticalDepth;
}

float OpticalDepthTable::densityAtPoint(glm::vec3 point) const
{
	float heightAboveSurface = glm::length(point - planetCenter) - planetRadius;
	float height01 = heightAboveSurface / (atmosphereRadius - planetRadius);

	//return exp(-height01 * densityFallOff);					// There is always some density
	return exp(-height01 * densityFallOff) * (1 - height01);	// Density ends at some distance
}

// Returns distance to sphere surface. If it's not, return maximum floating point.
// Returns vector(distToSphere, distThroughSphere). 
//		If rayOrigin is inside sphere, distToSphere = 0. 
//		If ray misses sphere, distToSphere = maxValue; distThroughSphere = 0.
glm::vec2 OpticalDepthTable::raySphere(glm::vec3 rayOrigin, glm::vec3 rayDir) const
{
	// Number of intersections
	glm::vec3 offset = rayOrigin - planetCenter;
	float a = 1;						// Set to dot(rayDir, rayDir) if rayDir might not be normalized
	float b = 2 * dot(offset, rayDir);
	float c = glm::dot(offset, offset) - atmosphereRadius * atmosphereRadius;
	float d = b * b - 4 * a * c;		// Discriminant of quadratic formula (sqrt has 2 solutions/intersections when positive)

	// Two intersections (d > 0)
	if (d > 0)
	{
		float s = sqrt(d);
		float distToSphereNear = std::max(0.f, (-b - s) / (2 * a));
		float distToSphereFar = (-b + s) / (2 * a);

		if (distToSphereFar >= 0)		// Ignore intersections that occur behind the ray
			return glm::vec2(distToSphereNear, distToSphereFar - distToSphereNear);
	}

	// No intersection (d < 0) or one (d = 0)
	return glm::vec2(FLT_MAX, 0);			// https://stackoverflow.com/questions/16069959/glsl-how-to-ensure-largest-possible-float-value-without-overflow

	/*
		/ Line:     y = mx + b
		\ Circle:   r^2 = x^2 + y^2;	y = sqrt(r^2 - x^2)
					r^2 = (x - h)^2 + (y - k)^2;	r^2 = X^2 + x^2 + 2Xx + Y^2 + y^2 + 2Yy

		mx + b = sqrt(r^2 - x^2)
		mmx^2 + b^2 + 2mbx = r^2 - x^2
		mmx^2 + b^2 + 2mbx - r^2 + x^2  = 0
		(mm + 1)x^2 + 2mbx + (b^2 - r^2) = 0
	*/

	//float m = rayDir.y / rayDir.x;	// line's slope
	//float B = rayOrigin.y;			// line's Y-intercept 
	//float a = m * m + 1;
	//float b = 2 * m * B;
	//float c = B * B - atmosphereRadius * atmosphereRadius;
	//float d = b * b - 4 * a * c;
}

DensityVector::DensityVector(float planetRadius, float atmosphereRadius, float stepSize, float densityFallOff)
{
	heightSteps = std::ceil((atmosphereRadius - planetRadius) / stepSize);
	bytes = 4 * heightSteps;
	table.resize(bytes);

	glm::vec2 point = { 0.f, planetRadius };
	glm::vec2 planetCenter = { 0.f, 0.f };
	float heightAboveSurface;
	float height01;
	float* density = (float*)table.data();

	for (size_t i = 0; i < heightSteps; i++)
	{
		heightAboveSurface = glm::length(point - planetCenter) - planetRadius;
		height01 = heightAboveSurface / (atmosphereRadius - planetRadius);

		//density[i] = std::exp(-height01 * densityFallOff);					// There is always some density
		density[i] = std::exp(-height01 * densityFallOff) * (1 - height01);	// Density ends at some distance

		point.y += stepSize;
	}
}