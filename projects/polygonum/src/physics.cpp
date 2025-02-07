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

