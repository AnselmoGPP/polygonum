
#include <iostream>

#include "ECSarch.hpp"
#include "commons.hpp"


Component::Component(CT type) : type(type) { }
Component::~Component() 
{ 
	#ifdef DEBUG_ECS
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif
}

Entity::Entity(uint32_t id, std::string name, std::vector<Component*>& components)
	: id(id), resourceHandle(this), name(name)
{ 
	for (Component* comp : components)
		this->components.push_back(std::shared_ptr<Component>(comp));
};

Entity::~Entity() 
{ 
	#ifdef DEBUG_ECS
		std::cout << typeid(*this).name() << "::" << __func__ << " (1/2)" << std::endl;
	#endif

	//for (Component* comp : components)
	//	delete comp;

	#ifdef DEBUG_ECS
		std::cout << typeid(*this).name() << "::" << __func__ << " (2/2)" << std::endl;
	#endif
}

const std::vector<std::shared_ptr<Component>>& Entity::getComponents() { return components; };

void Entity::addComponent(Component* component) { components.push_back(std::shared_ptr<Component>(component)); };

Component* Entity::getSingleComponent(CT type)
{
	for (uint32_t i = 0; i < components.size(); i++)
		if (type == components[i]->type)
			return components[i].get();
	
	return nullptr;
}

std::vector<Component*> Entity::getAllComponents()
{
	std::vector<Component*> result;

	for (unsigned i = 0; i < components.size(); i++)
		result.push_back(components[i].get());

	return result;
}

System::~System()
{ 
	#ifdef DEBUG_ECS
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif
};

EntityManager::EntityManager() { }

EntityManager::~EntityManager() 
{ 
	#ifdef DEBUG_ECS
		std::cout << typeid(*this).name() << "::" << __func__ << " (1/2)" << std::endl;
	#endif

	// Delete entities
	for (auto it = entities.begin(); it != entities.end(); it++)
		delete it->second;

	entities.clear();
	
	// Delete systems
	for (unsigned i = 0; i < systems.size(); i++)
		delete systems[i];

	systems.clear();

	#ifdef DEBUG_ECS
		std::cout << typeid(*this).name() << "::" << __func__ << " (2/2)" << std::endl;
	#endif
};

uint32_t EntityManager::getNewId()
{
	if (lowestUnassignedId < UINT32_MAX) return lowestUnassignedId++;
	
	for (uint32_t i = 1; i < UINT32_MAX; ++i)
		if (entities.find(i) == entities.end())
			return i;

	std::cout << "ERROR: No available IDs!" << std::endl;
	return 0;
}

void EntityManager::update(float timeStep)
{
	#ifdef DEBUG_ECS
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	for (System* s : systems)
		s->update(timeStep);
}

void EntityManager::printInfo()
{
	std::vector<Component*> comps;

	// Print Entities & Components
	for (auto it = entities.begin(); it != entities.end(); it++)
		comps = it->second->getAllComponents();
}

uint32_t EntityManager::addEntity(std::string name, std::vector<Component*> components)
{
	#ifdef DEBUG_ECS
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	uint32_t newId = getNewId();
	if (newId) entities[newId] = new Entity(newId, name, components);
	return newId;
}

std::vector<uint32_t> EntityManager::addEntities(std::vector<std::string> names, std::vector<std::vector<Component*>> newEntities)
{
	#ifdef DEBUG_ECS
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	std::vector<uint32_t> newIds;
	uint32_t newId;

	for (int i = 0; i < newEntities.size(); i++)
	{
		newId = getNewId();
		if (newId)
		{
			entities[newId] = new Entity(newId, names[i], newEntities[i]);
			newIds.push_back(newId);
		}
	}
	
	return newIds;
}

void EntityManager::addSystem(System* system)
{
	#ifdef DEBUG_ECS
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	system->em = this;
	this->systems.push_back(system);
}

std::vector<uint32_t> EntityManager::getEntitySet(CT type)
{
	std::vector<uint32_t> result;

	for (auto it = entities.begin(); it != entities.end(); it++)
		if (it->second->getSingleComponent(type))
			result.push_back(it->second->id);

	return result;
}

Component* EntityManager::getSComponent(CT type)
{
	Component* result;

	for (auto it = entities.begin(); it != entities.end(); it++)
	{
		result = it->second->getSingleComponent(type);
		if (result) return result;
	}

	return nullptr;
}

void EntityManager::removeEntity(uint32_t entityId)
{
	#ifdef DEBUG_ECS
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	if (entities.find(entityId) != entities.end())
	{
		delete entities[entityId];
		entities.erase(entityId);
	}
}

void EntityManager::addComponentToEntity(uint32_t entityId, Component* component)
{
	entities[entityId]->addComponent(component);
}

Component* EntityManager::getComponent(CT type, uint32_t entityId)
{
	return entities[entityId]->getSingleComponent(type);
}

std::vector<Component*> EntityManager::getComponents(CT type)
{
	std::vector<Component*> componentSet;
	Component* component;

	for (auto it = entities.begin(); it != entities.end(); it++)
	{
		component = it->second->getSingleComponent(type);
		if (component) componentSet.push_back(component);
	}

	return componentSet;
}

std::string EntityManager::getName(uint32_t entityId)
{
	return entities[entityId]->name;
}
