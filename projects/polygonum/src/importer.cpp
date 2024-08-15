
#include <iostream>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION		// Import textures
#include "stb_image.h"

#include "importer.hpp"
#include "commons.hpp"


const VertexType vt_3   ({ 3 * sizeof(float) }, { VK_FORMAT_R32G32B32_SFLOAT });
const VertexType vt_32  ({ 3 * sizeof(float), 2 * sizeof(float) }, { VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT });
const VertexType vt_33  ({ 3 * sizeof(float), 3 * sizeof(float) }, { VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT });
const VertexType vt_332 ({ 3 * sizeof(float), 3 * sizeof(float), 2 * sizeof(float) }, { VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT });
const VertexType vt_333 ({ 3 * sizeof(float), 3 * sizeof(float), 3 * sizeof(float) }, { VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT });
const VertexType vt_3332({ 3 * sizeof(float), 3 * sizeof(float), 3 * sizeof(float), 2 * sizeof(float) }, { VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT });

std::vector<TextureLoader> noTextures;
std::vector<uint16_t> noIndices;

// RESOURCES --------------------------------------------------------

ResourcesLoader::ResourcesLoader(VerticesLoader& verticesLoader, std::vector<ShaderLoader>& shadersInfo, std::vector<TextureLoader>& texturesInfo, VulkanEnvironment* e)
	: vertices(verticesLoader), shaders(shadersInfo), textures(texturesInfo), e(e) { }

void ResourcesLoader::loadResources(VertexData& destVertexData, std::vector<shaderIter>& destShaders, std::list<Shader>& loadedShaders, std::vector<texIter>& destTextures, std::list<Texture>& loadedTextures, std::mutex& mutResources)
{
	#ifdef DEBUG_RESOURCES
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif
	
	vertices.loadVertices(destVertexData, this, e);
	
	{
		const std::lock_guard<std::mutex> lock(mutResources);
		
		// Load shaders
		for (unsigned i = 0; i < shaders.size(); i++)
		{
			shaderIter iter = shaders[i].loadShader(loadedShaders , *e);
			iter->counter++;
			destShaders.push_back(iter);
		}
		
		// Load textures
		for (unsigned i = 0; i < textures.size(); i++)
		{
			texIter iter = textures[i].loadTexture(loadedTextures, *e);
			iter->counter++;
			destTextures.push_back(iter);
		}
	}
}


// VERTICES --------------------------------------------------------

VLModule::VLModule(size_t vertexSize) : vertexSize(vertexSize) { }

void VLModule::loadVertices(VertexData& result, ResourcesLoader* resources, VulkanEnvironment* e)
{
	VertexSet rawVertices;
	std::vector<uint16_t> rawIndices;
	
	getRawData(rawVertices, rawIndices, *resources);		// Get the raw data
	createBuffers(result, rawVertices, rawIndices, e);		// Upload data to Vulkan
}

void VLModule::createBuffers(VertexData& result, const VertexSet& rawVertices, const std::vector<uint16_t>& rawIndices, VulkanEnvironment* e)
{
	createVertexBuffer(rawVertices, result, e);
	createIndexBuffer(rawIndices, result, e);
}

