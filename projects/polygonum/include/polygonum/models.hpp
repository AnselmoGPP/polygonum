#ifndef MODELS_HPP
#define MODELS_HPP

//#include <functional>						// std::function (function wrapper that stores a callable object)

#include "polygonum/vertex.hpp"
#include "polygonum/ubo.hpp"
#include "polygonum/importer.hpp"
#include "polygonum/commons.hpp"

#define LINE_WIDTH 1.0f


class Renderer;
class VulkanEnvironment;
class RenderPipeline;

struct ModelDataInfo
{
	ModelDataInfo();

	const char* name;
	size_t activeInstances;						//!< <= maxDescriptorsCount_vs
	VkPrimitiveTopology topology;				//!< Primitive topology (VK_PRIMITIVE_TOPOLOGY_ ... POINT_LIST, LINE_LIST, LINE_STRIP, TRIANGLE_LIST, TRIANGLE_STRIP). Used when creating the graphics pipeline.
	VertexType vertexType;						//!< VertexType defines the characteristics of a vertex (size and type of the vertex' attributes: Position, Color, Texture coordinates, Normals...).
	VertexesLoader* vertexesLoader;				//!< Info for loading vertices from any source.
	std::vector<ShaderLoader*> shadersInfo;		//!< Shaders info
	std::vector<TextureLoader*> texturesInfo;	//!< Textures info
	size_t maxDescriptorsCount_vs;				//!< Max. number of active instances
	size_t maxDescriptorsCount_fs;
	size_t UBOsize_vs;
	size_t UBOsize_fs;
	UBO* globalUBO_vs;
	UBO* globalUBO_fs;
	bool transparency;
	uint32_t renderPassIndex;					//!< 0 (geometry pass), 1 (lighting pass), 2 (forward pass), 3 (postprocessing pass)
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
	VulkanEnvironment* e;
	VkPrimitiveTopology primitiveTopology;		//!< Primitive topology (VK_PRIMITIVE_TOPOLOGY_ ... POINT_LIST, LINE_LIST, LINE_STRIP, TRIANGLE_LIST, TRIANGLE_STRIP). Used when creating the graphics pipeline.
	VertexType vertexType;
	bool hasTransparencies;						//!< Flags if textures contain transparencies (alpha channel)
	VkCullModeFlagBits cullMode;				//!< VK_CULL_MODE_BACK_BIT, VK_CULL_MODE_NONE, ...
	UBO* globalUBO_vs;
	UBO* globalUBO_fs;
	size_t activeInstances;		//!< Number of renderings (<= vsDynUBO.dynBlocksCount). Can be set with setRenderCount.

	// Main methods:

	/// Layout for the descriptor set (descriptor: handle or pointer into a resource (buffer, sampler, texture...))
	void createDescriptorSetLayout();

	/**
		@brief Create the graphics pipeline.

		Graphics pipeline: Sequence of operations that take the vertices and textures of your meshes all the way to the pixels in the render targets. Stages (F: fixed-function stages, P: programable):
			<ul>
				<li>Vertex/Index buffer: Raw vertex data.</li>
				<li>Input assembler (F): Collects data from the buffers and may use an index buffer to repeat certain elements without duplicating the vertex data.</li>
				<li>Vertex shader (P): Run for every vertex. Generally, applies transformations to turn vertex positions from model space to screen space. Also passes per-vertex data down the pipeline.</li>
				<li>Tessellation shader (P): Subdivides geometry based on certain rules to increase mesh quality (example: make brick walls look less flat from nearby).</li>
				<li>Geometry shader (P): Run for every primitive (triangle, line, point). It can discard the primitive or output more new primitives. Similar to tessellation shader, more flexible but with worse performance.</li>
				<li>Rasterization (F): Discretizes primitives into fragments (pixel elements that fill the framebuffer). Attributes outputted by the vertex shaders are interpolated across fragments. Fragments falling outside the screen are discarded. Usually, fragments behind others are discarded (depth testing).</li>
				<li>Fragment shader (P): Run for every surviving fragment. Determines which framebuffer/s the fragments are written to and with which color and depth values (uses interpolated data from vertex shader, and may include things like texture coordinates, normals for lighting).</li>
				<li>Color blending (F): Mixes different fragments that map to the same pixel in the framebuffer (overwrite each other, add up, or mix based upon transparency).</li>
				<li>Framebuffer.</li>
			</ul>
		Some programmable stages are optional (example: tessellation and geometry stages).
		In Vulkan, the graphics pipeline is almost completely immutable. You will have to create a number of pipelines representing all of the different combinations of states you want to use.
	*/
	void createGraphicsPipeline();

