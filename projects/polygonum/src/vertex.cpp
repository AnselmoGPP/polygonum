#include <iostream>
#include <array>

#include "polygonum/vertex.hpp"
#include "polygonum/renderer.hpp"

VertexType::VertexType(std::initializer_list<uint32_t> attribsSizes, std::initializer_list<VkFormat> attribsFormats)
	: attribsFormats(attribsFormats), attribsSizes(attribsSizes), vertexSize(0)
{
	for (unsigned i = 0; i < this->attribsSizes.size(); i++)
		vertexSize += this->attribsSizes[i];
}

VertexType::VertexType(std::initializer_list<VertAttrib> vertexAttributes)
	: vertexSize(0)
{
	VkFormat format;
	unsigned size;
	bool posAttrib = false;

	for (auto attribute : vertexAttributes)
	{
		format = getFormat(attribute);
		size = getSize(format);
		attribsTypes.push_back(attribute);
		attribsFormats.push_back(format);
		attribsSizes.push_back(size);
		vertexSize += size;
		if (attribute == vaPos) posAttrib = true;
	}

	if (!posAttrib) throw std::runtime_error("Position must be a vertex attribute.");
}

VertexType::VertexType() : vertexSize(0) {}

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

VkFormat VertexType::getFormat(VertAttrib attribute)
{
	//col, col16, col4, uv, boneWeights, boneIndices, instanceTransform

	switch (attribute)
	{
	case vaPos:
	case vaNorm:
	case vaTan:
	case vaFixes:
		return VK_FORMAT_R32G32B32_SFLOAT;   // glm::vec3
	case vaCol:
	case vaBoneWeights:
		return VK_FORMAT_R32G32B32A32_SFLOAT;   // glm::vec4
	case vaCol4:
		return VK_FORMAT_R8G8B8A8_UNORM;   // u8vec4
	case vaUv:
		return VK_FORMAT_R32G32_SFLOAT;   // glm::vec2
	case vaBoneIndices:
		return VK_FORMAT_R32G32B32A32_UINT;   // uvec4
		//case instanceTransform:
		//	return VK_FORMAT_R32G32B32A32_SFLOAT(x4);   // glm::mat4
	default:
		throw std::runtime_error("Attribute not mapped");
	}
}

