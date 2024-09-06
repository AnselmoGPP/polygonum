#ifndef ECSARCH_HPP
#define ECSARCH_HPP

#include <unordered_map>
#include <typeindex>
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


// Class definitions ----------

/// It stores state data (fields) and have no behavior (no methods).
struct Component
{
    Component();
    virtual ~Component();

    std::type_index typeIndex;
};

/// An ID associated with a set of components.
class Entity
{
    std::unordered_map<std::type_index, std::unique_ptr<Component>> components;

public:
    Entity(std::string name);
    ~Entity();

    template<typename T> void addComp(T* component);
    template<typename T> T* getComp();
    void printInfo();

    const Entity* resourceHandle;
    const std::string name;
};


/// It has behavior (methods) and have no state data (no fields). To each system corresponds a set of components. The systems iterate through their components performing operations (behavior) on their state.
class System
{
public:
    System(EntityManager* entityManager = nullptr);
    virtual ~System();

    EntityManager* em;
    std::type_index typeIndex;

    virtual void update(float timeStep) = 0;
};


/// Acts as a "database", where you look up entities and get their list of components.
class EntityManager
{
    uint32_t getNewId();
    uint32_t lowestUnassignedId = 1;

    std::unordered_map<uint32_t, std::unique_ptr<Entity>> entities;
    std::vector<std::unique_ptr<System>> systems;

public:
    EntityManager();
    ~EntityManager();

    void update(float timeStep);
    void printInfo();

    uint32_t addEntity(Entity* entity);                                     //!< Add new entity by defining its components.
    std::vector<uint32_t> addEntities(std::vector<Entity*> entities);       //!< Add many new entities
    template<typename T> void addComp(uint32_t entityId, T* component);
    template<typename T> void addSystem(T* system);                         //!< Add new system

    template<typename T> std::vector<uint32_t> getEntities();               //!< Get set of entities containing component of type X.
    template<typename T, typename Q> std::vector<uint32_t> getEntities();   //!< Get set of entities containing component of type X and type Y.
    template<typename T> T* getComp(uint32_t entityId);
    std::string getName(uint32_t entityId);

    void removeEntity(uint32_t entityId);

    uint32_t singletonId;   // Id of the entity containing all the singleton components (implementation dependent)
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


// Templates definitions ----------

template<typename T>
void Entity::addComp(T* component)
{
    components[std::type_index(typeid(T))] = std::unique_ptr<T>(component);
}

template<typename T>
T* Entity::getComp()
{
    auto it = components.find(std::type_index(typeid(T)));
    return it != components.end() ? static_cast<T*>(it->second.get()) : nullptr;
}

template<typename T>
void EntityManager::addSystem(T* system)
{
    #ifdef DEBUG_ECS
        std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
    #endif

    system->em = this;
    system->typeIndex = typeid(T);
    systems.push_back(std::unique_ptr<T>(system));
}

template<typename T>
std::vector<uint32_t> EntityManager::getEntities()
{
    std::vector<uint32_t> result;

    for (auto& it : entities)
        if (getComp<T>(it.first))
            result.push_back(it.first);

    return result;
}

template<typename T, typename Q>
std::vector<uint32_t> EntityManager::getEntities()
{
    std::vector<uint32_t> result;

    for (auto& it : entities)
        if (getComp<T>(it.first) && getComp<Q>(it.first))
            result.push_back(it.first);

    return result;
}

template<typename T>
void EntityManager::addComp(uint32_t entityId, T* component)
{
    entities[entityId]->addComp(component);
}

template<typename T>
T* EntityManager::getComp(uint32_t entityId)
{
    auto it = entities.find(entityId);
    return it != entities.end() ? entities[entityId]->getComp<T>() : nullptr;
}


#endif