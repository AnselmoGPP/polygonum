#include <iostream>

#include "polygonum/ecs.hpp"
#include "polygonum/commons.hpp"


Component::Component()
	: typeIndex(typeid(Component))
{ }

Component::~Component() 
{ 
	#ifdef DEBUG_ECS
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif
}

Entity::Entity(std::string name)
	: resourceHandle(this), name(name)
{ }

Entity::~Entity() 
{ 
	#ifdef DEBUG_ECS
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif
}

void Entity::printInfo()
{
	std::cout << name << '(' << typeid(Entity).name() << ")\n";
	
	for (const auto& pair : components)
		std::cout << "   " << pair.first.name() << '\n';
		//std::cout << "   " << typeid(pair.second).name() << '\n';
}

System::System(EntitiesManager* entityManager)
	: typeIndex(typeid(System)), em(entityManager)
{ }

System::~System()
{ 
	#ifdef DEBUG_ECS
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif
};

EntitiesManager::EntitiesManager() : singletonId(1) { }

EntitiesManager::~EntitiesManager()
{ 
	#ifdef DEBUG_ECS
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif
};

uint32_t EntitiesManager::getNewId()
{
	if (lowestUnassignedId < UINT32_MAX) return lowestUnassignedId++;
	
	for (uint32_t i = 1; i < UINT32_MAX; ++i)
		if (entities.find(i) == entities.end())
			return i;

	std::cout << "ERROR: No available IDs!" << std::endl;
	return 0;
}

void EntitiesManager::update(float timeStep)
{
	#ifdef DEBUG_ECS
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	for (auto& s : systems)
		s->update(timeStep);
}

void EntitiesManager::printInfo()
{
	std::cout << "Entities ----------\n";
	for (const auto& e : entities)
		e.second->printInfo();

	std::cout << "Systems -----------\n";
	for (const auto& s : systems)
		std::cout << "   " << s->typeIndex.name() << '\n';
}

uint32_t EntitiesManager::addEntity(Entity* entity)
{
	#ifdef DEBUG_ECS
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	uint32_t newId = getNewId();
	if (newId)
		entities[newId] = std::unique_ptr<Entity>(entity);

	return newId;
}

std::vector<uint32_t> EntitiesManager::addEntities(std::vector<Entity*> entities)
{
	#ifdef DEBUG_ECS
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	std::vector<uint32_t> newIds;

	for (auto& entity : entities)
		newIds.push_back(addEntity(entity));

	return newIds;
}

void EntitiesManager::removeEntity(uint32_t entityId)
{
	#ifdef DEBUG_ECS
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	if (entities.find(entityId) != entities.end())
		entities.erase(entityId);
}

std::string EntitiesManager::getName(uint32_t entityId)
{
	return entities[entityId]->name;
}
