#ifndef PHYSICS_HPP
#define PHYSICS_HPP

#include "polygonum/commons.hpp"

// <<< Implement: Collision detection, rigid body simulation


// Particle ----------------------------------------------------------

float getFHeight(const glm::vec3& pos);   //!< Callback example

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

// OTHERS --------------------------------------------------------

/// Precompute all Optical Depth values through the atmosphere. Useful for creating a lookup table for atmosphere rendering.
class OpticalDepthTable
{
    glm::vec3 planetCenter;
    unsigned planetRadius;
    unsigned atmosphereRadius;
    unsigned numOptDepthPoints;
    float heightStep;
    float angleStep;
    float densityFallOff;

    float opticalDepth(glm::vec3 rayOrigin, glm::vec3 rayDir, float rayLength) const;
    float densityAtPoint(glm::vec3 point) const;
    glm::vec2 raySphere(glm::vec3 rayOrigin, glm::vec3 rayDir) const;

public:
    OpticalDepthTable(unsigned numOptDepthPoints, unsigned planetRadius, unsigned atmosphereRadius, float heightStep, float angleStep, float densityFallOff);

    std::vector<unsigned char> table;
    size_t heightSteps;
    size_t angleSteps;
    size_t bytes;
};

/// Precompute all Density Values through the atmosphere. Useful for creating a lookup table for atmosphere rendering.
class DensityVector
{
public:
    DensityVector(float planetRadius, float atmosphereRadius, float stepSize, float densityFallOff);

    std::vector<unsigned char> table;
    size_t heightSteps;
    size_t bytes;
};

#endif