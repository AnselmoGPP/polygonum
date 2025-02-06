#include <iostream>
#include <array>

#define STB_IMAGE_IMPLEMENTATION		// Import textures
#include "stb_image.h"

#include "polygonum/environment.hpp"
#include "polygonum/models.hpp"
#include "polygonum/importer.hpp"
#include "polygonum/physics.hpp"


const VertexType vt_3   ({ 3 * sizeof(float) }, { VK_FORMAT_R32G32B32_SFLOAT });
const VertexType vt_32  ({ 3 * sizeof(float), 2 * sizeof(float) }, { VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT });
const VertexType vt_33  ({ 3 * sizeof(float), 3 * sizeof(float) }, { VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT });
const VertexType vt_332 ({ 3 * sizeof(float), 3 * sizeof(float), 2 * sizeof(float) }, { VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT });
const VertexType vt_333 ({ 3 * sizeof(float), 3 * sizeof(float), 3 * sizeof(float) }, { VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT });
const VertexType vt_3332({ 3 * sizeof(float), 3 * sizeof(float), 3 * sizeof(float), 2 * sizeof(float) }, { VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT });

std::vector<TextureLoader> noTextures;
std::vector<uint16_t> noIndices;

// RESOURCES --------------------------------------------------------

ResourcesLoader::ResourcesLoader(VertexesLoader* VertexesLoader, std::vector<ShaderLoader*>& shadersInfo, std::vector<TextureLoader*>& texturesInfo)
	: vertices(VertexesLoader), shaders(shadersInfo), textures(texturesInfo) { }

void ResourcesLoader::loadResources(ModelData& model, Renderer& rend)
{
	#ifdef DEBUG_RESOURCES
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif
	
	// <<< why not using Renderer to access destination buffers?
	
	// Load vertexes and indices
	vertices->loadVertexes(model.vert, this, rend);
	
	{
		const std::lock_guard<std::mutex> lock(rend.worker.mutResources);
		
		// Load shaders
		for (unsigned i = 0; i < shaders.size(); i++)
			model.shaders.push_back(shaders[i]->loadShader(rend.shaders, rend.c));
		
		// Load textures
		for (unsigned i = 0; i < textures.size(); i++)
			model.textures.push_back(textures[i]->loadTexture(rend.textures, rend));
	}
}


// VERTICES --------------------------------------------------------

VertexesLoader::VertexesLoader(size_t vertexSize, std::initializer_list<VerticesModifier*> modifiers)
	: vertexSize(vertexSize), modifiers(modifiers) { }

VertexesLoader::~VertexesLoader() { }

void VertexesLoader::loadVertexes(VertexData& result, ResourcesLoader* resources, Renderer& r)
{
	VertexSet rawVertices;
	std::vector<uint16_t> rawIndices;
	
	getRawData(rawVertices, rawIndices, *resources);		// Get raw data from source
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
	vkDestroyBuffer(r.c.device, stagingBuffer, nullptr);
	vkFreeMemory(r.c.device, stagingBufferMemory, nullptr);
	r.c.memAllocObjects--;
}

void VertexesLoader::createIndexBuffer(const std::vector<uint16_t>& rawIndices, VertexData& result, Renderer& r)
{
	#ifdef DEBUG_RESOURCES
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	result.indexCount = rawIndices.size();

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
	vkDestroyBuffer(r.c.device, stagingBuffer, nullptr);
	vkFreeMemory(r.c.device, stagingBufferMemory, nullptr);
	r.c.memAllocObjects--;
}

glm::vec3 VertexesLoader::getVertexTangent(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec2 uv1, const glm::vec2 uv2, const glm::vec2 uv3)
{
	glm::vec3 edge1 = v2 - v1;
	glm::vec3 edge2 = v3 - v1;
	glm::vec3 uvDiff1 = glm::vec3(uv2, 0) - glm::vec3(uv1, 0);
	glm::vec3 uvDiff2 = glm::vec3(uv3, 0) - glm::vec3(uv1, 0);

	glm::vec3 denominator = (uvDiff1 * edge2 - uvDiff2 * edge1);
	if (abs(denominator.x) < 0.0001) denominator.x = 0.0001;			// Handle cases where the denominator approaches zero (which can happen when the triangle's texture coordinates are aligned)
	if (abs(denominator.y) < 0.0001) denominator.y = 0.0001;
	if (abs(denominator.z) < 0.0001) denominator.z = 0.0001;

	//glm::vec3 tangent   = glm::normalize((uvDiff2 * edge1 - uvDiff1 * edge2) / denominator);
	//glm::vec3 biTangent = glm::normalize((uvDiff1 * edge2 - uvDiff2 * edge1) / denominator);

	return glm::normalize((uvDiff2 * edge1 - uvDiff1 * edge2) / denominator);
}