unsigned VertexType::getSize(VkFormat format)
{
	switch (format)
	{
	case VK_FORMAT_R32G32B32A32_UINT:
	case VK_FORMAT_R32G32B32A32_SFLOAT:
		return 16;
	case VK_FORMAT_R32G32B32_SFLOAT:
		return 12;
	case VK_FORMAT_R32G32_SFLOAT:
	case VK_FORMAT_R16G16B16A16_SFLOAT:
	case VK_FORMAT_R16G16B16A16_SNORM:
		return 8;
	case VK_FORMAT_R32_SFLOAT:
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_R8G8B8A8_SRGB:
	case VK_FORMAT_R16G16_SFLOAT:
		return 4;
	default:
		throw std::runtime_error("Format not mapped");
	}
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

VertexSet::VertexSet() : vertexSize(0), buffer(nullptr), capacity(0), numVertex(0) {}

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

	if (buffer) delete[] buffer;
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

uint32_t VertexSet::getNumVertex() const { return numVertex; }

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

void VertexSet::reset(uint32_t vertexSize, uint32_t numOfVertex, const void* buffer)
{
	if (buffer) delete[] this->buffer;

	this->vertexSize = vertexSize;
	this->numVertex = numOfVertex;

	capacity = static_cast<unsigned>(pow(2, 1 + (int)(log(numOfVertex) / log(2))));		// log b (M) = ln(M) / ln(b)
	this->buffer = new char[capacity * vertexSize];
	std::memcpy(this->buffer, buffer, totalBytes());
}

void VertexSet::reset(uint32_t vertexSize)
{
	if (buffer) delete[] this->buffer;

	this->vertexSize = vertexSize;
	numVertex = 0;
	capacity = 8;

	this->buffer = new char[capacity * vertexSize];
}

VertexesLoader::VertexesLoader(size_t vertexSize, std::initializer_list<VerticesModifier*> modifiers)
	: vertexSize(vertexSize), modifiers(modifiers) {
}

VertexesLoader::~VertexesLoader() {}

void VertexesLoader::loadVertexes(VertexData& result, vec2<std::shared_ptr<Texture>>& textures, Renderer& r)
{
	VertexSet rawVertices;
	std::vector<uint16_t> rawIndices;

	getRawData(rawVertices, rawIndices, textures);		// Get raw data from source
	applyModifiers(rawVertices);
	createBuffers(result, rawVertices, rawIndices, r);		// Upload data to Vulkan
}

void VertexesLoader::createBuffers(VertexData& result, const VertexSet& rawVertices, const std::vector<uint16_t>& rawIndices, Renderer& r)
{
	createVertexBuffer(rawVertices, result, r);
	createIndexBuffer(rawIndices, result, r);
}

void VertexesLoader::applyModifiers(VertexSet& vertexes)
{
	for (VerticesModifier* modifier : modifiers)
		modifier->modify(vertexes);
}

void VertexesLoader::createVertexBuffer(const VertexSet& rawVertices, VertexData& result, Renderer& r)
{
#ifdef DEBUG_RESOURCES
	std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
#endif

	// Create a staging buffer (host visible buffer used as temporary buffer for mapping and copying the vertex data) (https://vkguide.dev/docs/chapter-5/memory_transfers/)
	VkDeviceSize   bufferSize = rawVertices.totalBytes();	// sizeof(vertices[0])* vertices.size();
	VkBuffer	   stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	r.c.createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 											// VK_BUFFER_USAGE_ ... TRANSFER_SRC_BIT / TRANSFER_DST_BIT (buffer can be used as source/destination in a memory transfer operation).
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory);

	// Fill the staging buffer (by mapping the buffer memory into CPU accessible memory: https://en.wikipedia.org/wiki/Memory-mapped_I/O)
	void* data;
	vkMapMemory(r.c.device, stagingBufferMemory, 0, bufferSize, 0, &data);	// Access a memory region. Use VK_WHOLE_SIZE to map all of the memory.
	memcpy(data, rawVertices.data(), (size_t)bufferSize);					// Copy the vertex data to the mapped memory.
	vkUnmapMemory(r.c.device, stagingBufferMemory);						// Unmap memory.

	/*
		Note:
		The driver may not immediately copy the data into the buffer memory (example: because of caching).
		It is also possible that writes to the buffer are not visible in the mapped memory yet. Two ways to deal with that problem:
		  - (Our option) Coherent memory heap: Use a memory heap that is host coherent, indicated with VK_MEMORY_PROPERTY_HOST_COHERENT_BIT. This ensures that the mapped memory always matches the contents of the allocated memory (this may lead to slightly worse performance than explicit flushing, but this doesn't matter since we will use a staging buffer).
		  - Flushing memory: Call vkFlushMappedMemoryRanges after writing to the mapped memory, and call vkInvalidateMappedMemoryRanges before reading from the mapped memory.
		Either option means that the driver will be aware of our writes to the buffer, but it doesn't mean that they are actually visible on the GPU yet.
		The transfer of data to the GPU happens in the background and the specification simply tells us that it is guaranteed to be complete as of the next call to vkQueueSubmit.
	*/

	// Create the actual vertex buffer (Device local buffer used as actual vertex buffer. Generally it doesn't allow to use vkMapMemory, but we can copy from stagingBuffer to vertexBuffer, though you need to specify the transfer source flag for stagingBuffer and the transfer destination flag for vertexBuffer).
	// This makes vertex data to be loaded from high performance memory.
	r.c.createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		result.vertexBuffer,
		result.vertexBufferMemory);

	result.vertexCount = rawVertices.getNumVertex();

	// Move the vertex data to the device local buffer
	r.commander.copyBuffer(stagingBuffer, result.vertexBuffer, bufferSize);

	// Clean up
	r.c.destroyBuffer(r.c.device, stagingBuffer, stagingBufferMemory);
}

void VertexesLoader::createIndexBuffer(const std::vector<uint16_t>& rawIndices, VertexData& result, Renderer& r)
{
#ifdef DEBUG_RESOURCES
	std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
#endif

	result.indexCount = static_cast<uint32_t>(rawIndices.size());

	if (rawIndices.size() == 0) return;

	// Create a staging buffer
	VkDeviceSize   bufferSize = sizeof(rawIndices[0]) * rawIndices.size();
	VkBuffer	   stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	r.c.createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory);

	// Fill the staging buffer
	void* data;
	vkMapMemory(r.c.device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, rawIndices.data(), (size_t)bufferSize);
	vkUnmapMemory(r.c.device, stagingBufferMemory);

	// Create the vertex buffer
	r.c.createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		result.indexBuffer,
		result.indexBufferMemory);

	// Move the vertex data to the device local buffer
	r.commander.copyBuffer(stagingBuffer, result.indexBuffer, bufferSize);

	// Clean up
	r.c.destroyBuffer(r.c.device, stagingBuffer, stagingBufferMemory);
}

