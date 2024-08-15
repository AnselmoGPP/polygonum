#ifndef MODELS_HPP
#define MODELS_HPP

#include <iostream>
#include <array>
#include <functional>						// std::function (function wrapper that stores a callable object)
#include <fstream>
#include <string>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE			// GLM uses OpenGL depth range [-1.0, 1.0]. This macro forces GLM to use Vulkan range [0.0, 1.0].
#define GLM_ENABLE_EXPERIMENTAL				// Required for using std::hash functions for the GLM types (since gtx folder contains experimental extensions)
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>		// Generate transformations matrices with glm::rotate (model), glm::lookAt (view), glm::perspective (projection).
#include <glm/gtx/hash.hpp>

#include "vertex.hpp"
#include "ubo.hpp"
#include "importer.hpp"

#define LINE_WIDTH 1.0f

class ModelData;
typedef std::list<ModelData>::iterator modelIter;

struct ModelDataInfo
{
	ModelDataInfo();

	const char* name;
	size_t activeInstances;						//!< <= maxDescriptorsCount_vs
	VkPrimitiveTopology topology;				//!< Primitive topology (VK_PRIMITIVE_TOPOLOGY_ ... POINT_LIST, LINE_LIST, LINE_STRIP, TRIANGLE_LIST, TRIANGLE_STRIP). Used when creating the graphics pipeline.
	VertexType vertexType;						//!< VertexType defines the characteristics of a vertex (size and type of the vertex' attributes: Position, Color, Texture coordinates, Normals...).
	VerticesLoader* verticesLoader;				//!< Info for loading vertices from any source.
	std::vector<ShaderLoader>* shadersInfo;		//!< Shaders info
	std::vector<TextureLoader>* texturesInfo;	//!< Textures info
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
	
	Manages vertex, indices, UBOs, textures (pointers), etc.
*/
class ModelData
{
	VulkanEnvironment* e;
	const VkPrimitiveTopology primitiveTopology;	    //!< Primitive topology (VK_PRIMITIVE_TOPOLOGY_ ... POINT_LIST, LINE_LIST, LINE_STRIP, TRIANGLE_LIST, TRIANGLE_STRIP). Used when creating the graphics pipeline.
	VertexType vertexType;
	std::vector<shaderIter> shaders;		//!< Vertex shader (0), Fragment shader (1)
	bool hasTransparencies;					//!< Flags if textures contain transparencies (alpha channel)
	VkCullModeFlagBits cullMode;			//!< VK_CULL_MODE_BACK_BIT, VK_CULL_MODE_NONE, ...
	UBO* globalUBO_vs;
	UBO* globalUBO_fs;

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
	/// Construct an object for rendering
	ModelData(VulkanEnvironment& environment, ModelDataInfo& modelInfo);
	virtual ~ModelData();

	/// Creates graphic pipeline and descriptor sets, and loads data for creating buffers (vertex, indices, textures). Useful in a second thread
	ModelData& fullConstruction(std::list<Shader>& shadersList, std::list<Texture>& texturesList, std::mutex& mutResources);

	/// Destroys graphic pipeline and descriptor sets. Called by destructor, and for window resizing (by Renderer::recreateSwapChain()::cleanupSwapChain()).
	void cleanup_Pipeline_Descriptors();

	/// Creates graphic pipeline and descriptor sets. Called for window resizing (by Renderer::recreateSwapChain()).
	void recreate_Pipeline_Descriptors();

	void setActiveInstancesCount(size_t activeInstancesCount);	//!< Set number of active instances (<= vsUBO.maxUBOcount).

	VkPipelineLayout			 pipelineLayout;		//!< Pipeline layout. Allows to use uniform values in shaders (globals similar to dynamic state variables that can be changed at drawing time to alter the behavior of your shaders without having to recreate them).
	VkPipeline					 graphicsPipeline;		//!< Opaque handle to a pipeline object.

	std::vector<texIter>		 textures;				//!< Set of textures used by this model.

	VertexData					 vert;					//!< Vertex data + Indices

	UBO							 vsUBO;					//!< Stores the set of UBOs that will be passed to the vertex shader
	UBO							 fsUBO;					//!< Stores the UBO that will be passed to the fragment shader
	VkDescriptorSetLayout		 descriptorSetLayout;	//!< Opaque handle to a descriptor set layout object (combines all of the descriptor bindings).
	VkDescriptorPool			 descriptorPool;		//!< Opaque handle to a descriptor pool object.
	std::vector<VkDescriptorSet> descriptorSets;		//!< List. Opaque handle to a descriptor set object. One for each swap chain image.

	const uint32_t				 renderPassIndex;		//!< Index of the renderPass used (0 for rendering geometry, 1 for post processing)
	const uint32_t				 subpassIndex;
	size_t						 layer;					//!< Layer where this model will be drawn (Painter's algorithm).
	size_t						 activeInstances;		//!< Number of renderings (<= vsDynUBO.dynBlocksCount). Can be set with setRenderCount.

	ResourcesLoader* resLoader;							//!< Info used for loading resources (vertices, indices, shaders, textures). When resources are loaded, this is set to nullptr.
	bool fullyConstructed;								//!< Flags if this object has been fully constructed (i.e. has a model loaded into Vulkan).
	bool inModels;										//!< Flags if this model is ready for rendering (i.e., if it's in Renderer::models)
	const std::string name;								//!< For debugging purposes.
};

#endif