#ifndef ECS_HPP
#define ECS_HPP

#include <unordered_map>
#include <typeindex>


// Prototypes ----------

class Entity;
struct Component;
class System;
class EntitiesManager;
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
    Entity(std::string name);

    std::unordered_map<std::type_index, std::unique_ptr<Component>> components;

public:
    ~Entity();

    static Entity* newEntity(std::string name);
    template <typename T, typename... Args> void addComponent(Args&&... args);
    template <typename T, typename K, typename... Args> void addComponent(Args&&... args);
    template<typename T> T* getComponent();
    void printInfo();

    const Entity* resourceHandle;
    const std::string name;
};


/// It has behavior (methods) and have no state data (no fields). To each system corresponds a set of components. The systems iterate through their components performing operations (behavior) on their state.
class System
{
public:
    System(EntitiesManager* entityManager = nullptr);
    virtual ~System();

    EntitiesManager* em;
    std::type_index typeIndex;

    virtual void update(float timeStep) = 0;
};

/// Acts as a "database", where you look up entities and get their list of components.
class EntitiesManager
{
    uint32_t getNewId();
    uint32_t lowestUnassignedId = 1;

    std::unordered_map<uint32_t, std::unique_ptr<Entity>> entities;
    std::vector<std::unique_ptr<System>> systems;

public:
    EntitiesManager();
    ~EntitiesManager();

    void update(float timeStep);
    void printInfo();

    uint32_t addEntity(Entity* entity);   //!< Add new entity by defining its components.
    template<typename T, typename... Args> void addSystem(Args&&... args);   //!< Add new system

    template<typename T> std::vector<uint32_t> getEntities();               //!< Get set of entities containing component of type X.
    template<typename T, typename Q> std::vector<uint32_t> getEntities();   //!< Get set of entities containing component of type X and type Y.
    template<typename T> T* getComponent(uint32_t entityId);   //!< Get a certain component from an entity.
    std::string getName(uint32_t entityId);

    void removeEntity(uint32_t entityId);

    // Useful ids. Feel free to define new ones here.
    uint32_t singletonId;   // Id of the entity containing all the singleton components (implementation dependent)
    uint32_t planetId;
    uint32_t seaId;
};


class MainEntityFactory
{
    //EntitiesManager* entitiesManager;

public:
    MainEntityFactory() { };
    //MainEntityFactory(EntitiesManager* entityManager) : entitiesManager(entitiesManager) { };

    //Entity* createHumanPlayer();
    //Entity* createAIPlayer();
    //Entity* createBasicMonster();
};


// Templates definitions ----------

template <typename T, typename... Args>
void Entity::addComponent(Args&&... args)
{
    components[std::type_index(typeid(T))] = std::make_unique<T>(std::forward<Args>(args)...);
}

template <typename T, typename K, typename... Args>
void Entity::addComponent(Args&&... args)
{
    components[std::type_index(typeid(T))] = std::unique_ptr<T>((T*) new K(std::forward<Args>(args)...));
}

template<typename T>
T* Entity::getComponent()
{
    auto it = components.find(std::type_index(typeid(T)));
    return it != components.end() ? static_cast<T*>(it->second.get()) : nullptr;
}

template<typename T, typename... Args>
void EntitiesManager::addSystem(Args&&... args)
{
    #ifdef DEBUG_ECS
        std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
    #endif
    
    std::unique_ptr<T> systemPtr = std::make_unique<T>(std::forward<Args>(args)...);
    systemPtr->em = this;
    systemPtr->typeIndex = typeid(T);
    systems.push_back(std::move(systemPtr));
}

template<typename T>
std::vector<uint32_t> EntitiesManager::getEntities()
{
    std::vector<uint32_t> result;

    for (auto& it : entities)
        if (getComponent<T>(it.first))
            result.push_back(it.first);

    return result;
}

template<typename T, typename Q>
std::vector<uint32_t> EntitiesManager::getEntities()
{
    std::vector<uint32_t> result;

    for (auto& it : entities)
        if (getComponent<T>(it.first) && getComponent<Q>(it.first))
            result.push_back(it.first);

    return result;
}

template<typename T>
T* EntitiesManager::getComponent(uint32_t entityId)
{
    auto it = entities.find(entityId);
    return it != entities.end() ? entities[entityId]->getComponent<T>() : nullptr;
}

#endif