void VLModule::createVertexBuffer(const VertexSet& rawVertices, VertexData& result, VulkanEnvironment* e)
{
	#ifdef DEBUG_RESOURCES
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif
	
	// Create a staging buffer (host visible buffer used as temporary buffer for mapping and copying the vertex data) (https://vkguide.dev/docs/chapter-5/memory_transfers/)
	VkDeviceSize   bufferSize = rawVertices.totalBytes();	// sizeof(vertices[0])* vertices.size();
	VkBuffer	   stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	createBuffer(
		e,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 											// VK_BUFFER_USAGE_ ... TRANSFER_SRC_BIT / TRANSFER_DST_BIT (buffer can be used as source/destination in a memory transfer operation).
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory);

	// Fill the staging buffer (by mapping the buffer memory into CPU accessible memory: https://en.wikipedia.org/wiki/Memory-mapped_I/O)
	void* data;
	vkMapMemory(e->c.device, stagingBufferMemory, 0, bufferSize, 0, &data);	// Access a memory region. Use VK_WHOLE_SIZE to map all of the memory.
	memcpy(data, rawVertices.data(), (size_t)bufferSize);					// Copy the vertex data to the mapped memory.
	vkUnmapMemory(e->c.device, stagingBufferMemory);						// Unmap memory.

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
	createBuffer(
		e,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		result.vertexBuffer,
		result.vertexBufferMemory);

	result.vertexCount = rawVertices.getNumVertex();

	// Move the vertex data to the device local buffer
	copyBuffer(stagingBuffer, result.vertexBuffer, bufferSize, e);

	// Clean up
	vkDestroyBuffer(e->c.device, stagingBuffer, nullptr);
	vkFreeMemory(e->c.device, stagingBufferMemory, nullptr);
	e->c.memAllocObjects--;
}

void VLModule::createIndexBuffer(const std::vector<uint16_t>& rawIndices, VertexData& result, VulkanEnvironment* e)
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

	createBuffer(
		e,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory);

	// Fill the staging buffer
	void* data;
	vkMapMemory(e->c.device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, rawIndices.data(), (size_t)bufferSize);
	vkUnmapMemory(e->c.device, stagingBufferMemory);

	// Create the vertex buffer
	createBuffer(
		e,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		result.indexBuffer,
		result.indexBufferMemory);

	// Move the vertex data to the device local buffer
	copyBuffer(stagingBuffer, result.indexBuffer, bufferSize, e);

	// Clean up
	vkDestroyBuffer(e->c.device, stagingBuffer, nullptr);
	vkFreeMemory(e->c.device, stagingBufferMemory, nullptr);
	e->c.memAllocObjects--;
}

/**
	@brief Copies some amount of data (size) from srcBuffer into dstBuffer. Used in createVertexBuffer() and createIndexBuffer().

	Memory transfer operations are executed using command buffers (like drawing commands), so we allocate a temporary command buffer. You may wish to create a separate command pool for these kinds of short-lived buffers, because the implementation could apply memory allocation optimizations. You should use the VK_COMMAND_POOL_CREATE_TRANSIENT_BIT flag during command pool generation in that case.
*/
void VLModule::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VulkanEnvironment* e)
{
	#ifdef DEBUG_RESOURCES
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	const std::lock_guard<std::mutex> lock(e->mutCommandPool);
	
	VkCommandBuffer commandBuffer = e->beginSingleTimeCommands();

	// Specify buffers and the size of the contents you will transfer (it's not possible to specify VK_WHOLE_SIZE here, unlike vkMapMemory command).
	VkBufferCopy copyRegion{};
	copyRegion.size = size;
	copyRegion.srcOffset = 0;	// Optional
	copyRegion.dstOffset = 0;	// Optional
	
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	e->endSingleTimeCommands(commandBuffer);
}

glm::vec3 VLModule::getVertexTangent(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec2 uv1, const glm::vec2 uv2, const glm::vec2 uv3)
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

VLM_fromBuffer::VLM_fromBuffer(const void* verticesData, size_t vertexSize, size_t vertexCount, const std::vector<uint16_t>& indices)
	: VLModule(vertexSize)
{
	rawVertices.reset(vertexSize, vertexCount, verticesData);
	rawIndices = indices;
}

VLModule* VLM_fromBuffer::clone() { return new VLM_fromBuffer(*this); }

void VLM_fromBuffer::getRawData(VertexSet& destVertices, std::vector<uint16_t>& destIndices, ResourcesLoader& destResources)
{
	destVertices = rawVertices;
	destIndices = rawIndices;
}

