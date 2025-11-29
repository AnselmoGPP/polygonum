#include <iostream>

#include "polygonum/ubo.hpp"
#include "polygonum/renderer.hpp"


namespace sizes {
	size_t UniformAlignment = 16;
	size_t vec2 = sizeof(glm::vec2);
	size_t vec3 = sizeof(glm::vec3);
	size_t vec4 = sizeof(glm::vec4);
	size_t ivec4 = sizeof(glm::ivec4);
	size_t mat4 = sizeof(glm::mat4);
	//size_t materialSize = sizeof(Material);
	//size_t lightSize;
}

//BindingInfo::~BindingInfo() { }

BindingBuffer::BindingBuffer(BindingBufferType descType, uint32_t numDescs, uint32_t numSubDescs, VkDeviceSize descSize, const std::vector<std::string>& glslLines)
	: c(nullptr), swapChain(nullptr),
	numDescriptors(numDescs),
	numSubDescriptors(numSubDescs),
	descriptorSize(alignedDescriptorSize(numDescs, descType, descSize)),
	glslLines(glslLines)
{
	if (!numDescriptors || !descriptorSize) throw std::runtime_error("Buffer cannot have size 0");

	binding.resize(numDescriptors * descriptorSize);
	size = binding.size();

	switch (descType)
	{
	case ubo:
		type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		break;
	case ssbo:
		type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		break;
	default:
		type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
		usage = VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
		throw std::runtime_error("Not valid descriptor type");
	}
}

BindingBuffer::BindingBuffer(const BindingBuffer& obj)
	: c(obj.c), swapChain(obj.swapChain), size(obj.size), type(obj.type), usage(obj.usage), numDescriptors(obj.numDescriptors), descriptorSize(obj.descriptorSize), numSubDescriptors(obj.numSubDescriptors), binding(obj.binding), glslLines(obj.glslLines)
{
	// Members "bindingBuffers" and "bindingMemories" are not copied because they're destroyed by the destructor.
}

BindingBuffer::~BindingBuffer() { destroyBuffer(); }

BindingBuffer::BindingBuffer(BindingBuffer&& other) noexcept
	: c(std::move(other.c)),
	swapChain(std::move(other.swapChain)),
	size(std::move(other.size)),
	type(std::move(other.type)),
	usage(std::move(other.usage)),
	numDescriptors(other.numDescriptors),   // Cannot use std::move on const variables.
	descriptorSize(other.descriptorSize),
	numSubDescriptors(std::move(other.numSubDescriptors)),
	binding(std::move(other.binding)),
	bindingBuffers(std::move(other.bindingBuffers)),
	bindingMemories(std::move(other.bindingMemories)),
	glslLines(std::move(other.glslLines))
{ }

BindingBuffer& BindingBuffer::operator=(const BindingBuffer& obj)
{
	if (&obj == this) return *this;

	c = obj.c;
	swapChain = obj.swapChain;
	size = obj.size;

	type = obj.type;
	usage = obj.usage;
	numDescriptors = obj.numDescriptors;
	descriptorSize = obj.descriptorSize;
	numSubDescriptors = obj.numSubDescriptors;
	binding = obj.binding;
	glslLines = obj.glslLines;

	return *this;
}

uint32_t BindingBuffer::alignedDescriptorSize(size_t numDescriptors, BindingBufferType descriptorType, size_t originalDescriptorSize)
{
	if (numDescriptors == 1) return originalDescriptorSize;   // No need to align if the binding has only one descriptor.
	if (numDescriptors == 0) return 0;

	VkDeviceSize alignment;

	switch (descriptorType)
	{
	case ubo:
		alignment = c->deviceData.minUniformBufferOffsetAlignment;
		break;
	case ssbo:
		alignment = c->deviceData.minStorageBufferOffsetAlignment;
		break;
	default:
		throw std::runtime_error("Not valid descriptor type");
	}

	//return (originalDescriptorSize + alignment - 1) & ~(alignment - 1);
	return alignment * ((originalDescriptorSize + alignment - 1) / alignment);

}

/*
BindingBuffer& BindingBuffer::operator=(BindingBuffer&& other) noexcept
{
	if (this != &other)
	{
		// Transfer resources ownership
		c = other.c;
		swapChain = other.swapChain;
		numDescriptors = other.numDescriptors;
		descriptorSize = other.descriptorSize;
		capacity = other.capacity;
		size = other.size;

		binding = std::move(other.binding);
		bindingBuffers = std::move(other.bindingBuffers);
		bindingMemories = std::move(other.bindingMemories);

		// Leave other in valid state
		other.numDescriptors = 0;
		other.descriptorSize = 0;
		other.capacity = 0;
		other.size = 0;

		other.binding.clear();
		other.bindingBuffers.clear();
		other.bindingMemories.clear();
	}
	return *this;
}
*/
uint8_t* BindingBuffer::getDescriptor(size_t index)
{
	uint8_t* ptr = binding.data() + index * descriptorSize;
	return ptr;
}

// (21)
void BindingBuffer::createBuffer(Renderer* rend)
{
	if (!numDescriptors || !descriptorSize)
	{
		std::cerr << "Cannot create buffer of size 0." << std::endl;
		return;
	}
	if (binding.empty()) return;   // if buffer was created
	if (!rend) return;

	c = &rend->c;
	swapChain = &rend->swapChain;

	bindingBuffers.resize(swapChain->images.size());
	bindingMemories.resize(swapChain->images.size());
	
	//destroyUniformBuffers();		// Not required since Renderer calls this first

	for (size_t i = 0; i < swapChain->images.size(); i++)
		c->createBuffer(
			getCapacity(),
			usage,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			bindingBuffers[i],
			bindingMemories[i]);
}

