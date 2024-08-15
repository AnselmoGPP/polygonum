#ifndef ECSARCH_HPP
#define ECSARCH_HPP

#include <map>
//#include <list>
#include <string>
#include <vector>
#include <memory>
//#include <initializer_list>


// Prototypes ----------

class Entity;
struct Component;
class System;
class EntityManager;
class MainEntityFactory;


// Definitions ----------

// Component type (CT). Used for identifying the type of component.
enum class CT
{
    engine,
    input,
    camera,
    lights,
    sky,
    model,
    modelParams,
    move,
    planet,
    distros,
    distributor
};

/// It stores state data (fields) and have no behavior (no methods).
struct Component
{
    Component(CT type);
    virtual ~Component();

    const CT type;
};

/// An ID associated with a set of components.
class Entity
{
    std::vector<std::shared_ptr<Component>> components;

public:
    Entity(uint32_t id, std::string name, std::vector<Component*>& components);
    ~Entity();

    void addComponent(Component* component);
    const std::vector<std::shared_ptr<Component>>& getComponents();
    Component* getSingleComponent(CT type);
    std::vector<Component*> getAllComponents();

    const uint32_t id;
    const Entity* resourceHandle;
    const std::string name;
};


/// It has behavior (methods) and have no state data (no fields). To each system corresponds a set of components. The systems iterate through their components performing operations (behavior) on their state.
class System
{
public:
    System(EntityManager* entityManager = nullptr) : em(entityManager) { };
    virtual ~System();

    EntityManager* em;

    virtual void update(float timeStep) = 0;
};


/// Acts as a "database", where you look up entities and get their list of components.
class EntityManager
{
    //std::vector<Component> m_componentPool;
    uint32_t getNewId();
    uint32_t lowestUnassignedId = 1;

    std::map<uint32_t, Entity*> entities;
    std::vector<System*> systems;
    //std::vector<std::shared_ptr<Component>> sComponents;     // singleton components. Type shared_ptr prevents singleton components to be deleted during entity destruction.

public:
    EntityManager();
    ~EntityManager();

    void update(float timeStep);
    void printInfo();

    // Entity methods
    uint32_t addEntity(std::string name, std::vector<Component*> components);                //!< Add new entity by defining its components.
    std::vector<uint32_t> addEntities(std::vector<std::string> names, std::vector<std::vector<Component*>> entities); //!< Add many entities.
    std::vector<uint32_t> getEntitySet(CT type);                            //!< Get set of entities containing component of type X.
    std::string getName(uint32_t entityId);

    // Component methods
    Component* getSComponent(CT type);                      //!< Get the first component found of type X in the whole set of entities. Useful for singleton components.
    Component* getComponent(CT type, uint32_t entityId);    //!< Get the first component found of type X in a given entity.
    std::vector<Component*> getComponents(CT type);

    // System methods
    void addSystem(System* system);                                     //!< Add new system

    // Others
    void removeEntity(uint32_t entityId);
    void addComponentToEntity(uint32_t entityId, Component* component);
};


class MainEntityFactory
{
    //EntityManager* entityManager;

public:
    MainEntityFactory() { };
    //MainEntityFactory(EntityManager* entityManager) : entityManager(entityManager) { };

    //Entity* createHumanPlayer();
    //Entity* createAIPlayer();
    //Entity* createBasicMonster();
};

#endif