VLM_fromFile::VLM_fromFile(std::string& filePath)
	: VLModule((3+3+2) * sizeof(float)), path(filePath), vertices(nullptr), indices(nullptr), resources(nullptr) { }

VLModule* VLM_fromFile::clone() { return new VLM_fromFile(*this); }

void VLM_fromFile::getRawData(VertexSet& destVertices, std::vector<uint16_t>& destIndices, ResourcesLoader& destResources)
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

void VLM_fromFile::processNode(const aiScene* scene, aiNode* node)
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

void VLM_fromFile::processMeshes(const aiScene* scene, std::vector<aiMesh*> &meshes)
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
					resources->textures.push_back(TextureLoader(fileName.C_Str()));	// Get RESOURCES
					fileName.Clear();
				}
		}
	}

	delete[] vertex;
}


VerticesLoader::VerticesLoader() : loader(nullptr) { }

VerticesLoader::VerticesLoader(std::string& filePath)
	: loader(nullptr)
{ 
	loader = new VLM_fromFile(filePath);
}

VerticesLoader::VerticesLoader(size_t vertexSize, const void* verticesData, size_t vertexCount, std::vector<uint16_t>& indices)
	: loader(nullptr)
{ 
	loader = new VLM_fromBuffer(verticesData, vertexSize, vertexCount, indices);
}

VerticesLoader::VerticesLoader(const VerticesLoader& obj)
{
	if (obj.loader) loader = obj.loader->clone();
	else loader = nullptr;
}

VerticesLoader::~VerticesLoader() 
{
	#ifdef DEBUG_IMPORT
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	if(loader) delete loader; 
}

void VerticesLoader::loadVertices(VertexData& result, ResourcesLoader* resources, VulkanEnvironment* e)
{
	#ifdef DEBUG_RESOURCES
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	loader->loadVertices(result, resources, e);
}


// SHADERS --------------------------------------------------------

Shader::Shader(VulkanEnvironment& e, const std::string id, VkShaderModule shaderModule) 
	: e(e), id(id), counter(0), shaderModule(shaderModule) { }

Shader::~Shader() 
{
	#ifdef DEBUG_IMPORT
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	vkDestroyShaderModule(e.c.device, shaderModule, nullptr); 
}

SLModule::SLModule(const std::string& id, std::vector<ShaderModifier>& modifications)
	: id(id), mods(modifications)
{
	if (mods.size())	// if there're modifiers, name has to change. Otherwise, it's possible that 2 different shaders have same name when the original shader is the same.
	{
		for (ShaderModifier mod : mods)
		{
			this->id += "_" + std::to_string((int)mod.flag);
			
			for(const std::string& str : mod.params)
				this->id += "_" + str;
		}
	}
};