void BindingBuffer::destroyBuffer()
{
	if (isFullyConstructed())
		for (size_t i = 0; i < swapChain->images.size(); i++)
			c->destroyBuffer(c->device, bindingBuffers[i], bindingMemories[i]);
}

bool BindingBuffer::isFullyConstructed() { return bindingBuffers.size(); }

uint32_t BindingBuffer::getSize() const { return size; }

uint32_t BindingBuffer::getCapacity() const  { return binding.size(); }

void BindingBuffer::setSize(uint32_t newSize)
{
	if (newSize > getCapacity())
		size = getCapacity();
	else
		size = newSize;
}

void BindingBuffer::setSize_subs(uint32_t numActiveSubDescriptors)
{
	if (numActiveSubDescriptors > numSubDescriptors)
		size = getCapacity();
	else
		size = (getCapacity() / numSubDescriptors) * numActiveSubDescriptors;
}

Material::Material(glm::vec3& diffuse, glm::vec3& specular, float shininess)
	: diffuse(diffuse), specular(specular), shininess(shininess) { }


// Light -------------------------------------------------------------

void Light::turnOff() { type = 0; }

void Light::setDirectional(glm::vec3 direction, glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular)
{
	this->type = 1;
	this->direction = direction;

	this->ambient = ambient;
	this->diffuse = diffuse;
	this->specular = specular;
}

void Light::setPoint(glm::vec3 position, glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular, float constant, float linear, float quadratic)
{
	this->type = 2;
	this->position = position;

	this->ambient = ambient;
	this->diffuse = diffuse;
	this->specular = specular;

	this->degree.x = constant;
	this->degree.y = linear;
	this->degree.z = quadratic;
}

void Light::setSpot(glm::vec3 position, glm::vec3 direction, glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular, float constant, float linear, float quadratic, float cutOff, float outerCutOff)
{
	this->type = 3;
	this->position = position;
	this->direction = direction;

	this->ambient = ambient;
	this->diffuse = diffuse;
	this->specular = specular;

	this->degree.x = constant;
	this->degree.y = linear;
	this->degree.z = quadratic;

	this->cutOff.x = cutOff;
	this->cutOff.y = outerCutOff;
}


// LightSet -------------------------------------------------------------

LightSet::LightSet(size_t numLights, size_t numActiveLights)
	: set(nullptr),
	bytesSize(numLights * sizeof(Light)), 
	numLights(numLights), 
	numActiveLights(numActiveLights > numLights ? numLights : numActiveLights)
{
	if (numLights)
	{
		set = new Light[numLights];

		for (size_t i = 0; i < numLights; i++)
			set[i].type = 0;
	}
}

LightSet::~LightSet()
{
	#ifdef DEBUG_RESOURCES
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif
	
	if (set);
		if (numLights == 1) delete set;
		else if (numLights > 1) delete[] set;
}

void LightSet::turnOff(size_t index)
{
	if (index > numLights) return;

	set[index].type = 0;
}

void LightSet::addDirectional(size_t index, glm::vec3 direction, glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular)
{
	if (index >= numLights) return;

	set[index].type      = 1;
	set[index].direction = direction;

	set[index].ambient   = ambient;
	set[index].diffuse   = diffuse;
	set[index].specular  = specular;
}

void LightSet::addPoint(size_t index, glm::vec3 position, glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular, float constant, float linear, float quadratic)
{
	if (index >= numLights) return;

	set[index].type      = 2;
	set[index].position  = position;

	set[index].ambient   = ambient;
	set[index].diffuse   = diffuse;
	set[index].specular  = specular;

	set[index].degree.x  = constant;
	set[index].degree.y  = linear;
	set[index].degree.z  = quadratic;
}

void LightSet::addSpot(size_t index, glm::vec3 position, glm::vec3 direction, glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular, float constant, float linear, float quadratic, float cutOff, float outerCutOff)
{
	if (index >= numLights) return;

	set[index].type      = 3;
	set[index].position  = position;
	set[index].direction = direction;

	set[index].ambient   = ambient;
	set[index].diffuse   = diffuse;
	set[index].specular  = specular;

	set[index].degree.x  = constant;
	set[index].degree.y  = linear;
	set[index].degree.z  = quadratic;

	set[index].cutOff.x  = cutOff;
	set[index].cutOff.y  = outerCutOff;
}

void LightSet::printLights() const
{
	for (unsigned i = 0; i < numLights; i++)
		std::cout
			<< "Light " << i << ':' << '\n'
			<< "   Type: " << set[i].type << '\n'
			<< "   Pos: " << set[i].position.x << ", " << set[i].position.y << ", " << set[i].position.z << '\n'
			<< "   Dir: " << set[i].direction.x << ", " << set[i].direction.y << ", " << set[i].direction.z << '\n'
			<< "   Ambient: " << set[i].ambient.x << ", " << set[i].ambient.y << ", " << set[i].ambient.z << '\n'
			<< "   Diffuse: " << set[i].diffuse.x << ", " << set[i].diffuse.y << ", " << set[i].diffuse.z << '\n'
			<< "   Specular: " << set[i].specular.x << ", " << set[i].specular.y << ", " << set[i].specular.z << '\n'
			<< "   Degree: " << set[i].degree.x << ", " << set[i].degree.y << ", " << set[i].degree.z << '\n'
			<< "   CutOff: " << set[i].cutOff.x << ", " << set[i].cutOff.y << ", " << '\n'
			<< std::endl;
}
