
#include <iostream>

#include "ubo.hpp"


Sizes size;


// (Set of) Uniform Buffer Objects -----------------------------------------------------------------

/// Constructor. Computes sizes (range, totalBytes) and allocates buffers (ubo, offsets).
UBO::UBO(VulkanEnvironment* e, UBOinfo uboInfo)
	:e(e),
	maxNumDescriptors(uboInfo.maxNumDescriptors),
	descriptorSize(uboInfo.minDescriptorSize ? e->c.deviceData.minUniformBufferOffsetAlignment * (1 + uboInfo.minDescriptorSize / e->c.deviceData.minUniformBufferOffsetAlignment) : 0),
	totalBytes(descriptorSize* maxNumDescriptors),
	ubo(totalBytes)
{
	setNumActiveDescriptors(uboInfo.numActiveDescriptors);
}

UBO::UBO() 
	: maxNumDescriptors(0), numActiveDescriptors(0), descriptorSize(0), totalBytes(0), ubo(0), uboBuffers(0), uboMemories(0) 
{ };

uint8_t* UBO::getDescriptorPtr(size_t descriptorIndex) { return ubo.data() + descriptorIndex * descriptorSize; }

bool UBO::setNumActiveDescriptors(size_t count)
{
	if (count > maxNumDescriptors)
	{
		numActiveDescriptors = maxNumDescriptors;
		return false;
	}

	numActiveDescriptors = count;
	return true;
}

// (21)
void UBO::createUBObuffers()
{
	if (!maxNumDescriptors) return;

	uboBuffers.resize(e->swapChain.images.size());
	uboMemories.resize(e->swapChain.images.size());
	
	//destroyUniformBuffers();		// Not required since Renderer calls this first

	if (descriptorSize)
		for (size_t i = 0; i < e->swapChain.images.size(); i++)
			createBuffer(
				e,
				maxNumDescriptors == 0 ? descriptorSize : totalBytes,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				uboBuffers[i],
				uboMemories[i]);
}

void UBO::destroyUBOs()
{
	if (descriptorSize)
	{
		for (size_t i = 0; i < e->swapChain.images.size(); i++)
		{
			vkDestroyBuffer(e->c.device, uboBuffers[i], nullptr);
			vkFreeMemory(e->c.device, uboMemories[i], nullptr);
			e->c.memAllocObjects--;
		}
	}
}

Material::Material(glm::vec3& diffuse, glm::vec3& specular, float shininess)
	: diffuse(diffuse), specular(specular), shininess(shininess) { }


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
