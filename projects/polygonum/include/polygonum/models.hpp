#ifndef MODELS_HPP
#define MODELS_HPP

//#include <functional>   // std::function (function wrapper that stores a callable object)
#include <mutex>

#include "polygonum/bindings.hpp"   // buffers & textures
#include "polygonum/vertex.hpp"
#include "polygonum/shader.hpp"

#include <unordered_set>

// Forward declarations ----------

class Renderer;
class RenderPipeline;
class ResourcesLoader;

// Declarations ----------

struct ModelDataInfo;
class ModelData;
class ModelsManager;

// Definitions ----------

#define LINE_WIDTH 1.0f

struct ModelDataInfo
{
	ModelDataInfo();

	const char* name;
	uint32_t numInstances;
	uint32_t maxNumInstances;				//!< Not necessary
	VkPrimitiveTopology topology;			//!< Primitive topology (VK_PRIMITIVE_TOPOLOGY_ ... POINT_LIST, LINE_LIST, LINE_STRIP, TRIANGLE_LIST, TRIANGLE_STRIP). Used when creating the graphics pipeline.
	VertexType vertexType;					//!< VertexType defines the characteristics of a vertex (size and type of the vertex' attributes: Position, Color, Texture coordinates, Normals...).
	VertexesLoader* vertexesLoader;			//!< Info for loading vertices from any source.
	std::vector<ShaderLoader*> shadersInfo;	//!< Shaders info
	std::vector<BindingSet> bindSets;		//!< Sets of bindings
	bool transparency;
	uint32_t renderPassIndex;				//!< 0 (geometry pass), 1 (lighting pass), 2 (forward pass), 3 (postprocessing pass)
	uint32_t subpassIndex;
	VkCullModeFlagBits cullMode;
};

/**
	@class ModelData
	@brief Stores the data directly related to a graphic object. 
	
	Manages vertices, indices, UBOs, textures (pointers), etc.
*/
class ModelData
{
	Renderer* r;
	VkPrimitiveTopology primitiveTopology;		//!< Primitive topology (VK_PRIMITIVE_TOPOLOGY_ ... POINT_LIST, LINE_LIST, LINE_STRIP, TRIANGLE_LIST, TRIANGLE_STRIP). Used when creating the graphics pipeline.
	VertexType vertexType;
	bool hasTransparencies;						//!< Flags if textures contain transparencies (alpha channel)
	VkCullModeFlagBits cullMode;				//!< VK_CULL_MODE_BACK_BIT, VK_CULL_MODE_NONE, ...

	uint32_t numInstances;

	/// Layout for the descriptor set (descriptor: handle or pointer into a resource (buffer, sampler, texture...))
	void createDescriptorSetLayout();

	/// Create the graphics pipeline (sequence of operations that take the vertices and textures of your meshes all the way to the pixels in the render targets).
	void createGraphicsPipeline();

	/// Descriptor pool creation (a descriptor set for each VkBuffer resource to bind it to the uniform buffer descriptor).
	void createDescriptorPool();

	/// Descriptor sets creation.
	void createDescriptorSets();

	/// Delete ResourcesLoader object (no longer required after uploading resources to Vulkan)
	void deleteLoader();

public:
	ModelData(Renderer* renderer = nullptr, ModelDataInfo& modelInfo = ModelDataInfo());   //!< Construct an object for rendering
	ModelData(ModelData&& other) noexcept;   //!< Move constructor: Tansfers resources of a temporary object (rvalue) to another object.
	virtual ~ModelData();
	ModelData& operator=(ModelData&& other) noexcept;   //!< Move assignment operator: Transfers resources from one object to another existing object.

	void fullConstruction(Renderer &ren);   //!< Creates graphic pipeline and descriptor sets, and loads data for creating buffers (vertex, indices, textures). Useful in a second thread

	void cleanup_pipeline_and_descriptors();   //!< Destroys graphic pipeline and descriptor sets. Called by destructor, and for window resizing (by Renderer::recreateSwapChain()::cleanupSwapChain()).
	void recreate_pipeline_and_descriptors();   //!< Creates graphic pipeline and descriptor sets. Called for window resizing (by Renderer::recreateSwapChain()).

	bool setNumInstances(uint32_t count);	//!< Set number of instances to render.
	inline uint32_t getNumInstances() const;

	VkPipelineLayout				pipelineLayout;		//!< Pipeline layout. Allows to use uniform values in shaders (globals similar to dynamic state variables that can be changed at drawing time to alter the behavior of your shaders without having to recreate them).
	VkPipeline						graphicsPipeline;	//!< Opaque handle to a pipeline object.

	std::vector<std::shared_ptr<Shader>>  shaders;		//!< Vertex shader (0), Fragment shader (1)

	VertexData						vert;				//!< Vertex data + Indices

	std::vector<BindingSet>			bindSets;			//!< [set] Set of binding sets (buffers and textures).
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts; //!< [set] Opaque handle to a descriptor set layout object (combines all of the descriptor bindings).
	VkDescriptorPool				descriptorPool;	//!< [set] Opaque handle to a descriptor pool object.
	vec2<VkDescriptorSet>			descriptorSets;		//!< [sc.img][set]. Opaque handle to a descriptor set object. One for each swap chain image.

	uint32_t						renderPassIndex;	//!< Index of the renderPass used (0 for rendering geometry, 1 for post processing)
	uint32_t						subpassIndex;
	//size_t						layer;				//!< Layer where this model will be drawn (Painter's algorithm).

	ResourcesLoader*				resLoader;			//!< Info used for loading resources (vertices, indices, shaders, textures). When resources are loaded, this is set to nullptr.
	bool							fullyConstructed;	//!< Object fully constructed (i.e. model loaded into Vulkan).
	bool							ready;				//!< Object ready for rendering (i.e., it's fully constructed and in Renderer::models)
	std::string						name;				//!< For debugging purposes.
};

class ModelsManager
{
public:
	ModelsManager(const std::shared_ptr<RenderPipeline>& renderPipeline);

	std::unordered_map<key64, ModelData> data;   //!< All models (constructed or not). std::unordered_map uses a hash table. Complexity for lookup, insertion, and deletion: O(1) (average) - O(n) (worst-case)
	vec3<key64> keys;   //!< keys[render pass][subpass][keys]. All keys of all models, distributed per renderpass ad subpass.

	void distributeKeys();   //!< Distribute models per render pass and subpass.
	key64 getNewKey();
	key64 newKey;

	void create_pipelines_and_descriptors(std::mutex* waitMutex);
	void cleanup_pipelines_and_descriptors(std::mutex* waitMutex);
};

/// Helper class used for grouping a set of ModelData objects. 
class ModelSet
{
	Renderer* r;

	uint32_t numInstances;
	uint32_t maxNumInstances;

public:
	ModelSet(Renderer& ren, std::vector<key64> keyList, uint32_t numInstances = 0, uint32_t maxNumInstances = 0);
	ModelSet(Renderer& ren, key64 key, uint32_t numInstances = 0, uint32_t maxNumInstances = 0);

	std::unordered_set<key64> models;   // Set of models. Fast lookup, and insert/erase. Undefined order.

	void setNumInstances(uint32_t count);	//!< Set number of instances to render.
	uint32_t getNumInstances() const;
	bool fullyConstructed();   //!< Object fully constructed (i.e. model loaded into Vulkan).
	bool ready();   //!< Object ready for rendering (i.e., it's fully constructed and in Renderer::models)

	size_t size();
	std::unordered_set<key64>::iterator begin();
	std::unordered_set<key64>::iterator end();
};

#endif