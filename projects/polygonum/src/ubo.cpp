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

UbosArrayInfo::UbosArrayInfo(size_t maxNumUbos, size_t numActiveUbos, size_t minUboSize)
	: maxNumUbos(maxNumUbos), numActiveUbos(numActiveUbos), minUboSize(minUboSize) { }

UbosArrayInfo::UbosArrayInfo(size_t maxNumUbos, size_t numActiveUbos, size_t minUboSize, const std::vector<std::string>& glslLines)
	: maxNumUbos(maxNumUbos), numActiveUbos(numActiveUbos), minUboSize(minUboSize), glslLines(glslLines) { }

UbosArrayInfo::UbosArrayInfo()
	: maxNumUbos(0), numActiveUbos(0), minUboSize(0) { }

UBOsArray::UBOsArray(Renderer* renderer, const UbosArrayInfo& bindingInfo) :
	c(&renderer->c),
	swapChain(&renderer->swapChain),
	maxNumUbos(bindingInfo.maxNumUbos),
	uboSize(bindingInfo.minUboSize ? c->deviceData.minUniformBufferOffsetAlignment * (1 + bindingInfo.minUboSize / c->deviceData.minUniformBufferOffsetAlignment) : 0),
	totalBytes(uboSize* maxNumUbos),
	binding(totalBytes),
	glslLines(bindingInfo.glslLines)
{
	setNumActiveUbos(bindingInfo.numActiveUbos);
}

UBOsArray::UBOsArray()
	: c(nullptr), swapChain(nullptr), maxNumUbos(0), numActiveUbos(0), uboSize(0), totalBytes(0), binding(0), bindingBuffers(0), bindingMemories(0)
{ };

UBOsArray::UBOsArray(UBOsArray&& other) noexcept
	: c(other.c),
	swapChain(other.swapChain),
	maxNumUbos(other.maxNumUbos),
	numActiveUbos(other.numActiveUbos),
	uboSize(other.uboSize),
	totalBytes(other.totalBytes)
{
	binding = std::move(other.binding);
	bindingBuffers = std::move(other.bindingBuffers);
	bindingMemories = std::move(other.bindingMemories);
}

UBOsArray& UBOsArray::operator=(UBOsArray&& other) noexcept
{
	if (this != &other)
	{
		// Transfer resources ownership
		c = other.c;
		swapChain = other.swapChain,
		maxNumUbos = other.maxNumUbos;
		numActiveUbos = other.numActiveUbos;
		uboSize = other.uboSize;
		totalBytes = other.totalBytes;

		binding = std::move(other.binding);
		bindingBuffers = std::move(other.bindingBuffers);
		bindingMemories = std::move(other.bindingMemories);

		// Leave other in valid state
		other.maxNumUbos = 0;
		other.numActiveUbos = 0;
		other.uboSize = 0;
		other.totalBytes = 0;

		other.binding.clear();
		other.bindingBuffers.clear();
		other.bindingMemories.clear();
	}
	return *this;
}

uint8_t* UBOsArray::getUboPtr(size_t uboIndex) { return binding.data() + uboIndex * uboSize; }

bool UBOsArray::setNumActiveUbos(size_t count)
{
	if (count > maxNumUbos)
	{
		numActiveUbos = maxNumUbos;
		return false;
	}

	numActiveUbos = count;
	return true;
}

// (21)
void UBOsArray::createBinding()
{
	if (!maxNumUbos) return;

	bindingBuffers.resize(swapChain->images.size());
	bindingMemories.resize(swapChain->images.size());
	
	//destroyUniformBuffers();		// Not required since Renderer calls this first

	if (uboSize)
		for (size_t i = 0; i < swapChain->images.size(); i++)
			c->createBuffer(
				maxNumUbos == 0 ? uboSize : totalBytes,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				bindingBuffers[i],
				bindingMemories[i]);
}

void UBOsArray::destroyBinding()
{
	if (uboSize)
		for (size_t i = 0; i < swapChain->images.size(); i++)
			c->destroyBuffer(c->device, bindingBuffers[i], bindingMemories[i]);
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