VL_fromBuffer::VL_fromBuffer(const void* verticesData, size_t vertexSize, size_t vertexCount, const std::vector<uint16_t>& indices, std::initializer_list<VerticesModifier*> modifiers)
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

void VL_fromBuffer::getRawData(VertexSet& destVertices, std::vector<uint16_t>& destIndices, ResourcesLoader& destResources)
{
	destVertices = rawVertices;
	destIndices = rawIndices;
}

VL_fromFile::VL_fromFile(std::string filePath, std::initializer_list<VerticesModifier*> modifiers)
	: VertexesLoader((3+3+2) * sizeof(float), modifiers), path(filePath), vertices(nullptr), indices(nullptr), resources(nullptr) { }

VL_fromFile* VL_fromFile::factory(std::string filePath, std::initializer_list<VerticesModifier*> modifiers)
{
	return new VL_fromFile(filePath, modifiers);
}

VertexesLoader* VL_fromFile::clone() { return new VL_fromFile(*this); }

void VL_fromFile::getRawData(VertexSet& destVertices, std::vector<uint16_t>& destIndices, ResourcesLoader& destResources)
{
	this->vertices = &destVertices;
	this->indices = &destIndices;
	this->resources = &destResources;

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
	//aiMesh* mesh;
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

void VL_fromFile::processMeshes(const aiScene* scene, std::vector<aiMesh*> &meshes)
{
	//<<< destVertices->reserve(destVertices->size() + mesh->mNumVertices);
	float* vertex = new float[vertexSize / sizeof(float)];			// [3 + 3 + 2]  (pos, normal, UV)
	unsigned i, j, k;

	// Go through each mesh contained in this node
	for (k = 0; k < meshes.size(); k++)
	{
		// Get VERTEX data (positions, normals, UVs) and store it.
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

		// Get INDICES and store them.
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
					material->GetTexture(types[i], j, &fileName);					// get texture file location
					resources->textures.push_back(TL_fromFile::factory(fileName.C_Str()));	// Get RESOURCES
					fileName.Clear();
				}
		}
	}

	delete[] vertex;
}


VerticesModifier::VerticesModifier(glm::vec4 params)
	: params(params) { }

VerticesModifier::~VerticesModifier() { }

//void VerticesModifier::modify(VertexSet& rawVertices) { }

VerticesModifier_Scale::VerticesModifier_Scale(glm::vec3 scale)
	: VerticesModifier(glm::vec4(scale, 0)) { }

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
		glm::mat3 scaleMatrix = glm::mat3(params.x, 0, 0,   0, params.y, 0,   0, 0, params.z);
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
	: VerticesModifier(rotationQuaternion) { }

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
	: VerticesModifier(glm::vec4(position, 0.f)) { }

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


// SHADERS --------------------------------------------------------

Shader::Shader(VulkanCore& c, const std::string id, VkShaderModule shaderModule) 
	: c(c), id(id), shaderModule(shaderModule) { }

Shader::~Shader() 
{
	#ifdef DEBUG_IMPORT
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	vkDestroyShaderModule(c.device, shaderModule, nullptr); 
}

SMod::SMod(unsigned modificationType, std::initializer_list<std::string> params)
	: modificationType(modificationType), params(params) { }

