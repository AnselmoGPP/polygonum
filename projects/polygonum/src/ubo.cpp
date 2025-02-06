#include <iostream>

#include "polygonum/ubo.hpp"
#include "polygonum/environment.hpp"


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

UBOinfo::UBOinfo(size_t maxNumSubUbos, size_t numActiveSubUbos, size_t minSubUboSize)
	: maxNumSubUbos(maxNumSubUbos), numActiveSubUbos(numActiveSubUbos), minSubUboSize(minSubUboSize) { }

UBOinfo::UBOinfo()
	: maxNumSubUbos(0), numActiveSubUbos(0), minSubUboSize(0) { }

UBO::UBO(Renderer* renderer, UBOinfo uboInfo) :
	c(&renderer->c),
	swapChain(&renderer->swapChain),
	maxNumSubUbos(uboInfo.maxNumSubUbos),
	subUboSize(uboInfo.minSubUboSize ? c->deviceData.minUniformBufferOffsetAlignment * (1 + uboInfo.minSubUboSize / c->deviceData.minUniformBufferOffsetAlignment) : 0),
	totalBytes(subUboSize* maxNumSubUbos),
	ubo(totalBytes)
{
	setNumActiveSubUbos(uboInfo.numActiveSubUbos);
}

UBO::UBO() 
	: c(nullptr), swapChain(nullptr), maxNumSubUbos(0), numActiveSubUbos(0), subUboSize(0), totalBytes(0), ubo(0), uboBuffers(0), uboMemories(0)
{ };

UBO::UBO(UBO&& other) noexcept
	: c(other.c),
	swapChain(other.swapChain),
	maxNumSubUbos(other.maxNumSubUbos),
	numActiveSubUbos(other.numActiveSubUbos),
	subUboSize(other.subUboSize),
	totalBytes(other.totalBytes)
{
	ubo = std::move(other.ubo);
	uboBuffers = std::move(other.uboBuffers);
	uboMemories = std::move(other.uboMemories);
}

UBO& UBO::operator=(UBO&& other) noexcept
{
	if (this != &other)
	{
		// Transfer resources ownership
		c = other.c;
		swapChain = other.swapChain,
		maxNumSubUbos = other.maxNumSubUbos;
		numActiveSubUbos = other.numActiveSubUbos;
		subUboSize = other.subUboSize;
		totalBytes = other.totalBytes;

		ubo = std::move(other.ubo);
		uboBuffers = std::move(other.uboBuffers);
		uboMemories = std::move(other.uboMemories);

		// Leave other in valid state
		other.maxNumSubUbos = 0;
		other.numActiveSubUbos = 0;
		other.subUboSize = 0;
		other.totalBytes = 0;

		other.ubo.clear();
		other.uboBuffers.clear();
		other.uboMemories.clear();
	}
	return *this;
}

uint8_t* UBO::getSubUboPtr(size_t descriptorIndex) { return ubo.data() + descriptorIndex * subUboSize; }

bool UBO::setNumActiveSubUbos(size_t count)
{
	if (count > maxNumSubUbos)
	{
		numActiveSubUbos = maxNumSubUbos;
		return false;
	}

	numActiveSubUbos = count;
	return true;
}

// (21)
void UBO::createUBO()
{
	if (!maxNumSubUbos) return;

	uboBuffers.resize(swapChain->images.size());
	uboMemories.resize(swapChain->images.size());
	
	//destroyUniformBuffers();		// Not required since Renderer calls this first

	if (subUboSize)
		for (size_t i = 0; i < swapChain->images.size(); i++)
			c->createBuffer(
				maxNumSubUbos == 0 ? subUboSize : totalBytes,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				uboBuffers[i],
				uboMemories[i]);
}

void UBO::destroyUBO()
{
	if (subUboSize)
		for (size_t i = 0; i < swapChain->images.size(); i++)
			c->destroyBuffer(c->device, uboBuffers[i], uboMemories[i]);
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