	/// Descriptor pool creation (a descriptor set for each VkBuffer resource to bind it to the uniform buffer descriptor).
	void createDescriptorPool();

	/// Descriptor sets creation.
	void createDescriptorSets();

	/// Clear descriptor sets, vertex and indices. Called by destructor.
	void cleanup();

	/// Delete ResourcesLoader object (no longer required after uploading resources to Vulkan)
	void deleteLoader();

public:
	ModelData(VulkanEnvironment* environment = nullptr, ModelDataInfo& modelInfo = ModelDataInfo());   //!< Construct an object for rendering
	virtual ~ModelData();
	ModelData(ModelData&& other) noexcept;   //!< Move constructor: Tansfers resources of a temporary object (rvalue) to another object.
	ModelData& ModelData::operator=(ModelData&& other) noexcept;   //!< Move assignment operator: Transfers resources from one object to another existing object.

	/// Creates graphic pipeline and descriptor sets, and loads data for creating buffers (vertex, indices, textures). Useful in a second thread
	ModelData& fullConstruction(Renderer &rend);

	/// Destroys graphic pipeline and descriptor sets. Called by destructor, and for window resizing (by Renderer::recreateSwapChain()::cleanupSwapChain()).
	void cleanup_Pipeline_Descriptors();

	/// Creates graphic pipeline and descriptor sets. Called for window resizing (by Renderer::recreateSwapChain()).
	void recreate_Pipeline_Descriptors();

	size_t getActiveInstancesCount();
	bool setActiveInstancesCount(size_t activeInstancesCount);	//!< Set number of active instances (<= vsUBO.maxUBOcount).

	VkPipelineLayout			 pipelineLayout;		//!< Pipeline layout. Allows to use uniform values in shaders (globals similar to dynamic state variables that can be changed at drawing time to alter the behavior of your shaders without having to recreate them).
	VkPipeline					 graphicsPipeline;		//!< Opaque handle to a pipeline object.

	std::vector<std::shared_ptr<Texture>> textures;		//!< Set of textures used by this model.
	std::vector<std::shared_ptr<Shader>> shaders;		//!< Vertex shader (0), Fragment shader (1)

	VertexData					 vert;					//!< Vertex data + Indices

	UBO							 vsUBO;					//!< Stores the set of UBOs that will be passed to the vertex shader
	UBO							 fsUBO;					//!< Stores the UBO that will be passed to the fragment shader
	VkDescriptorSetLayout		 descriptorSetLayout;	//!< Opaque handle to a descriptor set layout object (combines all of the descriptor bindings).
	VkDescriptorPool			 descriptorPool;		//!< Opaque handle to a descriptor pool object.
	std::vector<VkDescriptorSet> descriptorSets;		//!< List. Opaque handle to a descriptor set object. One for each swap chain image.

	uint32_t					 renderPassIndex;		//!< Index of the renderPass used (0 for rendering geometry, 1 for post processing)
	uint32_t					 subpassIndex;
	size_t						 layer;					//!< Layer where this model will be drawn (Painter's algorithm).

	ResourcesLoader*			 resLoader;				//!< Info used for loading resources (vertices, indices, shaders, textures). When resources are loaded, this is set to nullptr.
	bool						 fullyConstructed;		//!< Object fully constructed (i.e. model loaded into Vulkan).
	bool						 ready;					//!< Object ready for rendering (i.e., it's fully constructed and in Renderer::models)
	std::string					 name;					//!< For debugging purposes.
};

class ModelsManager
{
public:
	ModelsManager(const std::shared_ptr<RenderPipeline>& renderPipeline);

	std::unordered_map<key64, ModelData> data;   //!< All models (constructed or not). std::unordered_map uses a hash table. Complexity for lookup, insertion, and deletion: O(1) (average) - O(n) (worst-case)
	vec3<key64> keys;   //!< keys[render pass][subpass][keys]. All keys of all models, distributed per renderpass ad subpass.

	void distributeKeys();
	key64 getNewKey();
	key64 newKey;
};

#endif