bool SMod::applyModification(std::string& shader)
{
	bool result = false;
	
	switch (modificationType)
	{
	case 1:   // - sm_albedo: (FS) Sampler used
		result = findTwoAndReplaceBetween(shader, "vec4 albedo", ";",
			"vec4 albedo = texture(texSampler[" + params[0] + "], inUVs)");
		break;

	case 2:   // sm_specular: (FS) Sampler used
		result = findTwoAndReplaceBetween(shader, "vec3 specular", ";",
			"vec3 specular = texture(texSampler[" + params[0] + "], inUVs).xyz");
		break;

	case 3:   // sm_roughness: (FS) Sampler used
		result = findTwoAndReplaceBetween(shader, "float roughness", ";",
			"float roughness = texture(texSampler[" + params[0] + "], inUVs).x");
		break;

	case 4:   // sm_normal: (VS, FS) Sampler used   <<< doesn't work yet
		result = findTwoAndReplaceBetween(shader, "vec3 normal", ";",
			"vec3 normal = planarNormal(texSampler[" + params[0] + "], inUVs, inTB, inNormal, 1)");
		for (int i = 0; i < 3; i++)
			if (!findStrAndErase(shader, "//normal: ")) break;
		findStrAndReplace(shader, "layout(location = 4) flat", "layout(location = 5) flat");
		break;

	case 5:   // - sm_discardAlpha: (FS) Discard fragments with low alpha
		result = findStrAndErase(shader, "//discardAlpha: ");
		break;

	case 6:   // sm_backfaceNormals: (VS) Recalculate normal based on backfacing
		result = findStrAndErase(shader, "//backfaceNormals: ");
		break;

	case 7:   // sm_sunfaceNormals: (VS) Recalculate normal based on light facing
		result = findStrAndErase(shader, "//sunfaceNormals: ");
		break;

	case 8:   // - sm_verticalNormals: (VS) Make all normals (before MVP transformation) vertical (vec3(0,0,1))
		result = findStrAndErase(shader, "outNormal = mat3(ubo.normalMatrix) * inNormal;");
		findStrAndErase(shader, "//verticalNormals: ");
		break;

	case 9:   // - sm_waving: (VS) Make mesh wave (wind)
		result = findStrAndErase(shader, "//waving: ");
		findStrAndReplace(shader, "<speed>", params[0]);
		findStrAndReplace(shader, "<amplitude>", params[1]);
		findStrAndReplace(shader, "<minHeight>", params[2]);
		findStrAndReplace(shader, "<minHeight>", params[2]);
		break;

	case 10:   // - sm_distDithering: (FS) Apply dithering to distant fragments
		result = findStrAndErase(shader, "//distDithering: ");
		findStrAndReplace(shader, "<near>", params[0]);
		findStrAndReplace(shader, "<far>", params[1]);
		break;

	case 11:   // - sm_earlyDepthTest: (FS) Apply Early Depth/Fragment Test
		result = findStrAndErase(shader, "//earlyDepthTest: ");
		break;

	case 12:   // - sm_dryColor: (FS) Apply dry color filter to albedo depending on fragment height
		result = findStrAndErase(shader, "//dryColor: ");
		findStrAndReplace(shader, "<dryColor>", params[0]);
		findStrAndReplace(shader, "<minHeight>", params[1]);
		findStrAndReplace(shader, "<maxHeight>", params[2]);
		break;

	case 13:   // sm_changeHeader: Change header (#include header_path)
		result = findStrAndReplaceLine(shader, "#include", "#include \"" + params[0] + '\"');
		break;

	case 0:   // sm_none
	default:
		break;
	}
	
	return result;
}

SMod SMod::none() { return SMod(0); }
SMod SMod::albedo(std::string index) { return SMod(1, { index }); }
SMod SMod::specular(std::string index) { return SMod(2, { index }); }
SMod SMod::roughness(std::string index) { return SMod(3, { index }); }
SMod SMod::normal() { return SMod(4); }
SMod SMod::discardAlpha() { return SMod(5); }
SMod SMod::backfaceNormals() { return SMod(6); }
SMod SMod::sunfaceNormals() { return SMod(7); }
SMod SMod::verticalNormals() { return SMod(8); }
SMod SMod::wave(std::string speed, std::string amplitude, std::string minHeight) { return SMod(9, {speed, amplitude, minHeight}); }
SMod SMod::distDithering(std::string near, std::string far) { return SMod(10, {near, far}); }
SMod SMod::earlyDepthTest() { return SMod(11); }
SMod SMod::dryColor(std::string color, std::string minHeight, std::string maxHeight) { return SMod(12, {color, minHeight, maxHeight}); }
SMod SMod::changeHeader(std::string path) { return SMod(13, { path }); }

unsigned SMod::getModType() { return modificationType; }

std::vector<std::string> SMod::getParams() { return params; }

bool SMod::findTwoAndReplaceBetween(std::string& text, const std::string& str1, const std::string& str2, const std::string& replacement)
{
	size_t pos1 = text.find(str1, 0);
	size_t pos2 = text.find(str2, pos1);
	if (pos1 == text.npos || pos2 == text.npos) return false;

	text.replace(pos1, pos2 - pos1, (replacement));
	return true;
}

bool SMod::findStrAndErase(std::string& text, const std::string& str)
{
	size_t pos = text.find(str, 0);
	if (pos == text.npos) return false;

	text.erase(pos, str.size());
	return true;
}

bool SMod::findStrAndReplace(std::string& text, const std::string& str, const std::string& replacement)
{
	size_t pos = text.find(str, 0);
	if (pos == text.npos) return false;

	text.replace(text.begin() + pos, text.begin() + pos + str.size(), replacement);
	return true;
}

bool SMod::findStrAndReplaceLine(std::string& text, const std::string& str, const std::string& replacement)
{
	size_t pos = text.find(str, 0);
	if (pos == text.npos) return false;

	size_t eol = text.find('\n', pos) - 1;
	if (eol == text.npos) return false;
	eol++;	// <<< why is this needed? Otherwise, something in the text is messed up (#line)

	text.replace(text.begin() + pos, text.begin() + eol, replacement);
	return true;
}

ShaderLoader::ShaderLoader(const std::string& id, const std::initializer_list<SMod>& modifications)
	: id(id), mods(modifications)
{
	if (mods.size())	// if there're modifiers, id has to change. Otherwise, it's possible that 2 different shaders have same name when the original shader is the same.
		for (SMod mod : mods)
		{
			this->id += "_" + std::to_string((int)mod.getModType());
			
			for(const std::string& str : mod.getParams())
				this->id += "_" + str;
		}
}

