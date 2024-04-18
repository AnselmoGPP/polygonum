#ifndef PHYSICS_HPP
#define PHYSICS_HPP

#include <vector>
#include <iostream>

//#define GLM_FORCE_RADIANS
//#define GLM_FORCE_DEPTH_ZERO_TO_ONE		// GLM uses OpenGL depth range [-1.0, 1.0]. This macro forces GLM to use Vulkan range [0.0, 1.0].
//#define GLM_ENABLE_EXPERIMENTAL			// Required for using std::hash functions for the GLM types (since gtx folder contains experimental extensions)
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>     // Generate transformations matrices with glm::rotate (model), glm::lookAt (view), glm::perspective (projection).
//#include <glm/gtx/hash.hpp>
#include "glm/gtc/type_ptr.hpp"

//#include "btBulletDynamicsCommon.h"


// BulletPhysics ----------------------------------------------------------
/*
class PhysicsWorld
{
    btBroadphaseInterface*                  broadphase;         //!< Broad-phase algorithm: Used during collision detection for filtering those objects that cannot collide. The remaining objects are passed to the Narrow-phase algorithm that checks actual shapes for collision.
    btDefaultCollisionConfiguration*        collisionConfig;    //!< Runs collision detection.
    btCollisionDispatcher*                  dispatcher;         //!< Dispatches collisions according to a given collision configuration
    btSequentialImpulseConstraintSolver*    solver;             //!< Causes the object to interact taking into account gravity, additional forces, collisions and constrains.
    btDiscreteDynamicsWorld*                world;              //!< World that follows the laws of physics passed to it.

public:
    PhysicsWorld();
    ~PhysicsWorld();
};


// https://www.kodeco.com/2606-bullet-physics-tutorial-getting-started
class PhysicsObject
{
public:
    PhysicsObject();

    void createShapeWithVertices(std::vector<float> &vertices, bool convex);
    void createBodyWithMass(float mass);

    //std::string name;
    btRigidBody* body;          //!< Rigid body
    btCollisionShape* shape;    //!< Shape of the physics body
    float mass;                 //!< Mass can be Static (m=0, immovable), Kinematic (m=0, user can set position and rotation), Dynamic (object movable using forces)
    bool convex;                //!< Physics engines work faster with convex objects
    int tag;                    //!< Used to determine what types of objects collided
    //GLKBaseEffect* shader;
    //std::vector<float> vertices;
};
*/

// Particle ----------------------------------------------------------

float getFHeight(const glm::vec3& pos);


/// State of a particle in a 3D space, with some speed, and subject to gravity acceleration towards (0,0,-1).
class Particle
{
protected:
    glm::vec3 pos;          //!< Position
    glm::vec3 speedVecNP;   //!< Not persistent speed (impulse)
    glm::vec3 speedVecP;    //!< Persistent speed (like that caused by g)
    glm::vec3 gVec;         //!< g acceleration

    bool onFloor;           //!< Flags whether the particle lies on the floor

public:
    Particle(glm::vec3 position, glm::vec3 direction, float speed, float gValue = 9.8, glm::vec3 gDirection = glm::vec3(0,0,-1));
    virtual ~Particle();

    glm::vec3 getPos();
    bool isOnFloor();
    //glm::vec3 getDir();
    //float getSpeed();

    virtual void setPos(glm::vec3 position);
    void setSpeedNP(glm::vec3 speedVector);
    void setSpeedP(glm::vec3 speedVector);
    void setCallback(float(*getFloorHeight)(const glm::vec3& pos));
    //void setDir(glm::vec3 direction);
    //void setSpeed(float speed);

    virtual void updateState(float deltaTime);

    float(*getFloorHeight) (const glm::vec3& pos);
};


// Particle subject to gravity acceleration towards one point.
class PlanetParticle : public Particle
{
    const glm::vec3 nucleus;
    const float g;                //!< g acceleration (magnitude)

public:
    PlanetParticle(glm::vec3 position, glm::vec3 direction, float speed, float gValue, glm::vec3 nucleus);

    void setPos(glm::vec3 position) override;

    void updateState(float deltaTime) override;
};


// Maths ----------------------------------------------------------

extern glm::vec3 xAxis;
extern glm::vec3 yAxis;
extern glm::vec3 zAxis;
extern glm::vec3 zero;

/// Get angle (radians) between 2 vectors from arbitrary origin
float angleBetween(glm::vec3 a, glm::vec3 b, glm::vec3 origin);
float angleBetween(glm::vec3 a, glm::vec3 b);

/// Quaternion that represents no rotation
extern glm::vec4 noRotQuat;

/// Get rotation quaternion. Quaternions are a 4 dimensional extension of complex numbers. Useful for rotations, and more efficient than Rotation matrices (Euler angles) (https://danceswithcode.net/engineeringnotes/quaternions/quaternions.html).
glm::vec4 getRotQuat(glm::vec3 rotAxis, float angle);

/// Use a rotation quaternion for rotating a 3D point. Active rotation (point rotated with respect to coordinate system). 
glm::vec3 rotatePoint(const glm::vec4& rotQuat, const glm::vec3& point);

/// Get the Hamilton product of 2 or 3 quaternions (q1 rotation first, then q2, then q3). The product of two rotation quaternions (A * B) will be equivalent to rotation B followed by rotation A (around the rotation axes the object has at the beginning).
glm::vec4 productQuat(const glm::vec4& q1, const glm::vec4& q2, const glm::vec4& q3);
glm::vec4 productQuat(const glm::vec4& q1, const glm::vec4& q2);

/// Get rotation matrix. Use it to rotate a point (result = rotMatrix * point) (http://answers.google.com/answers/threadview/id/361441.html).
glm::mat3 getRotationMatrix(glm::vec3 rotAxis, float angle);

/// Get rotation matrix from a rotation quaternion (https://www.mathworks.com/help/nav/ref/quaternion.rotmat.html).
glm::mat4 getRotationMatrix(glm::vec4 quat);

// Others ----------------------------------------------------------

void printV(std::string begin, glm::vec3 vec);

#endif