glm::vec3 VertexesLoader::getVertexTangent(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec2 uv1, const glm::vec2 uv2, const glm::vec2 uv3)
{
	glm::vec3 edge1 = v2 - v1;
	glm::vec3 edge2 = v3 - v1;
	glm::vec3 uvDiff1 = glm::vec3(uv2, 0) - glm::vec3(uv1, 0);
	glm::vec3 uvDiff2 = glm::vec3(uv3, 0) - glm::vec3(uv1, 0);

	glm::vec3 denominator = (uvDiff1 * edge2 - uvDiff2 * edge1);
	if (abs(denominator.x) < 0.0001f) denominator.x = 0.0001f;			// Handle cases where the denominator approaches zero (which can happen when the triangle's texture coordinates are aligned)
	if (abs(denominator.y) < 0.0001f) denominator.y = 0.0001f;
	if (abs(denominator.z) < 0.0001f) denominator.z = 0.0001f;

	//glm::vec3 tangent   = glm::normalize((uvDiff2 * edge1 - uvDiff1 * edge2) / denominator);
	//glm::vec3 biTangent = glm::normalize((uvDiff1 * edge2 - uvDiff2 * edge1) / denominator);

	return glm::normalize((uvDiff2 * edge1 - uvDiff1 * edge2) / denominator);
}

VL_fromBuffer::VL_fromBuffer(const void* verticesData, uint32_t vertexSize, uint32_t vertexCount, const std::vector<uint16_t>& indices, std::initializer_list<VerticesModifier*> modifiers)
	: VertexesLoader(vertexSize, modifiers)
{
	rawVertices.reset(vertexSize, vertexCount, verticesData);
	rawIndices = indices;
}

VL_fromBuffer* VL_fromBuffer::factory(const void* verticesData, size_t vertexSize, size_t vertexCount, const std::vector<uint16_t>& indices, std::initializer_list<VerticesModifier*> modifiers)
{
	return new VL_fromBuffer(verticesData, vertexSize, vertexCount, indices, modifiers);
}

VertexesLoader* VL_fromBuffer::clone() { return new VL_fromBuffer(*this); }

void VL_fromBuffer::getRawData(VertexSet& destVertices, std::vector<uint16_t>& destIndices, vec2<std::shared_ptr<Texture>>& textures)
{
	destVertices = rawVertices;
	destIndices = rawIndices;
}

VL_fromFile::VL_fromFile(std::string filePath, std::initializer_list<VerticesModifier*> modifiers)
	: VertexesLoader((3 + 3 + 2) * sizeof(float), modifiers), path(filePath), vertices(nullptr), indices(nullptr) {
}

VL_fromFile* VL_fromFile::factory(std::string filePath, std::initializer_list<VerticesModifier*> modifiers)
{
	return new VL_fromFile(filePath, modifiers);
}

VertexesLoader* VL_fromFile::clone() { return new VL_fromFile(*this); }

void VL_fromFile::getRawData(VertexSet& destVertices, std::vector<uint16_t>& destIndices, vec2<std::shared_ptr<Texture>>& textures)
{
	/*
		Data is imported as a Scene (aiScene), which contains:
		  - All meshes (aiMesh)
		  - Root node of a tree (aiNode). Each node contains:
			- Indices to its meshes
			- Children nodes
		Reading: Traverse all nodes and read the vertex data they contain.
	*/

	this->vertices = &destVertices;
	this->indices = &destIndices;
	this->textures = &textures;

	vertices->reset(vertexSize);

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
	// aiProcess_JoinIdenticalVertices | aiProcess_MakeLeftHanded

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
		return;
	}

	processNode(scene, scene->mRootNode);	// recursive
}

void VL_fromFile::processNode(const aiScene* scene, aiNode* node)
{
	// Process all node's meshes
	std::vector<aiMesh*> meshes;

	for (unsigned i = 0; i < node->mNumMeshes; i++)
	{
		//mesh = scene->mMeshes[node->mMeshes[i]];
		meshes.push_back(scene->mMeshes[node->mMeshes[i]]);
		processMeshes(scene, meshes);
	}

	// Repeat process in children
	for (unsigned i = 0; i < node->mNumChildren; i++)
		processNode(scene, node->mChildren[i]);
}