std::shared_ptr<Shader> ShaderLoader::loadShader(PointersManager<std::string, Shader>& loadedShaders, VulkanCore& c)
{
	#ifdef DEBUG_RESOURCES
		std::cout << typeid(*this).name() << "::" << __func__ << ": " << this->id << std::endl;
	#endif
	
	// Look for it in loadedShaders
	if (loadedShaders.contains(id))
		return loadedShaders.get(id);

	// Load shader (if not loaded yet)
	std::string glslData;
	getRawData(glslData);
	
	// Make some changes to the shader string.
	if (mods.size()) applyModifications(glslData);

	// Compile data (preprocessing > compilation):
	shaderc::CompileOptions options;
	options.SetIncluder(std::make_unique<ShaderIncluder>());
	options.SetGenerateDebugInfo();
	//if (optimize) options.SetOptimizationLevel(shaderc_optimization_level_performance);	// This option makes shaderc::CompileGlslToSpv fail when Assimp::Importer is present in code, even if an Importer object is not created (odd) (Importer is in DataFromFile2::loadVertex).
	
	shaderc::Compiler compiler;
	
	shaderc::PreprocessedSourceCompilationResult preProcessed = compiler.PreprocessGlsl(glslData.data(), glslData.size(), shaderc_glsl_infer_from_source, id.c_str(), options);
	if (preProcessed.GetCompilationStatus() != shaderc_compilation_status_success)
		std::cerr << "Shader module preprocessing failed - " << preProcessed.GetErrorMessage() << std::endl;
	
	std::string ppData(preProcessed.begin());
	shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(ppData.data(), ppData.size(), shaderc_glsl_infer_from_source, id.c_str(), options);

	if (module.GetCompilationStatus() != shaderc_compilation_status_success)
		std::cerr << "Shader module compilation failed - " << module.GetErrorMessage() << std::endl;
	
	std::vector<uint32_t> spirv = { module.cbegin(), module.cend() };
	
	//Create shader module:

	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = spirv.size() * sizeof(uint32_t);
	createInfo.pCode = reinterpret_cast<const uint32_t*>(spirv.data());	// The default allocator from std::vector ensures that the data satisfies the alignment requirements of `uint32_t`.
	
	VkShaderModule shaderModule;
	if (vkCreateShaderModule(c.device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		throw std::runtime_error("Failed to create shader module!");
	
	// Create and save shader object
	return loadedShaders.emplace(id, c, id, shaderModule);
}

void ShaderLoader::applyModifications(std::string& shader)
{
	for (auto& mod : mods)
		mod.applyModification(shader);
}

SL_fromBuffer::SL_fromBuffer(const std::string& id, const std::string& glslText, const std::initializer_list<SMod>& modifications)
	: ShaderLoader(id, modifications), data(glslText) { }

ShaderLoader* SL_fromBuffer::clone() { return new SL_fromBuffer(*this); }

void SL_fromBuffer::getRawData(std::string& glslData) { glslData = data; }

SL_fromBuffer* SL_fromBuffer::factory(std::string id, const std::string& glslText, std::initializer_list<SMod> modifications)
{
	return new SL_fromBuffer(id, glslText, modifications);
}

SL_fromFile::SL_fromFile(const std::string& filePath, std::initializer_list<SMod>& modifications)
	: ShaderLoader(filePath, modifications), filePath(filePath) { };

ShaderLoader* SL_fromFile::clone() { return new SL_fromFile(*this); }

void SL_fromFile::getRawData(std::string& glslData) { readFile(filePath.c_str(), glslData); }

SL_fromFile* SL_fromFile::factory(std::string filePath, std::initializer_list<SMod> modifications)
{
	return new SL_fromFile(filePath, modifications);
}

shaderc_include_result* ShaderIncluder::GetInclude(const char* sourceName, shaderc_include_type type, const char* destName, size_t includeDepth)
{
	auto container = new std::array<std::string, 2>;
	(*container)[0] = std::string(sourceName);
	readFile(sourceName, (*container)[1]);

	auto data = new shaderc_include_result;
	data->user_data = container;
	data->source_name = (*container)[0].data();
	data->source_name_length = (*container)[0].size();
	data->content = (*container)[1].data();
	data->content_length = (*container)[1].size();

	return data;
}

void ShaderIncluder::ReleaseInclude(shaderc_include_result* data)
{
	delete static_cast<std::array<std::string, 2>*>(data->user_data);
	delete data;
}


// TEXTURE --------------------------------------------------------

Texture::Texture(VulkanCore& c, const std::string& id, VkImage textureImage, VkDeviceMemory textureImageMemory, VkImageView textureImageView, VkSampler textureSampler)
	: c(c), id(id), textureImage(textureImage), textureImageMemory(textureImageMemory), textureImageView(textureImageView), textureSampler(textureSampler) { }

Texture::~Texture()
{
	#ifdef DEBUG_IMPORT
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	vkDestroySampler(c.device, textureSampler, nullptr);
	vkDestroyImage(c.device, textureImage, nullptr);
	vkDestroyImageView(c.device, textureImageView, nullptr);
	vkFreeMemory(c.device, textureImageMemory, nullptr);
	c.memAllocObjects--;
}

TextureLoader::TextureLoader(const std::string& id, VkFormat imageFormat, VkSamplerAddressMode addressMode)
	: id(id), imageFormat(imageFormat), addressMode(addressMode) { };

std::shared_ptr<Texture> TextureLoader::loadTexture(PointersManager<std::string, Texture>& loadedTextures, Renderer& r)
{
	#ifdef DEBUG_RESOURCES
		std::cout << typeid(*this).name() << "::" << __func__ << ": " << this->id << std::endl;
	#endif
	
	//this->c = &core;
	
	// Look for it in loadedShaders
	if (loadedTextures.contains(id))
		return loadedTextures.get(id);

	// Load an image
	unsigned char* pixels;
	int32_t texWidth, texHeight;
	getRawData(pixels, texWidth, texHeight);
	
	// Get arguments for creating the texture object
	uint32_t mipLevels;		//!< Number of levels (mipmaps)
	std::pair<VkImage, VkDeviceMemory> image = createTextureImage(pixels, texWidth, texHeight, mipLevels, r);
	VkImageView textureImageView             = createTextureImageView(std::get<VkImage>(image), mipLevels, r.c);
	VkSampler textureSampler                 = createTextureSampler(mipLevels, r.c);
	
	// Create and save texture object
	return loadedTextures.emplace(id, r.c, id, std::get<VkImage>(image), std::get<VkDeviceMemory>(image), textureImageView, textureSampler);
}

std::pair<VkImage, VkDeviceMemory> TextureLoader::createTextureImage(unsigned char* pixels, int32_t texWidth, int32_t texHeight, uint32_t& mipLevels, Renderer& r)
{
	#ifdef DEBUG_RESOURCES
		std::cout << "   " << __func__ << std::endl;
	#endif

	VkDeviceSize imageSize = texWidth * texHeight * 4;												// 4 bytes per rgba pixel
	mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;	// Calculate the number levels (mipmaps)
	
	// Create a staging buffer (temporary buffer in host visible memory so that we can use vkMapMemory and copy the pixels to it)
	VkBuffer	   stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	r.c.createBuffer(
		imageSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory);
	
	// Copy directly the pixel values from the image we loaded to the staging-buffer.
	void* data;
	vkMapMemory(r.c.device, stagingBufferMemory, 0, imageSize, 0, &data);	// vkMapMemory retrieves a host virtual address pointer (data) to a region of a mappable memory object (stagingBufferMemory). We have to provide the logical device that owns the memory (e.device).
	memcpy(data, pixels, static_cast<size_t>(imageSize));					// Copies a number of bytes (imageSize) from a source (pixels) to a destination (data).
	vkUnmapMemory(r.c.device, stagingBufferMemory);						// Unmap a previously mapped memory object (stagingBufferMemory).
	stbi_image_free(pixels);	// Clean up the original pixel array
	
	// Create the texture image
	VkImage			textureImage;
	VkDeviceMemory	textureImageMemory;

	Image::createImage(
		textureImage,
		textureImageMemory,
		r.c,
		texWidth, texHeight,
		mipLevels,
		VK_SAMPLE_COUNT_1_BIT,
		imageFormat,			// VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R64_SFLOAT
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	// Copy the staging buffer to the texture image
	r.commander.transitionImageLayout(textureImage, imageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);					// Transition the texture image to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	r.commander.copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));											// Execute the buffer to image copy operation
	// Transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps
	// transitionImageLayout(textureImage, imageFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels);	// To be able to start sampling from the texture image in the shader, we need one last transition to prepare it for shader access
	r.commander.generateMipmaps(textureImage, imageFormat, texWidth, texHeight, mipLevels);

	// Cleanup the staging buffer and its memory
	vkDestroyBuffer(r.c.device, stagingBuffer, nullptr);
	vkFreeMemory(r.c.device, stagingBufferMemory, nullptr);
	r.c.memAllocObjects--;

	return std::pair(textureImage, textureImageMemory);
}

VkImageView TextureLoader::createTextureImageView(VkImage textureImage, uint32_t mipLevels, VulkanCore& c)
{
	#ifdef DEBUG_RESOURCES
		std::cout << "   " << __func__ << std::endl;
	#endif

	VkImageView imageView;
	Image::createImageView(imageView, c, textureImage, imageFormat, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);

	return imageView;
}

VkSampler TextureLoader::createTextureSampler(uint32_t mipLevels, VulkanCore& c)
{
	#ifdef DEBUG_RESOURCES
		std::cout << "   " << __func__ << std::endl;
	#endif

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;					// How to interpolate texels that are magnified (oversampling) or ...
	samplerInfo.minFilter = VK_FILTER_LINEAR;					// ... minified (undersampling). Choices: VK_FILTER_NEAREST, VK_FILTER_LINEAR
	samplerInfo.addressModeU = addressMode;						// Addressing mode per axis (what happens when going beyond the image dimensions). In texture space coordinates, XYZ are UVW. Available values: VK_SAMPLER_ADDRESS_MODE_ ... REPEAT (repeat the texture), MIRRORED_REPEAT (like repeat, but inverts coordinates to mirror the image), CLAMP_TO_EDGE (take the color of the closest edge), MIRROR_CLAMP_TO_EDGE (like clamp to edge, but taking the opposite edge), CLAMP_TO_BORDER (return solid color).
	samplerInfo.addressModeV = addressMode;
	samplerInfo.addressModeW = addressMode;

	if (c.deviceData.samplerAnisotropy)						// If anisotropic filtering is available (see isDeviceSuitable) <<<<<
	{
		samplerInfo.anisotropyEnable = VK_TRUE;							// Specify if anisotropic filtering should be used
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(c.physicalDevice, &properties);
		samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;		// another option:  samplerInfo.maxAnisotropy = 1.0f;
	}
	else
	{
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxAnisotropy = 1.0f;
	}

	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;	// Color returned (black, white or transparent, in format int or float) when sampling beyond the image with VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER. You cannot specify an arbitrary color.
	samplerInfo.unnormalizedCoordinates = VK_FALSE;				// Coordinate system to address texels in an image. False: [0, 1). True: [0, texWidth) & [0, texHeight). 
	samplerInfo.compareEnable = VK_FALSE;						// If a comparison function is enabled, then texels will first be compared to a value, and the result of that comparison is used in filtering operations. This is mainly used for percentage-closer filtering on shadow maps (https://developer.nvidia.com/gpugems/gpugems/part-ii-lighting-and-shadows/chapter-11-shadow-map-antialiasing). 
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;		// VK_SAMPLER_MIPMAP_MODE_ ... NEAREST (lod selects the mip level to sample from), LINEAR (lod selects 2 mip levels to be sampled, and the results are linearly blended)
	samplerInfo.minLod = 0.0f;									// minLod=0 & maxLod=mipLevels allow the full range of mip levels to be used
	samplerInfo.maxLod = static_cast<float>(mipLevels);			// lod: Level Of Detail
	samplerInfo.mipLodBias = 0.0f;								// Used for changing the lod value. It forces to use lower "lod" and "level" than it would normally use

	VkSampler textureSampler;
	Image::createSampler(textureSampler, c, samplerInfo);
	return textureSampler;

	/*
	* VkImage holds the mipmap data. VkSampler controls how that data is read while rendering.
	* The sampler selects a mip level according to this pseudocode:
	*
	*	lod = getLodLevelFromScreenSize();						// Smaller when the object is close (may be negative)
	*	lod = clamp(lod + mipLodBias, minLod, maxLod);
	*
	*	level = clamp(floor(lod), 0, texture.miplevels - 1);	// Clamped to the number of miplevels in the texture
	*
	*	if(mipmapMode == VK_SAMPLER_MIPMAP_MODE_NEAREST)		// Sample operation
	*		color = sampler(level);
	*	else
	*		color = blend(sample(level), sample(level + 1));
	*
	*	if(lod <= 0)											// Filter
	*		color = readTexture(uv, magFilter);
	*	else
	*		color = readTexture(uv, minFilter);
	*/
}

TL_fromBuffer::TL_fromBuffer(const std::string& id, unsigned char* pixels, int texWidth, int texHeight, VkFormat imageFormat, VkSamplerAddressMode addressMode)
	: TextureLoader(id, imageFormat, addressMode), texWidth(texWidth), texHeight(texHeight)
{ 
	size_t size = sizeof(float) * texWidth * texHeight;
	data.resize(size);
	std::copy(pixels, pixels + size, data.data());		// memcpy(data, pixels, size);
}

TL_fromBuffer* TL_fromBuffer::factory(const std::string id, unsigned char* pixels, int texWidth, int texHeight, VkFormat imageFormat, VkSamplerAddressMode addressMode)
{
	return new TL_fromBuffer(id, pixels, texWidth, texHeight, imageFormat, addressMode);
}

TextureLoader* TL_fromBuffer::clone() { return new TL_fromBuffer(*this); }

void TL_fromBuffer::getRawData(unsigned char*& pixels, int32_t& texWidth, int32_t& texHeight) 
{ 
	texWidth  = this->texWidth;
	texHeight = this->texHeight;

	pixels = new unsigned char[data.size()];
	std::copy(data.data(), data.data() + data.size(), pixels);
}

TL_fromFile::TL_fromFile(const std::string& filePath, VkFormat imageFormat, VkSamplerAddressMode addressMode)
	: TextureLoader(filePath, imageFormat, addressMode), filePath(filePath) { }

TL_fromFile* TL_fromFile::factory(const std::string filePath, VkFormat imageFormat, VkSamplerAddressMode addressMode)
{
	return new TL_fromFile(filePath, imageFormat, addressMode);
}

TextureLoader* TL_fromFile::clone() { return new TL_fromFile(*this); }

void TL_fromFile::getRawData(unsigned char*& pixels, int32_t& texWidth, int32_t& texHeight)
{
	int texChannels;
	pixels = stbi_load(filePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);	// Returns a pointer to an array of pixel values. STBI_rgb_alpha forces the image to be loaded with an alpha channel, even if it doesn't have one.
	if (!pixels) throw std::runtime_error("Failed to load texture image!");
}

/*
/// Example of how to use stb_image.h for loading OBJ files
void DataFromFile::loadVertex()
{
	// Load model
	tinyobj::attrib_t					 attrib;			// Holds all of the positions, normals and texture coordinates.
	std::vector<tinyobj::shape_t>		 shapes;			// Holds all of the separate objects and their faces. Each face consists of an array of vertices. Each vertex contains the indices of the position, normal and texture coordinate attributes.
	std::vector<tinyobj::material_t>	 materials;			// OBJ models can also define a material and texture per face, but we will ignore those.
	std::string							 warn, err;			// Errors and warnings that occur while loading the file.

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath))
		throw std::runtime_error(warn + err);

	// Combine all the faces in the file into a single model
	std::unordered_map<VertexPCT, uint32_t> uniqueVertices{};	// Keeps track of the unique vertices and the respective indices, avoiding duplicated vertices (not indices).

	for (const auto& shape : shapes)
		for (const auto& index : shape.mesh.indices)
		{
			// Get each vertex
			VertexPCT vertex{};	// <<< out of the loop better?

			vertex.pos = {
				attrib.vertices[3 * index.vertex_index + 0],			// attrib.vertices is an array of floats, so we need to multiply the index by 3 and add offsets for accessing XYZ components.
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			vertex.texCoord = {
					   attrib.texcoords[2 * index.texcoord_index + 0],	// attrib.texcoords is an array of floats, so we need to multiply the index by 3 and add offsets for accessing UV components.
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1]	// Flip vertical component of texture coordinates: OBJ format assumes Y axis go up, but Vulkan has top-to-bottom orientation.
			};

			vertex.color = { 1.0f, 1.0f, 1.0f };

			// Check if we have already seen this vertex. If not, assign an index to it and save the vertex.
			if (uniqueVertices.count(vertex) == 0)			// Using a user-defined type (Vertex struct) as key in a hash table requires us to implement two functions: equality test (override operator ==) and hash calculation (implement a hash function for Vertex).
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(destVertices->size());	// Set new index for this vertex
				destVertices->push_back(&vertex);										// Save vertex
			}

			// Save the index
			destIndices->push_back(uniqueVertices[vertex]);								// Save index
		}
}
*/


// getOpticalDepthTable -----------------------------------------------------------

/*
	getOpticalDepthTable
		operator ()
			raySphere()
			opticalDepth()
				densityAtPoint()
*/

OpticalDepthTable::OpticalDepthTable(unsigned numOptDepthPoints, unsigned planetRadius, unsigned atmosphereRadius, float heightStep, float angleStep, float densityFallOff)
		: planetRadius(planetRadius), atmosphereRadius(atmosphereRadius), numOptDepthPoints(numOptDepthPoints), heightStep(heightStep), angleStep(angleStep), densityFallOff(densityFallOff)
{
	// Compute useful variables	
	heightSteps = std::ceil(1 + (atmosphereRadius - planetRadius) / heightStep);	// <<<
	angleSteps = std::ceil(1 + 3.141592653589793238462 / angleStep);
	bytes = 4 * heightSteps * angleSteps;	// sizeof(float) = 4
	
	// Get table
	table.resize(bytes);

	float rayLength, angle;
	glm::vec3 point, rayDir;
	float* optDepth = (float*)table.data();

	for (size_t i = 0; i < heightSteps; i++)
	{
		point = { 0, planetRadius + i * heightStep, 0 };	// rayOrigin

		for (size_t j = 0; j < angleSteps; j++)
		{
			angle = j * angleStep;
			rayDir = glm::vec3(sin(angle), cos(angle), 0);
			rayLength = raySphere(point, rayDir).y;

			optDepth[i * angleSteps + j] = opticalDepth(point, rayDir, rayLength);
			
			//if (point.y > 2399 && point.y < 2401 && angle > 1.84 && angle < 1.86)
			//	std::cout << "(" << i << ", " << j << ") / " << point.y << " / " << optDepth[i * angleSteps + j] << " / " << rayLength << " / " << angle << " / (" << rayDir.x << ", " << rayDir.y << ", " << rayDir.z << ")" << std::endl;
		}
	}

	// Compute
	float angleRange = 3.141592653589793238462;
	point = glm::vec3(2400, 0, 0);
	angle = angleRange / 1.7;
	rayDir = glm::vec3(cos(angle), 0, sin(angle));
	rayLength = raySphere(point, rayDir).y;

	// Look up
	float heightRatio = (2400.f - planetRadius) / (atmosphereRadius - planetRadius);
	float angleRatio = angle / angleRange;
	unsigned i = (heightSteps - 1) * heightRatio;
	unsigned j = (angleSteps - 1) * angleRatio;
}

float OpticalDepthTable::opticalDepth(glm::vec3 rayOrigin, glm::vec3 rayDir, float rayLength) const
{
	glm::vec3 point = rayOrigin;
	float stepSize = rayLength / (numOptDepthPoints - 1);
	float opticalDepth = 0;

	for (int i = 0; i < numOptDepthPoints; i++)
	{
		opticalDepth += densityAtPoint(point) * stepSize;
		point += rayDir * stepSize;
	}

	return opticalDepth;
}

float OpticalDepthTable::densityAtPoint(glm::vec3 point) const
{
	float heightAboveSurface = glm::length(point - planetCenter) - planetRadius;
	float height01 = heightAboveSurface / (atmosphereRadius - planetRadius);

	//return exp(-height01 * densityFallOff);					// There is always some density
	return exp(-height01 * densityFallOff) * (1 - height01);	// Density ends at some distance
}

// Returns distance to sphere surface. If it's not, return maximum floating point.
// Returns vector(distToSphere, distThroughSphere). 
//		If rayOrigin is inside sphere, distToSphere = 0. 
//		If ray misses sphere, distToSphere = maxValue; distThroughSphere = 0.
glm::vec2 OpticalDepthTable::raySphere(glm::vec3 rayOrigin, glm::vec3 rayDir) const
{
	// Number of intersections
	glm::vec3 offset = rayOrigin - planetCenter;
	float a = 1;						// Set to dot(rayDir, rayDir) if rayDir might not be normalized
	float b = 2 * dot(offset, rayDir);
	float c = glm::dot(offset, offset) - atmosphereRadius * atmosphereRadius;
	float d = b * b - 4 * a * c;		// Discriminant of quadratic formula (sqrt has 2 solutions/intersections when positive)

	// Two intersections (d > 0)
	if (d > 0)
	{
		float s = sqrt(d);
		float distToSphereNear = std::max(0.f, (-b - s) / (2 * a));
		float distToSphereFar = (-b + s) / (2 * a);

		if (distToSphereFar >= 0)		// Ignore intersections that occur behind the ray
			return glm::vec2(distToSphereNear, distToSphereFar - distToSphereNear);
	}

	// No intersection (d < 0) or one (d = 0)
	return glm::vec2(FLT_MAX, 0);			// https://stackoverflow.com/questions/16069959/glsl-how-to-ensure-largest-possible-float-value-without-overflow

	/*
		/ Line:     y = mx + b
		\ Circle:   r^2 = x^2 + y^2;	y = sqrt(r^2 - x^2)
					r^2 = (x - h)^2 + (y - k)^2;	r^2 = X^2 + x^2 + 2Xx + Y^2 + y^2 + 2Yy

		mx + b = sqrt(r^2 - x^2)
		mmx^2 + b^2 + 2mbx = r^2 - x^2
		mmx^2 + b^2 + 2mbx - r^2 + x^2  = 0
		(mm + 1)x^2 + 2mbx + (b^2 - r^2) = 0 
	*/

	//float m = rayDir.y / rayDir.x;	// line's slope
	//float B = rayOrigin.y;			// line's Y-intercept 
	//float a = m * m + 1;
	//float b = 2 * m * B;
	//float c = B * B - atmosphereRadius * atmosphereRadius;
	//float d = b * b - 4 * a * c;
}

DensityVector::DensityVector(float planetRadius, float atmosphereRadius, float stepSize, float densityFallOff)
{
	heightSteps = std::ceil((atmosphereRadius - planetRadius) / stepSize);
	bytes = 4 * heightSteps;
	table.resize(bytes);

	glm::vec2 point = { 0.f, planetRadius };
	glm::vec2 planetCenter = { 0.f, 0.f };
	float heightAboveSurface;
	float height01;
	float* density = (float*)table.data();

	for (size_t i = 0; i < heightSteps; i++)
	{
		heightAboveSurface = glm::length(point - planetCenter) - planetRadius;
		height01 = heightAboveSurface / (atmosphereRadius - planetRadius);

		//density[i] = std::exp(-height01 * densityFallOff);					// There is always some density
		density[i] = std::exp(-height01 * densityFallOff) * (1 - height01);	// Density ends at some distance

		point.y += stepSize;
	}
}
