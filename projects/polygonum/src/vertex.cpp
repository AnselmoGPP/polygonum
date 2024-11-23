#include <iostream>
#include <array>

#include "polygonum/vertex.hpp"


//VertexType -----------------------------------------------------------------

//const std::array<size_t, 4> VertexType::attribsSize = { sizeof(glm::vec3), sizeof(glm::vec3), sizeof(glm::vec2), sizeof(glm::vec3) };

VertexType::VertexType(std::initializer_list<size_t> attribsSizes, std::initializer_list<VkFormat> attribsFormats)
	: attribsFormats(attribsFormats), attribsSizes(attribsSizes), vertexSize(0)
{ 
	for (unsigned i = 0; i < this->attribsSizes.size(); i++)
		vertexSize += this->attribsSizes[i];
}

VertexType::VertexType() : vertexSize(0) { }

VertexType::~VertexType()
{
	#ifdef DEBUG_RESOURCES
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif
}

VertexType& VertexType::operator=(const VertexType& obj)
{
	if (&obj == this) return *this;

	attribsFormats = obj.attribsFormats;
	attribsSizes = obj.attribsSizes;
	vertexSize = obj.vertexSize;

	return *this;
}

VkVertexInputBindingDescription VertexType::getBindingDescription() const
{
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;									// Index of the binding in the array of bindings. We have a single array, so we only have one binding.
	bindingDescription.stride = vertexSize;							// Number of bytes from one entry to the next.
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;		// VK_VERTEX_INPUT_RATE_ ... VERTEX, INSTANCE (move to the next data entry after each vertex or instance).

	return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> VertexType::getAttributeDescriptions() const
{
	VkVertexInputAttributeDescription vertexAttrib;
	uint32_t location = 0;
	uint32_t offset = 0;

	std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

	for (unsigned i = 0; i < attribsSizes.size(); i++)
	{
		vertexAttrib.binding = 0;							// From which binding the per-vertex data comes.
		vertexAttrib.location = location;					// Directive "location" of the input in the vertex shader.
		vertexAttrib.format = attribsFormats[i];			// Type of data for the attribute: VK_FORMAT_ ... R32_SFLOAT (float), R32G32_SFLOAT (vec2), R32G32B32_SFLOAT (vec3), R32G32B32A32_SFLOAT (vec4), R64_SFLOAT (64-bit double), R32G32B32A32_UINT (uvec4: 32-bit unsigned int), R32G32_SINT (ivec2: 32-bit signed int)...
		vertexAttrib.offset = offset;						// Number of bytes since the start of the per-vertex data to read from. // offsetof(VertexPCT, pos);	

		location++;
		offset += attribsSizes[i];
		attributeDescriptions.push_back(vertexAttrib);
	}

	return attributeDescriptions;
}


//VerteSet -----------------------------------------------------------------

VertexSet::VertexSet() : vertexSize(0), buffer(nullptr), capacity(0), numVertex(0) { }

VertexSet::VertexSet(size_t vertexSize)
	: vertexSize(vertexSize), capacity(8), numVertex(0)
{ 
	this->buffer = new char[capacity * vertexSize];
}

VertexSet::~VertexSet()
{
	#ifdef DEBUG_RESOURCES
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	if(buffer) delete[] buffer;
};

VertexSet& VertexSet::operator=(const VertexSet& obj)
{
	if (&obj == this) return *this;

	numVertex = obj.numVertex;
	vertexSize = obj.vertexSize;
	capacity = obj.capacity;

	delete[] buffer;
	buffer = new char[capacity * vertexSize];
	std::memcpy(buffer, obj.buffer, totalBytes());

	return *this;
}

VertexSet::VertexSet(const VertexSet& obj)
{
	numVertex = obj.numVertex;
	vertexSize = obj.vertexSize;
	capacity = obj.capacity;

	buffer = new char[capacity * vertexSize];
	std::memcpy(buffer, obj.buffer, totalBytes());
}

size_t VertexSet::totalBytes() const { return numVertex * vertexSize; }

size_t VertexSet::size() const { return numVertex; }

char* VertexSet::data() const { return buffer; }

void* VertexSet::getElement(size_t i) const { return &(buffer[i * vertexSize]); }

void VertexSet::printElement(size_t i) const
{
	float* ptr = (float*)getElement(i);
	size_t size = vertexSize / sizeof(float);
	for (size_t i = 0; i < size; i++)
		std::cout << *((float*)ptr + i) << ", ";
	std::cout << std::endl;
}

void VertexSet::printAllElements() const
{
	for (size_t i = 0; i < numVertex; i++)
		printElement(i);
}

size_t VertexSet::getNumVertex() const { return numVertex; }

void VertexSet::push_back(const void* element)
{
	// Resize buffer if required
	if (numVertex == capacity)
		reserve(2 * capacity);

	std::memcpy(&buffer[totalBytes()], (char*)element, vertexSize);
	numVertex++;
}

void VertexSet::reserve(unsigned newCapacity)
{
	if (newCapacity > capacity)
	{
		char* temp = new char[newCapacity * vertexSize];
		std::memcpy(temp, buffer, totalBytes());
		delete[] buffer;
		buffer = temp;
		capacity = newCapacity;
	}
	else if (newCapacity < capacity)
	{
		char* temp = new char[newCapacity * vertexSize];
		std::memcpy(temp, buffer, newCapacity * vertexSize);
		delete[] buffer;
		buffer = temp;
		capacity = newCapacity;
	}
}

void VertexSet::reset(size_t vertexSize, size_t numOfVertex, const void* buffer)
{
	if(buffer) delete[] this->buffer;

	this->vertexSize = vertexSize;
	this->numVertex = numOfVertex;

	capacity = pow(2, 1 + (int)(log(numOfVertex) / log(2)));		// log b (M) = ln(M) / ln(b)
	this->buffer = new char[capacity * vertexSize];
	std::memcpy(this->buffer, buffer, totalBytes());
}

void VertexSet::reset(size_t vertexSize)
{
	if(buffer) delete[] this->buffer;

	this->vertexSize = vertexSize;
	numVertex = 0;
	capacity = 8;

	this->buffer = new char[capacity * vertexSize];
}


//Vertex PCT (Position, Color, Texture) -----------------------------------------------------------------

VertexPCT::VertexPCT(glm::vec3 vertex, glm::vec3 vertexColor, glm::vec2 textureCoordinates)
	: pos(vertex), color(vertexColor), texCoord(textureCoordinates) { }

VkVertexInputBindingDescription VertexPCT::getBindingDescription()
{
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;							// Index of the binding in the array of bindings. We have a single array, so we only have one binding.
	bindingDescription.stride = sizeof(VertexPCT);			// Number of bytes from one entry to the next.
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;	// VK_VERTEX_INPUT_RATE_ ... VERTEX, INSTANCE (move to the next data entry after each vertex or instance).

	return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 3> VertexPCT::getAttributeDescriptions()
{
	std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

	attributeDescriptions[0].binding = 0;							// From which binding the per-vertex data comes.
	attributeDescriptions[0].location = 0;							// Directive "location" of the input in the vertex shader.
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;	// Type of data for the attribute: VK_FORMAT_ ... R32_SFLOAT (float), R32G32_SFLOAT (vec2), R32G32B32_SFLOAT (vec3), R32G32B32A32_SFLOAT (vec4), R64_SFLOAT (64-bit double), R32G32B32A32_UINT (uvec4: 32-bit unsigned int), R32G32_SINT (ivec2: 32-bit signed int)...
	attributeDescriptions[0].offset = offsetof(VertexPCT, pos);		// Number of bytes since the start of the per-vertex data to read from.

	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset = offsetof(VertexPCT, color);

	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[2].offset = offsetof(VertexPCT, texCoord);

	return attributeDescriptions;
}

bool VertexPCT::operator==(const VertexPCT& other) const {
	return	pos == other.pos &&
		color == other.color &&
		texCoord == other.texCoord;
}

size_t std::hash<VertexPCT>::operator()(VertexPCT const& vertex) const
{
	return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
}