void VL_fromFile::processMeshes(const aiScene* scene, std::vector<aiMesh*>& meshes)
{
	//<<< destVertices->reserve(destVertices->size() + mesh->mNumVertices);
	float* vertex = new float[vertexSize / sizeof(float)];			// [3 + 3 + 2]  (pos, normal, UV)
	unsigned i, j, k;

	// Go through each mesh contained in this node
	for (k = 0; k < meshes.size(); k++)
	{
		// Get VERTEX data (positions, normals, UVs)
		for (i = 0; i < meshes[k]->mNumVertices; i++)
		{
			vertex[0] = meshes[k]->mVertices[i].x;
			vertex[1] = meshes[k]->mVertices[i].y;
			vertex[2] = meshes[k]->mVertices[i].z;

			if (meshes[k]->mNormals)
			{
				vertex[3] = meshes[k]->mNormals[i].x;
				vertex[4] = meshes[k]->mNormals[i].y;
				vertex[5] = meshes[k]->mNormals[i].z;
			}
			else { vertex[3] = 0.f; vertex[4] = 0.f; vertex[5] = 1.f; };

			if (meshes[k]->mTextureCoords)
			{
				vertex[6] = meshes[k]->mTextureCoords[0][i].x;
				vertex[7] = meshes[k]->mTextureCoords[0][i].y;
			}
			else { vertex[6] = 0.f; vertex[7] = 0.f; };

			//if (meshes[k]->mTangents) { };

			vertices->push_back(vertex);	// Get VERTICES
		}

		// Get INDICES
		aiFace face;
		for (i = 0; i < meshes[k]->mNumFaces; i++)
		{
			face = meshes[k]->mFaces[i];
			for (j = 0; j < face.mNumIndices; j++)
				indices->push_back(face.mIndices[j]);	// Get INDICES
		}

		// Process MATERIAL
		if (meshes[k]->mMaterialIndex >= 0)
		{
			aiMaterial* material = scene->mMaterials[meshes[k]->mMaterialIndex];
			aiTextureType types[] = { aiTextureType_DIFFUSE, aiTextureType_SPECULAR };
			aiString fileName;

			for (unsigned i = 0; i < 2; i++)
				for (unsigned j = 0; j < material->GetTextureCount(types[i]); j++)
				{
					if (textures->empty()) textures->push_back(vec<std::shared_ptr<Texture>>());
					material->GetTexture(types[i], j, &fileName);					// get texture file location
					(*textures)[0].push_back(Tex_fromFile::factory(fileName.C_Str()));	// Get RESOURCES
					fileName.Clear();
				}
		}
	}

	delete[] vertex;
}


VerticesModifier::VerticesModifier(glm::vec4 params)
	: params(params) {
}

VerticesModifier::~VerticesModifier() {}

//void VerticesModifier::modify(VertexSet& rawVertices) { }

VerticesModifier_Scale::VerticesModifier_Scale(glm::vec3 scale)
	: VerticesModifier(glm::vec4(scale, 0)) {
}

void VerticesModifier_Scale::modify(VertexSet& rawVertices)
{
	glm::vec3 scale = glm::vec3(params);

	if (scale.x == scale.y && scale.y == scale.z)   // Uniform scaling
	{
		for (size_t i = 0; i < rawVertices.size(); ++i)
			*(glm::vec3*)rawVertices.getElement(i) *= scale;
	}
	else   // Non-uniform scaling
	{
		glm::mat3 scaleMatrix = glm::mat3(params.x, 0, 0, 0, params.y, 0, 0, 0, params.z);
		glm::mat3 normalMatrix = glm::transpose(glm::inverse(scaleMatrix));   // Inverse transpose of the scaling matrix for normals
		glm::vec3* pos;

		for (size_t i = 0; i < rawVertices.size(); ++i)   // Scale points and adjust normals
		{
			pos = (glm::vec3*)rawVertices.getElement(i);
			*pos = scaleMatrix * (*pos);
			pos++;
			*pos = glm::normalize(normalMatrix * (*pos));
		}
	}
}

VerticesModifier_Scale* VerticesModifier_Scale::factory(glm::vec3 scale)
{
	return new VerticesModifier_Scale(scale);
}

VerticesModifier_Rotation::VerticesModifier_Rotation(glm::vec4 rotationQuaternion)
	: VerticesModifier(rotationQuaternion) {
}

void VerticesModifier_Rotation::modify(VertexSet& rawVertices)
{
	glm::vec3* pos;

	for (size_t i = 0; i < rawVertices.size(); ++i)
	{
		pos = (glm::vec3*)rawVertices.getElement(i);
		*pos = rotatePoint(params, *(glm::vec3*)rawVertices.getElement(i));
		pos++;
		*pos = rotatePoint(params, *(glm::vec3*)rawVertices.getElement(i));
	}
}

VerticesModifier_Rotation* VerticesModifier_Rotation::factory(glm::vec4 rotation)
{
	return new VerticesModifier_Rotation(rotation);
}

VerticesModifier_Translation::VerticesModifier_Translation(glm::vec3 position)
	: VerticesModifier(glm::vec4(position, 0.f)) {
}

void VerticesModifier_Translation::modify(VertexSet& rawVertices)
{
	glm::vec3 translation = glm::vec3(params);

	for (size_t i = 0; i < rawVertices.size(); ++i)
		*(glm::vec3*)rawVertices.getElement(i) += translation;
}

VerticesModifier_Translation* VerticesModifier_Translation::factory(glm::vec3 position)
{
	return new VerticesModifier_Translation(position);
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