std::list<Shader>::iterator SLModule::loadShader(std::list<Shader>& loadedShaders, VulkanEnvironment* e)
{
	#ifdef DEBUG_RESOURCES
		std::cout << typeid(*this).name() << "::" << __func__ << ": " << this->id << std::endl;
	#endif
	
	// Look for it in loadedShaders
	for (auto i = loadedShaders.begin(); i != loadedShaders.end(); i++)
		if (i->id == id) return i;
	
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
	if (vkCreateShaderModule(e->c.device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		throw std::runtime_error("Failed to create shader module!");
	
	// Create and save shader object
	loadedShaders.emplace(loadedShaders.end(), *e, id, shaderModule);	//loadedShaders.push_back(Shader(e, id, shaderModule));
	return (--loadedShaders.end());
}

void SLModule::applyModifications(std::string& shader)
{
	int count = 0;
	bool found;
	
	for (const ShaderModifier& mod : mods)
	{
		switch (mod.flag)
		{
		case sm_albedo:					// (FS) Sampler used
			found = findTwoAndReplaceBetween(shader, "vec4 albedo", ";",
				"vec4 albedo = texture(texSampler[" + std::to_string(count) + "], inUVs)");
			if (found) count++;
			break;

		case sm_specular:					// (FS) Sampler used
			found = findTwoAndReplaceBetween(shader, "vec3 specular", ";",
				"vec3 specular = texture(texSampler[" + std::to_string(count) + "], inUVs).xyz");
			if (found) count++;
			break;

		case sm_roughness:					// (FS) Sampler used
			found = findTwoAndReplaceBetween(shader, "float roughness", ";",
				"float roughness = texture(texSampler[" + std::to_string(count) + "], inUVs).x");
			if (found) count++;
			break;

		case sm_normal:					// (VS, FS) Sampler used <<< doesn't work yet
			found = findTwoAndReplaceBetween(shader, "vec3 normal", ";",
				"vec3 normal = planarNormal(texSampler[" + std::to_string(count) + "], inUVs, inTB, inNormal, 1)");
			if (found) count++;
			for (int i = 0; i < 3; i++) if (!findStrAndErase(shader, "//normal: ")) break;
			findStrAndReplace(shader, "layout(location = 4) flat", "layout(location = 5) flat");
			std::cout << shader << std::endl;
			std::cout << "---------------" << std::endl;
			break;
			
		case sm_discardAlpha:				// (FS) Discard fragments with low alpha
			findStrAndErase(shader, "//discardAlpha: ");
			break;

		case sm_backfaceNormals:			// (VS) Recalculate normal based on backfacing
			findStrAndErase(shader, "//backfaceNormals: ");
			break;

		case sm_sunfaceNormals:				// (VS) Recalculate normal based on light facing
			findStrAndErase(shader, "//sunfaceNormals: ");
			break;

		case sm_verticalNormals:			// (VS) Make all normals (before MVP transformation) vertical (vec3(0,0,1))
			findStrAndErase(shader, "outNormal = mat3(ubo.normalMatrix) * inNormal;");
			findStrAndErase(shader, "//verticalNormals: ");
			break;

		case sm_waving_weak:				// (VS) Make mesh wave (wind)
			findStrAndErase(shader, "//waving: ");
			findStrAndReplace(shader, "<speed>", "2");
			findStrAndReplace(shader, "<amplitude>", "0.01");
			break;

		case sm_waving_strong:				// (VS) Make mesh wave (wind)
			findStrAndErase(shader, "//waving: ");
			findStrAndReplace(shader, "<speed>", "3");
			findStrAndReplace(shader, "<amplitude>", "0.02");
			break;

		case sm_displace:					// (VS) Move vertex aside a bit (0.2 meter towards x-axis)
			findStrAndErase(shader, "//displace: ");
			break;

		case sm_reduceNightLight:			// (FS) Reduce sunlight at night
			findStrAndErase(shader, "//reduceNightLight: ");
			break;

		case sm_distDithering_near:			// (FS) Apply dithering to distant fragments
			findStrAndErase(shader, "//distDithering: ");
			findStrAndReplace(shader, "near, far", "40, 50");
			break;

		case sm_distDithering_far:			// (FS) Apply dithering to distant fragments
			findStrAndErase(shader, "//distDithering: ");
			findStrAndReplace(shader, "near, far", "100, 130");
			break;

		case sm_earlyDepthTest:				// (FS) Apply Early Depth/Fragment Test
			findStrAndErase(shader, "//earlyDepthTest: ");
			break;

		case sm_dryColor:					// (FS) Apply dry color filter to albedo depending on fragment height
			findStrAndErase(shader, "//dryColor: ");
			break;

		case sm_changeHeader:				// Change header (#include header_path)
			findStrAndReplaceLine(shader, "#include", "#include \"" + mod.params[0] + '\"');
			break;

		case sm_none:
		default:
			break;
		}
	}

	if (count > 0)
		findStrAndReplace(shader, "texSampler[1]", "texSampler[" + std::to_string((int)count) + "]");
}

bool SLModule::findTwoAndReplaceBetween(std::string& text, const std::string& str1, const std::string& str2, const std::string& replacement)
{
	size_t pos1  = text.find(str1, 0);
	size_t pos2 = text.find(str2, pos1);
	if (pos1 == text.npos || pos2 == text.npos) return false;

	text.replace(pos1, pos2 - pos1, (replacement));
	return true;
}

bool SLModule::findStrAndErase(std::string& text, const std::string& str)
{
	size_t pos = text.find(str, 0);
	if (pos == text.npos) return false;

	text.erase(pos, str.size());
	return true;
}

bool SLModule::findStrAndReplace(std::string& text, const std::string& str, const std::string& replacement)
{
	size_t pos = text.find(str, 0);
	if (pos == text.npos) return false;

	text.replace(text.begin() + pos, text.begin() + pos + str.size(), replacement);
	return true;
}

bool SLModule::findStrAndReplaceLine(std::string& text, const std::string& str, const std::string& replacement)
{
	size_t pos = text.find(str, 0);
	if (pos == text.npos) return false;

	size_t eol = text.find('\n', pos) - 1;
	if (eol == text.npos) return false;
	eol++;	// <<< why is this needed? Otherwise, something in the text is messed up (#line)

	text.replace(text.begin() + pos, text.begin() + eol, replacement);
	return true;
}

SLM_fromBuffer::SLM_fromBuffer(const std::string& id, const std::string& glslText, std::vector<ShaderModifier>& modifications)
	: SLModule(id, modifications), data(glslText) { }

SLModule* SLM_fromBuffer::clone() { return new SLM_fromBuffer(*this); }

void SLM_fromBuffer::getRawData(std::string& glslData) { glslData = data; }

SLM_fromFile::SLM_fromFile(const std::string& filePath, std::vector<ShaderModifier>& modifications)
	: SLModule(filePath, modifications), filePath(filePath) { };

SLModule* SLM_fromFile::clone() { return new SLM_fromFile(*this); }

void SLM_fromFile::getRawData(std::string& glslData) { readFile(filePath.c_str(), glslData); }

ShaderLoader::ShaderLoader(const std::string& filePath, std::vector<ShaderModifier>& modifications)
{
	loader = new SLM_fromFile(filePath, modifications);
}

ShaderLoader::ShaderLoader(const std::string& id, const std::string& text, std::vector<ShaderModifier>& modifications)
{
	loader = new SLM_fromBuffer(id, text, modifications);
}

ShaderLoader::ShaderLoader() : loader(nullptr) { }

ShaderLoader::ShaderLoader(const ShaderLoader& obj)
{
	if (obj.loader) loader = obj.loader->clone();
	else loader = nullptr;
}

ShaderLoader::~ShaderLoader()
{ 
	#ifdef DEBUG_IMPORT
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	if (loader) delete loader; 
}

std::list<Shader>::iterator ShaderLoader::loadShader(std::list<Shader>& loadedShaders, VulkanEnvironment& e)
{
	#ifdef DEBUG_RESOURCES
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	return loader->loadShader(loadedShaders, &e);
}


// TEXTURE --------------------------------------------------------

Texture::Texture(VulkanEnvironment& e, const std::string& id, VkImage textureImage, VkDeviceMemory textureImageMemory, VkImageView textureImageView, VkSampler textureSampler)
	: e(e), id(id), counter(0), textureImage(textureImage), textureImageMemory(textureImageMemory), textureImageView(textureImageView), textureSampler(textureSampler) { }

Texture::~Texture()
{
	#ifdef DEBUG_IMPORT
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	vkDestroySampler(e.c.device, textureSampler, nullptr);
	vkDestroyImage(e.c.device, textureImage, nullptr);
	vkDestroyImageView(e.c.device, textureImageView, nullptr);
	vkFreeMemory(e.c.device, textureImageMemory, nullptr);
	e.c.memAllocObjects--;
}

TLModule::TLModule(const std::string& id, VkFormat imageFormat, VkSamplerAddressMode addressMode) 
	: id(id), imageFormat(imageFormat), addressMode(addressMode) { };

std::list<Texture>::iterator TLModule::loadTexture(std::list<Texture>& loadedTextures, VulkanEnvironment* e)
{
	#ifdef DEBUG_RESOURCES
		std::cout << typeid(*this).name() << "::" << __func__ << ": " << this->id << std::endl;
	#endif
	
	this->e = e;
	
	// Look for it in loadedShaders
	for (auto i = loadedTextures.begin(); i != loadedTextures.end(); i++)
		if (i->id == id) return i;
	
	// Load an image
	unsigned char* pixels;
	int32_t texWidth, texHeight;
	getRawData(pixels, texWidth, texHeight);
	
	// Get arguments for creating the texture object
	uint32_t mipLevels;		//!< Number of levels (mipmaps)
	std::pair<VkImage, VkDeviceMemory> image = createTextureImage(pixels, texWidth, texHeight, mipLevels);
	VkImageView textureImageView             = createTextureImageView(std::get<VkImage>(image), mipLevels);
	VkSampler textureSampler                 = createTextureSampler(mipLevels);
	
	// Create and save texture object
	loadedTextures.emplace(loadedTextures.end(), *e, id, std::get<VkImage>(image), std::get<VkDeviceMemory>(image), textureImageView, textureSampler);		//loadedTextures.push_back(texture);
	return (--loadedTextures.end());
}

std::pair<VkImage, VkDeviceMemory> TLModule::createTextureImage(unsigned char* pixels, int32_t texWidth, int32_t texHeight, uint32_t& mipLevels)
{
	#ifdef DEBUG_RESOURCES
		std::cout << "   " << __func__ << std::endl;
	#endif

	VkDeviceSize imageSize = texWidth * texHeight * 4;												// 4 bytes per rgba pixel
	mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;	// Calculate the number levels (mipmaps)
	
	// Create a staging buffer (temporary buffer in host visible memory so that we can use vkMapMemory and copy the pixels to it)
	VkBuffer	   stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	createBuffer(
		e,
		imageSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory);
	
	// Copy directly the pixel values from the image we loaded to the staging-buffer.
	void* data;
	vkMapMemory(e->c.device, stagingBufferMemory, 0, imageSize, 0, &data);	// vkMapMemory retrieves a host virtual address pointer (data) to a region of a mappable memory object (stagingBufferMemory). We have to provide the logical device that owns the memory (e.device).
	memcpy(data, pixels, static_cast<size_t>(imageSize));					// Copies a number of bytes (imageSize) from a source (pixels) to a destination (data).
	vkUnmapMemory(e->c.device, stagingBufferMemory);						// Unmap a previously mapped memory object (stagingBufferMemory).
	stbi_image_free(pixels);	// Clean up the original pixel array
	
	// Create the texture image
	VkImage			textureImage;
	VkDeviceMemory	textureImageMemory;

	e->createImage(
		texWidth, texHeight,
		mipLevels,
		VK_SAMPLE_COUNT_1_BIT,
		imageFormat,			// VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R64_SFLOAT
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		textureImage,
		textureImageMemory);

	// Copy the staging buffer to the texture image
	e->transitionImageLayout(textureImage, imageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);					// Transition the texture image to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));											// Execute the buffer to image copy operation
	// Transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps
	// transitionImageLayout(textureImage, imageFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels);	// To be able to start sampling from the texture image in the shader, we need one last transition to prepare it for shader access
	generateMipmaps(textureImage, imageFormat, texWidth, texHeight, mipLevels);

	// Cleanup the staging buffer and its memory
	vkDestroyBuffer(e->c.device, stagingBuffer, nullptr);
	vkFreeMemory(e->c.device, stagingBufferMemory, nullptr);
	e->c.memAllocObjects--;

	return std::pair(textureImage, textureImageMemory);
}

VkImageView TLModule::createTextureImageView(VkImage textureImage, uint32_t mipLevels)
{
	#ifdef DEBUG_RESOURCES
		std::cout << "   " << __func__ << std::endl;
	#endif

	return e->createImageView(textureImage, imageFormat, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
}

VkSampler TLModule::createTextureSampler(uint32_t mipLevels)
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

	if (e->c.deviceData.samplerAnisotropy)						// If anisotropic filtering is available (see isDeviceSuitable) <<<<<
	{
		samplerInfo.anisotropyEnable = VK_TRUE;							// Specify if anisotropic filtering should be used
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(e->c.physicalDevice, &properties);
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
	if (vkCreateSampler(e->c.device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS)
		throw std::runtime_error("Failed to create texture sampler!");
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

void TLModule::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
	const std::lock_guard<std::mutex> lock(e->mutCommandPool);

	VkCommandBuffer commandBuffer = e->beginSingleTimeCommands();

	// Specify which part of the buffer is going to be copied to which part of the image
	VkBufferImageCopy region{};
	region.bufferOffset = 0;							// Byte offset in the buffer at which the pixel values start
	region.bufferRowLength = 0;							// How the pixels are laid out in memory. 0 indicates that the pixels are thightly packed. Otherwise, you could have some padding bytes between rows of the image, for example. 
	region.bufferImageHeight = 0;							// How the pixels are laid out in memory. 0 indicates that the pixels are thightly packed. Otherwise, you could have some padding bytes between rows of the image, for example.
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;	// imageSubresource indicate to which part of the image we want to copy the pixels
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { 0, 0, 0 };					// Indicate to which part of the image we want to copy the pixels
	region.imageExtent = { width, height, 1 };			// Indicate to which part of the image we want to copy the pixels

	// Enqueue buffer to image copy operations
	vkCmdCopyBufferToImage(
		commandBuffer,
		buffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// Layout the image is currently using
		1,
		&region);

	e->endSingleTimeCommands(commandBuffer);
}

void TLModule::generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
	// Check if the image format supports linear blitting. We are using vkCmdBlitImage, but it's not guaranteed to be supported on all platforms because it requires our texture image format to support linear filtering, so we check it with vkGetPhysicalDeviceFormatProperties.
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(e->c.physicalDevice, imageFormat, &formatProperties);
	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
	{
		throw std::runtime_error("Texture image format does not support linear blitting!");
		// Two alternatives:
		//		- Implement a function that searches common texture image formats for one that does support linear blitting.
		//		- Implement the mipmap generation in software with a library like stb_image_resize. Each mip level can then be loaded into the image in the same way that you loaded the original image.
		// It's uncommon to generate the mipmap levels at runtime anyway. Usually they are pregenerated and stored in the texture file alongside the base level to improve loading speed. <<<<<
	}
	
	const std::lock_guard<std::mutex> lock(e->mutCommandPool);

	VkCommandBuffer commandBuffer = e->beginSingleTimeCommands();

	// Specify the barriers
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = texWidth;
	int32_t mipHeight = texHeight;

	for (uint32_t i = 1; i < mipLevels; i++)	// This loop records each of the VkCmdBlitImage commands. The source mip level is i - 1 and the destination mip level is i.
	{
		// 1. Record a barrier (we transition level i - 1 to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL. This transition will wait for level i - 1 to be filled, either from the previous blit command, or from vkCmdCopyBufferToImage. The current blit command will wait on this transition).
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;	// We transition level i - 1 to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL. This transition will wait for level i - 1 to be filled, either from the previous blit command, or from vkCmdCopyBufferToImage. The current blit command will wait on this transition.
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		// 2. Record a blit command. Beware if you are using a dedicated transfer queue: vkCmdBlitImage must be submitted to a queue with graphics capability.		
		VkImageBlit blit{};										// Specify the regions that will be used in the blit operation
		blit.srcOffsets[0] = { 0, 0, 0 };						// srcOffsets determine the 3D regions ...
		blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };		// ... that data will be blitted from.
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };																	// dstOffsets determine the 3D region ...
		blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1,  mipHeight > 1 ? mipHeight / 2 : 1,  1 };	// ... that data will be blitted to.
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		vkCmdBlitImage(commandBuffer,
			image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// The textureImage is used for both the srcImage and dstImage parameter ...
			image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// ...  because we're blitting between different levels of the same image.
			1, &blit,
			VK_FILTER_LINEAR);									// Enable interpolation

		// 3. Record a barrier (This barrier transitions mip level i - 1 to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL. This transition waits on the current blit command to finish. All sampling operations will wait on this transition to finish).
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
	}

	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	// n. Record a barrier (This barrier transitions the last mip level from VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL. This wasn't handled by the loop, since the last mip level is never blitted from).
	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		0, nullptr,
		0, nullptr,
		1, &barrier);

	e->endSingleTimeCommands(commandBuffer);
}

TLM_fromBuffer::TLM_fromBuffer(const std::string& id, unsigned char* pixels, int texWidth, int texHeight, VkFormat imageFormat, VkSamplerAddressMode addressMode)
	: TLModule(id, imageFormat, addressMode), texWidth(texWidth), texHeight(texHeight)
{ 
	size_t size = sizeof(float) * texWidth * texHeight;
	data.resize(size);
	std::copy(pixels, pixels + size, data.data());		// memcpy(data, pixels, size);
}

TLModule* TLM_fromBuffer::clone() { return new TLM_fromBuffer(*this); }

void TLM_fromBuffer::getRawData(unsigned char*& pixels, int32_t& texWidth, int32_t& texHeight) 
{ 
	texWidth  = this->texWidth;
	texHeight = this->texHeight;

	pixels = new unsigned char[data.size()];
	std::copy(data.data(), data.data() + data.size(), pixels);
}

TLM_fromFile::TLM_fromFile(const std::string& filePath, VkFormat imageFormat, VkSamplerAddressMode addressMode)
	: TLModule(filePath, imageFormat, addressMode), filePath(filePath) { }

TLModule* TLM_fromFile::clone() { return new TLM_fromFile(*this); }

void TLM_fromFile::getRawData(unsigned char*& pixels, int32_t& texWidth, int32_t& texHeight)
{
	int texChannels;
	pixels = stbi_load(filePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);	// Returns a pointer to an array of pixel values. STBI_rgb_alpha forces the image to be loaded with an alpha channel, even if it doesn't have one.
	if (!pixels) throw std::runtime_error("Failed to load texture image!");
}

TextureLoader::TextureLoader(std::string filePath, VkFormat imageFormat , VkSamplerAddressMode addressMode)
{ 
	loader = new TLM_fromFile(filePath, imageFormat, addressMode);
}

TextureLoader::TextureLoader(unsigned char* pixels, int texWidth, int texHeight, std::string id, VkFormat imageFormat, VkSamplerAddressMode addressMode)
{
	loader = new TLM_fromBuffer(id, pixels, texWidth, texHeight, imageFormat, addressMode);
}

TextureLoader::TextureLoader() : loader(nullptr) { }

TextureLoader::TextureLoader(const TextureLoader& obj)
{
	if (obj.loader) loader = obj.loader->clone();
	else loader = nullptr;
}

TextureLoader::~TextureLoader() 
{
	#ifdef DEBUG_IMPORT
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	if (loader) delete loader;
}

std::list<Texture>::iterator TextureLoader::loadTexture(std::list<Texture>& loadedTextures, VulkanEnvironment& e)
{
	#ifdef DEBUG_RESOURCES
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	return loader->loadTexture(loadedTextures, &e);
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
