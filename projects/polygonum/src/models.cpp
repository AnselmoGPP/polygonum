#include <iostream>

#include "polygonum/models.hpp"
#include "polygonum/renderer.hpp"


ModelDataInfo::ModelDataInfo()
	: name("noName"),
	activeInstances(0),
	topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST),
	vertexType(vt_332),
	vertexesLoader(nullptr),
	shadersInfo(std::vector<ShaderLoader*>()),
	texturesInfo(std::vector<TextureLoader*>()),
	maxDescriptorsCount_vs(0),
	maxDescriptorsCount_fs(0),
	UBOsize_vs(0),
	UBOsize_fs(0),
	globalUBO_vs(nullptr),
	globalUBO_fs(nullptr),
	transparency(false),
	renderPassIndex(0),
	subpassIndex(0),
	cullMode(VK_CULL_MODE_BACK_BIT)
{ }


ModelData::ModelData(VulkanEnvironment* environment, ModelDataInfo& modelInfo)
	: e(environment),
	name(modelInfo.name),
	primitiveTopology(modelInfo.topology),
	vertexType(modelInfo.vertexType),
	hasTransparencies(modelInfo.transparency),
	cullMode(modelInfo.cullMode),
	globalUBO_vs(modelInfo.globalUBO_vs),
	globalUBO_fs(modelInfo.globalUBO_fs),
	vsUBO(e, UBOinfo(modelInfo.maxDescriptorsCount_vs, modelInfo.activeInstances, modelInfo.UBOsize_vs)),
	fsUBO(e, UBOinfo(modelInfo.maxDescriptorsCount_fs, modelInfo.maxDescriptorsCount_fs, modelInfo.UBOsize_fs)),
	renderPassIndex(modelInfo.renderPassIndex),
	subpassIndex(modelInfo.subpassIndex),
	activeInstances(modelInfo.activeInstances),
	fullyConstructed(false),
	ready(false)
{
	#ifdef DEBUG_MODELS
		std::cout << typeid(*this).name() << "::" << __func__ << " (" << name << ')' << std::endl;
	#endif

	resLoader = new ResourcesLoader(modelInfo.vertexesLoader, modelInfo.shadersInfo, modelInfo.texturesInfo);
}

ModelData::~ModelData()
{
	#ifdef DEBUG_MODELS
		std::cout << typeid(*this).name() << "::" << __func__ << " (" << name << ')' << std::endl;
	#endif

	if (fullyConstructed) {
		cleanup_Pipeline_Descriptors();
		cleanup();
	}

	deleteLoader();
}

ModelData::ModelData(ModelData&& other) noexcept
	: e(other.e),
	primitiveTopology(other.primitiveTopology),
	hasTransparencies(other.hasTransparencies),
	cullMode(other.cullMode),
	globalUBO_vs(other.globalUBO_vs),
	globalUBO_fs(other.globalUBO_fs),
	pipelineLayout(other.pipelineLayout),
	graphicsPipeline(other.graphicsPipeline),
	descriptorSetLayout(other.descriptorSetLayout),
	descriptorPool(other.descriptorPool),
	renderPassIndex(other.renderPassIndex),
	subpassIndex(other.subpassIndex),
	layer(other.layer),
	activeInstances(other.activeInstances),
	resLoader(other.resLoader),
	fullyConstructed(other.fullyConstructed),
	ready(other.ready),
	name(other.name)
{
	vertexType = std::move(other.vertexType);
	shaders = std::move(other.shaders);
	textures = std::move(other.textures);
	vert = std::move(other.vert);
	vsUBO = std::move(other.vsUBO);
	fsUBO = std::move(other.fsUBO);
	descriptorSets = std::move(other.descriptorSets);

	other.e = nullptr;
	other.globalUBO_vs = nullptr;
	other.globalUBO_fs = nullptr;
	other.resLoader = nullptr;
}

ModelData& ModelData::operator=(ModelData&& other) noexcept
{
	if (this != &other)
	{
		// Free existing resources
		deleteLoader();

		// Transfer resources ownership
		e = other.e;
		primitiveTopology = other.primitiveTopology;
		hasTransparencies = other.hasTransparencies;
		cullMode = other.cullMode;
		globalUBO_vs = other.globalUBO_vs;
		globalUBO_fs = other.globalUBO_fs;
		pipelineLayout = other.pipelineLayout;
		graphicsPipeline = other.graphicsPipeline;
		descriptorSetLayout = other.descriptorSetLayout;
		descriptorPool = other.descriptorPool;
		renderPassIndex = other.renderPassIndex;
		subpassIndex = other.subpassIndex;
		layer = other.layer;
		activeInstances = other.activeInstances;
		resLoader = other.resLoader;
		fullyConstructed = other.fullyConstructed;
		ready = other.ready;

		vertexType = std::move(other.vertexType);
		shaders = std::move(other.shaders);
		textures = std::move(other.textures);
		vert = std::move(other.vert);
		vsUBO = std::move(other.vsUBO);
		fsUBO = std::move(other.fsUBO);
		descriptorSets = std::move(other.descriptorSets);
		name = std::move(other.name);

		// Leave other in valid state
		other.e = nullptr;
		other.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
		other.hasTransparencies = false;
		other.cullMode = VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
		other.globalUBO_vs = nullptr;
		other.globalUBO_fs = nullptr;
		other.pipelineLayout = other.pipelineLayout;
		other.graphicsPipeline = other.graphicsPipeline;
		other.descriptorSetLayout = other.descriptorSetLayout;
		other.descriptorPool = other.descriptorPool;
		other.renderPassIndex = 0;
		other.subpassIndex = 0;
		other.layer = 0;
		other.activeInstances = 0;
		other.resLoader = nullptr;
		other.fullyConstructed = false;
		other.ready = false;
		other.name = "";
		
		other.vertexType = VertexType();
		other.shaders.clear();
		other.textures.clear();
		other.vert = VertexData();
		other.vsUBO = UBO();
		other.fsUBO = UBO();
		other.descriptorSets.clear();
	}
	return *this;
}

ModelData& ModelData::fullConstruction(Renderer& rend)
{
	#ifdef DEBUG_MODELS
		std::cout << typeid(*this).name() << "::" << __func__ << " (" << name << ')' << std::endl;
	#endif
	
	if (resLoader) {
		resLoader->loadResources(*this, rend);
		deleteLoader();
	} else std::cout << "Error: No loading info data" << std::endl;
	
	createDescriptorSetLayout();
	createGraphicsPipeline();
	
	vsUBO.createUBO();
	fsUBO.createUBO();
	createDescriptorPool();
	createDescriptorSets();
	
	fullyConstructed = true;
	return *this;
}

// (9)
void ModelData::createDescriptorSetLayout()
{
	#ifdef DEBUG_MODELS
		std::cout << typeid(*this).name() << "::" << __func__ << " (" << name << ')' << std::endl;
	#endif

	std::vector<VkDescriptorSetLayoutBinding> bindings;
	uint32_t bindNumber = 0;

	// 0) Global UBO (vertex shader)
	if (globalUBO_vs && globalUBO_vs->subUboSize)
	{
		VkDescriptorSetLayoutBinding vsGlobalUboLayoutBinding{};
		vsGlobalUboLayoutBinding.binding = bindNumber++;
		vsGlobalUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vsGlobalUboLayoutBinding.descriptorCount = globalUBO_vs->maxNumSubUbos;
		vsGlobalUboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		vsGlobalUboLayoutBinding.pImmutableSamplers = nullptr;
		
		bindings.push_back(vsGlobalUboLayoutBinding);
	}

	//	1) Uniform buffer descriptor (vertex shader)
	if (vsUBO.subUboSize)
	{
		VkDescriptorSetLayoutBinding vsUboLayoutBinding{};
		vsUboLayoutBinding.binding = bindNumber++;
		vsUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;			// VK_DESCRIPTOR_TYPE_ ... UNIFORM_BUFFER, UNIFORM_BUFFER_DYNAMIC
		vsUboLayoutBinding.descriptorCount = vsUBO.maxNumSubUbos;						// In case you want to specify an array of UBOs <<< (example: for specifying a transformation for each bone in a skeleton for skeletal animation).
		vsUboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;						// Tell in which shader stages the descriptor will be referenced. This field can be a combination of VkShaderStageFlagBits values or the value VK_SHADER_STAGE_ALL_GRAPHICS.
		vsUboLayoutBinding.pImmutableSamplers = nullptr;								// [Optional] Only relevant for image sampling related descriptors.
		
		bindings.push_back(vsUboLayoutBinding);
	}

	// 2) Global UBO (fragment shader)
	if (globalUBO_fs && globalUBO_fs->subUboSize)
	{
		VkDescriptorSetLayoutBinding fsGlobalUboLayoutBinding{};
		fsGlobalUboLayoutBinding.binding = bindNumber++;
		fsGlobalUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		fsGlobalUboLayoutBinding.descriptorCount = globalUBO_fs->maxNumSubUbos;
		fsGlobalUboLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		fsGlobalUboLayoutBinding.pImmutableSamplers = nullptr;
		
		bindings.push_back(fsGlobalUboLayoutBinding);
	}

	// 3) Uniform buffer descriptor (fragment shader)
	if (fsUBO.subUboSize)
	{
		VkDescriptorSetLayoutBinding fsUboLayoutBinding{};
		fsUboLayoutBinding.binding = bindNumber++;
		fsUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		fsUboLayoutBinding.descriptorCount = fsUBO.maxNumSubUbos;
		fsUboLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		fsUboLayoutBinding.pImmutableSamplers = nullptr;
		
		bindings.push_back(fsUboLayoutBinding);
	}

	//	4) Combined image sampler descriptor (set of textures) (it lets shaders access an image resource through a sampler object)
	if (textures.size())
	{
		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding = bindNumber++;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.descriptorCount = textures.size();
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;			// We want to use the combined image sampler descriptor in the fragment shader. It's possible to use texture sampling in the vertex shader (example: to dynamically deform a grid of vertices by a heightmap).
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		
		bindings.push_back(samplerLayoutBinding);
	}

	// 5) Input attachments
	if(e->rp->getSubpass(renderPassIndex, subpassIndex).inputAtts.size())
	{
		VkDescriptorSetLayoutBinding inputAttachmentLayoutBinding{};
		inputAttachmentLayoutBinding.binding = bindNumber++;
		inputAttachmentLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;	// VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		inputAttachmentLayoutBinding.descriptorCount = e->rp->getSubpass(renderPassIndex, subpassIndex).inputAtts.size();
		inputAttachmentLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		inputAttachmentLayoutBinding.pImmutableSamplers = nullptr;

		bindings.push_back(inputAttachmentLayoutBinding);
	}

	// Create a descriptor set layout (combines all of the descriptor bindings)
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();
	
	if (vkCreateDescriptorSetLayout(e->c.device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
		throw std::runtime_error("Failed to create descriptor set layout!");
}

// (10)
void ModelData::createGraphicsPipeline()
{
	#ifdef DEBUG_MODELS
		std::cout << typeid(*this).name() << "::" << __func__ << " (" << name << ')' << std::endl;
	#endif

	// Create pipeline layout   <<< sameMod
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;						// Optional
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;		// Optional	 <<<<<
	pipelineLayoutInfo.pushConstantRangeCount = 0;				// Optional. <<< Push constants are another way of passing dynamic values to shaders.
	pipelineLayoutInfo.pPushConstantRanges = nullptr;			// Optional

	if (vkCreatePipelineLayout(e->c.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
		throw std::runtime_error("Failed to create pipeline layout!");
	
	// Read shader files
	//std::vector<char> vertShaderCode = readFile(VSpath);
	//std::vector<char> fragShaderCode = readFile(FSpath);
	//VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
	//VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);
	
	// Configure Vertex shader
	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = shaders[0]->shaderModule;
	vertShaderStageInfo.pName = "main";						// Function to invoke (entrypoint). You may combine multiple fragment shaders into a single shader module and use different entry points (different behaviors).  
	vertShaderStageInfo.pSpecializationInfo = nullptr;		// Optional. Specifies values for shader constants.
	
	// Configure Fragment shader
	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = shaders[1]->shaderModule;
	fragShaderStageInfo.pName = "main";
	fragShaderStageInfo.pSpecializationInfo = nullptr;

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };
	
	// Vertex input: Describes format of the vertex data that will be passed to the vertex shader.
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	auto bindingDescription = vertexType.getBindingDescription();
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;							// Optional
	auto attributeDescriptions = vertexType.getAttributeDescriptions();
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();				// Optional

	// Input assembly: Describes what kind of geometry will be drawn from the vertices, and if primitive restart should be enabled.
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = primitiveTopology;		// VK_PRIMITIVE_TOPOLOGY_ ... POINT_LIST, LINE_LIST, LINE_STRIP, TRIANGLE_LIST, TRIANGLE_STRIP
	inputAssembly.primitiveRestartEnable = VK_FALSE;					// If VK_TRUE, then it's possible to break up lines and triangles in the _STRIP topology modes by using a special index of 0xFFFF or 0xFFFFFFFF.

	// Viewport: Describes the region of the framebuffer that the output will be rendered to.
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)e->swapChain.extent.width;
	viewport.height = (float)e->swapChain.extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	// Scissor rectangle: Defines in which region pixels will actually be stored. Pixels outside the scissor rectangles will be discarded by the rasterizer. It works like a filter rather than a transformation.
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = e->swapChain.extent;

	// Viewport state: Combines the viewport and scissor rectangle into a viewport state. Multiple viewports and scissors require enabling a GPU feature.
	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	// Rasterizer: It takes the geometry shaped by the vertices from the vertex shader and turns it into fragments to be colored by the fragment shader. It also performs depth testing, face culling and the scissor test, and can be configured to output fragments that fill entire polygons or just the edges (wireframe rendering).
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;						// If VK_TRUE, fragments that are beyond the near and far planes are clamped to them (requires enabling a GPU feature), as opposed to discarding them.
	rasterizer.rasterizerDiscardEnable = VK_FALSE;				// If VK_TRUE, geometry never passes through the rasterizer stage (disables any output to the framebuffer).
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;				// How fragments are generated for geometry (VK_POLYGON_MODE_ ... FILL, LINE, POINT). Any mode other than FILL requires enabling a GPU feature.
	rasterizer.lineWidth = LINE_WIDTH;							// Thickness of lines in terms of number of fragments. The maximum line width supported depends on the hardware. Lines thicker than 1.0f requires enabling the `wideLines` GPU feature.
	rasterizer.cullMode = cullMode;								// (VK_CULL_MODE_BACK_BIT, VK_CULL_MODE_NONE...) Type of face culling (disable culling, cull front faces, cull back faces, cull both).
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;		// Vertex order for faces to be considered front-facing (clockwise, counterclockwise). If we draw vertices clockwise, because of the Y-flip we did in the projection matrix, the vertices are now drawn counter-clockwise.
	rasterizer.depthBiasEnable = VK_FALSE;						// If VK_TRUE, it allows to alter the depth values (sometimes used for shadow mapping).
	rasterizer.depthBiasConstantFactor = 0.0f;					// [Optional] 
	rasterizer.depthBiasClamp = 0.0f;							// [Optional] 
	rasterizer.depthBiasSlopeFactor = 0.0f;						// [Optional] 

	// Multisampling: One way to perform anti-aliasing. Combines the fragment shader results of multiple polygons that rasterize to the same pixel. Requires enabling a GPU feature.
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = e->c.msaaSamples;	// VK_SAMPLE_COUNT_1_BIT);	// <<<
	multisampling.sampleShadingEnable = (e->c.add_SS ? VK_TRUE : VK_FALSE);	// Enable sample shading in the pipeline
	if (e->c.add_SS)
		multisampling.minSampleShading = .2f;								// [Optional] Min fraction for sample shading; closer to one is smoother
	multisampling.pSampleMask = nullptr;									// [Optional]
	multisampling.alphaToCoverageEnable = VK_FALSE;							// [Optional]
	multisampling.alphaToOneEnable = VK_FALSE;								// [Optional]

	// Depth and stencil testing. Used if you are using a depth and/or stencil buffer.
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;				// Specify if the depth of new fragments should be compared to the depth buffer to see if they should be discarded.
	depthStencil.depthWriteEnable = VK_TRUE;				// Specify if the new depth of fragments that pass the depth test should actually be written to the depth buffer.
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;	// Specify the comparison that is performed to keep or discard fragments.
	depthStencil.depthBoundsTestEnable = VK_FALSE;				// [Optional] Use depth bound test (allows to only keep fragments that fall within a specified depth range.
	depthStencil.minDepthBounds = 0.0f;					// [Optional]
	depthStencil.maxDepthBounds = 1.0f;					// [Optional]
	depthStencil.stencilTestEnable = VK_FALSE;				// [Optional] Use stencil buffer operations (if you want to use it, make sure that the format of the depth/stencil image contains a stencil component).
	depthStencil.front = {};					// [Optional]
	depthStencil.back = {};					// [Optional]

	// Color blending: After a fragment shader has returned a color, it needs to be combined with the color that is already in the framebuffer. Two ways to do it: Mix old and new value to produce a final color, or combine the old and new value using a bitwise operation.
	//	- Configuration per attached framebuffer
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if (!hasTransparencies)	// Not alpha blending implemented
	{
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;		// Optional. Check VkBlendFactor enum.
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;	// Optional
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;				// Optional. Check VkBlendOp enum.
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;		// Optional
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;	// Optional
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;				// Optional
	}
	else	// Options for implementing alpha blending (new color blended with old color based on its opacity):
	{
		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		/* Pseudocode demonstration:
			if (blendEnable) {
				finalColor.rgb = (srcColorBlendFactor * newColor.rgb) <colorBlendOp> (dstColorBlendFactor * oldColor.rgb);
				finalColor.a = (srcAlphaBlendFactor * newColor.a) <alphaBlendOp> (dstAlphaBlendFactor * oldColor.a);
			}
			else finalColor = newColor;
			finalColor = finalColor & colorWriteMask;
		*/
	}

	std::vector<VkPipelineColorBlendAttachmentState> setColorBlendAttachments(e->rp->getSubpass(renderPassIndex, subpassIndex).colorAttsCount, colorBlendAttachment);

	//	- Global color blending settings. Set blend constants that you can use as blend factors in the aforementioned calculations.
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;					// VK_FALSE: Blending method of mixing values.  VK_TRUE: Blending method of bitwise values combination (this disables the previous structure, like blendEnable = VK_FALSE).
	colorBlending.logicOp = VK_LOGIC_OP_COPY;				// Optional
	colorBlending.attachmentCount = e->rp->getSubpass(renderPassIndex, subpassIndex).colorAttsCount;
	colorBlending.pAttachments = setColorBlendAttachments.data();
	colorBlending.blendConstants[0] = 0.0f;						// Optional
	colorBlending.blendConstants[1] = 0.0f;						// Optional
	colorBlending.blendConstants[2] = 0.0f;						// Optional
	colorBlending.blendConstants[3] = 0.0f;						// Optional

	// Dynamic states: A limited amount of the state that we specified in the previous structs can actually be changed without recreating the pipeline (size of viewport, lined width, blend constants...). If you want to do that, you have to fill this struct. This will cause the configuration of these values to be ignored and you will be required to specify the data at drawing time. This struct can be substituted by a nullptr later on if you don't have any dynamic state.
	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH };

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	// Create graphics pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	//pipelineInfo.flags				= VK_PIPELINE_CREATE_DERIVATIVE_BIT;	// Required for using basePipelineHandle and basePipelineIndex members
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;		// [Optional]
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr;					// [Optional] <<< dynamic state not passed. If the bound graphics pipeline state was created with the VK_DYNAMIC_STATE_VIEWPORT dynamic state enabled then vkCmdSetViewport must have been called in the current command buffer prior to this drawing command. 
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = e->rp->renderPasses[renderPassIndex].renderPass;// It's possible to use other render passes with this pipeline instead of this specific instance, but they have to be compatible with "renderPass" (https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#renderpass-compatibility).
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;		// [Optional] Specify the handle of an existing pipeline.
	pipelineInfo.basePipelineIndex = -1;					// [Optional] Reference another pipeline that is about to be created by index.
	
	if (vkCreateGraphicsPipelines(e->c.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
		throw std::runtime_error("Failed to create graphics pipeline!");
	
	// Cleanup
	//vkDestroyShaderModule(e.device, fragShaderModule, nullptr);
	//vkDestroyShaderModule(e.device, vertShaderModule, nullptr);
}

// (22)
void ModelData::createDescriptorPool()
{
	#ifdef DEBUG_MODELS
		std::cout << typeid(*this).name() << "::" << __func__ << " (" << name << ')' << std::endl;
	#endif

	// Describe our descriptor sets.
	std::vector<VkDescriptorPoolSize> poolSizes;
	VkDescriptorPoolSize pool;

	if (globalUBO_vs && globalUBO_vs->subUboSize)
	{
		pool.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool.descriptorCount = static_cast<uint32_t>(e->swapChain.images.size());
		poolSizes.push_back(pool);
	}

	if (vsUBO.maxNumSubUbos && vsUBO.subUboSize)
	{
		pool.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;								// VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER or VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
		pool.descriptorCount = static_cast<uint32_t>(e->swapChain.images.size());	// Number of descriptors of this type to allocate
		poolSizes.push_back(pool);
	}

	if (globalUBO_fs && globalUBO_fs->subUboSize)
	{
		pool.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool.descriptorCount = static_cast<uint32_t>(e->swapChain.images.size());
		poolSizes.push_back(pool);
	}

	if (fsUBO.maxNumSubUbos && fsUBO.subUboSize)
	{
		pool.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool.descriptorCount = static_cast<uint32_t>(e->swapChain.images.size());
		poolSizes.push_back(pool);
	}

	for (size_t i = 0; i < textures.size(); ++i)
	{
		pool.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool.descriptorCount = static_cast<uint32_t>(e->swapChain.images.size());
		poolSizes.push_back(pool);
	}

	if(e->rp->getSubpass(renderPassIndex, subpassIndex).inputAtts.size())
	{
		pool.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		pool.descriptorCount = static_cast<uint32_t>(e->swapChain.images.size());
		poolSizes.push_back(pool);
	}

	// Allocate one of these descriptors for every frame.
	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = static_cast<uint32_t>(e->swapChain.images.size());	// Max. number of individual descriptor sets that may be allocated
	poolInfo.flags = 0;														// Determine if individual descriptor sets can be freed (VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) or not (0). Since we aren't touching the descriptor set after its creation, we put 0 (default).

	if (vkCreateDescriptorPool(e->c.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
		throw std::runtime_error("Failed to create descriptor pool!");
}

// (23)
void ModelData::createDescriptorSets()
{
	#ifdef DEBUG_MODELS
		std::cout << typeid(*this).name() << "::" << __func__ << " (" << name << ')' << std::endl;
	#endif

	descriptorSets.resize(e->swapChain.images.size());

	std::vector<VkDescriptorSetLayout> layouts(e->swapChain.images.size(), descriptorSetLayout);

	// Describe the descriptor set. Here, we will create one descriptor set for each swap chain image, all with the same layout
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;											// Descriptor pool to allocate from
	allocInfo.descriptorSetCount = static_cast<uint32_t>(e->swapChain.images.size());	// Number of descriptor sets to allocate
	allocInfo.pSetLayouts = layouts.data();												// Descriptor layout to base them on

	// Allocate the descriptor set handles
	if (vkAllocateDescriptorSets(e->c.device, &allocInfo, descriptorSets.data()) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate descriptor sets!");

	// Populate each descriptor set.
	for (size_t i = 0; i < e->swapChain.images.size(); i++)
	{
		VkDescriptorBufferInfo descriptorInfo;						// Info about one descriptor

		// Global UBO vertex shader
		std::vector< VkDescriptorBufferInfo> globalBufferInfo_vs;	// Info about each descriptor
		if(globalUBO_vs && globalUBO_vs->subUboSize)
			for (unsigned j = 0; j < globalUBO_vs->maxNumSubUbos; j++)
			{
				if (globalUBO_vs->subUboSize) descriptorInfo.buffer = globalUBO_vs->uboBuffers[i];
				descriptorInfo.range = globalUBO_vs->subUboSize;
				descriptorInfo.offset = j * globalUBO_vs->subUboSize;
				globalBufferInfo_vs.push_back(descriptorInfo);
			}

		// UBO vertex shader
		std::vector< VkDescriptorBufferInfo> bufferInfo_vs;
		for (unsigned j = 0; j < vsUBO.maxNumSubUbos; j++)
		{
			if (vsUBO.subUboSize) descriptorInfo.buffer = vsUBO.uboBuffers[i];
			descriptorInfo.range  = vsUBO.subUboSize;
			descriptorInfo.offset = j * vsUBO.subUboSize;
			bufferInfo_vs.push_back(descriptorInfo);
		}

		// Global UBO fragment shader
		std::vector< VkDescriptorBufferInfo> globalBufferInfo_fs;	// Info about each descriptor
		if (globalUBO_fs && globalUBO_fs->subUboSize)
			for (unsigned j = 0; j < globalUBO_fs->maxNumSubUbos; j++)
			{
				if (globalUBO_fs->subUboSize) descriptorInfo.buffer = globalUBO_fs->uboBuffers[i];
				descriptorInfo.range = globalUBO_fs->subUboSize;
				descriptorInfo.offset = j * globalUBO_fs->subUboSize;
				globalBufferInfo_fs.push_back(descriptorInfo);
			}

		// UBO fragment shader
		std::vector< VkDescriptorBufferInfo> bufferInfo_fs;
		for (unsigned j = 0; j < fsUBO.maxNumSubUbos; j++)
		{
			if (fsUBO.subUboSize) descriptorInfo.buffer = fsUBO.uboBuffers[i];
			descriptorInfo.range  = fsUBO.subUboSize;
			descriptorInfo.offset = j * fsUBO.subUboSize;
			bufferInfo_fs.push_back(descriptorInfo);
		}

		// Textures
		std::vector<VkDescriptorImageInfo> imageInfo(textures.size());
		for (size_t i = 0; i < textures.size(); i++) {
			imageInfo[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo[i].imageView = textures[i]->textureImageView;
			imageInfo[i].sampler = textures[i]->textureSampler;
		}

		// Input attachments
		std::vector<VkDescriptorImageInfo> inputAttachInfo(e->rp->getSubpass(renderPassIndex, subpassIndex).inputAtts.size());
		for (unsigned i = 0; i < inputAttachInfo.size(); i++)
		{
			inputAttachInfo[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			inputAttachInfo[i].imageView = e->rp->getSubpass(renderPassIndex, subpassIndex).inputAtts[i]->view;
			inputAttachInfo[i].sampler = e->rp->getSubpass(renderPassIndex, subpassIndex).inputAtts[i]->sampler;
		}
		
		std::vector<VkWriteDescriptorSet> descriptorWrites;
		VkWriteDescriptorSet descriptor;
		uint32_t binding = 0;
		
		if (globalUBO_vs && globalUBO_vs->subUboSize)
		{
			descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptor.dstSet = descriptorSets[i];
			descriptor.dstBinding = binding++;
			descriptor.dstArrayElement = 0;
			descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptor.descriptorCount = globalUBO_vs->maxNumSubUbos;
			descriptor.pBufferInfo = globalBufferInfo_vs.data();
			descriptor.pImageInfo = nullptr;
			descriptor.pTexelBufferView = nullptr;
			descriptor.pNext = nullptr;

			descriptorWrites.push_back(descriptor);
		}

		if (vsUBO.subUboSize)
		{
			descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptor.dstSet = descriptorSets[i];
			descriptor.dstBinding = binding++;
			descriptor.dstArrayElement = 0;
			descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptor.descriptorCount = vsUBO.maxNumSubUbos;
			descriptor.pBufferInfo = bufferInfo_vs.data();
			descriptor.pImageInfo = nullptr;
			descriptor.pTexelBufferView = nullptr;
			descriptor.pNext = nullptr;

			descriptorWrites.push_back(descriptor);
		}

		if (globalUBO_fs && globalUBO_fs->subUboSize)
		{
			descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptor.dstSet = descriptorSets[i];
			descriptor.dstBinding = binding++;
			descriptor.dstArrayElement = 0;
			descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptor.descriptorCount = globalUBO_fs->maxNumSubUbos;
			descriptor.pBufferInfo = globalBufferInfo_fs.data();
			descriptor.pImageInfo = nullptr;
			descriptor.pTexelBufferView = nullptr;
			descriptor.pNext = nullptr;

			descriptorWrites.push_back(descriptor);
		}

		if (fsUBO.subUboSize)
		{
			descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptor.dstSet = descriptorSets[i];
			descriptor.dstBinding = binding++;
			descriptor.dstArrayElement = 0;
			descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptor.descriptorCount = fsUBO.maxNumSubUbos;
			descriptor.pBufferInfo = bufferInfo_fs.data();
			descriptor.pImageInfo = nullptr;
			descriptor.pTexelBufferView = nullptr;
			descriptor.pNext = nullptr;

			descriptorWrites.push_back(descriptor);
		}

		if (textures.size())
		{
			descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptor.dstSet = descriptorSets[i];
			descriptor.dstBinding = binding++;
			descriptor.dstArrayElement = 0;
			descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptor.descriptorCount = textures.size();			// LOOK maybe this can be used instead of the for-loop
			descriptor.pBufferInfo = nullptr;
			descriptor.pImageInfo = imageInfo.data();
			descriptor.pTexelBufferView = nullptr;
			descriptor.pNext = nullptr;

			descriptorWrites.push_back(descriptor);
		}
		
		if (e->rp->getSubpass(renderPassIndex, subpassIndex).inputAtts.size())
		{
			descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptor.dstSet = descriptorSets[i];
			descriptor.dstBinding = binding++;
			descriptor.dstArrayElement = 0;
			descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;	// VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
			descriptor.descriptorCount = inputAttachInfo.size();
			descriptor.pBufferInfo = nullptr;
			descriptor.pImageInfo = inputAttachInfo.data();
			descriptor.pTexelBufferView = nullptr;
			descriptor.pNext = nullptr;

			descriptorWrites.push_back(descriptor);
		}

		vkUpdateDescriptorSets(e->c.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);	// Accepts 2 kinds of arrays as parameters: VkWriteDescriptorSet, VkCopyDescriptorSet.
	}
}

void ModelData::recreate_Pipeline_Descriptors()
{
	#ifdef DEBUG_MODELS
		std::cout << typeid(*this).name() << "::" << __func__ << " (" << name << ')' << std::endl;
	#endif

	createGraphicsPipeline();			// Recreate graphics pipeline because viewport and scissor rectangle size is specified during graphics pipeline creation (this can be avoided by using dynamic state for the viewport and scissor rectangles).

	vsUBO.createUBO();					// Uniform buffers depend on the number of swap chain images.
	fsUBO.createUBO();
	createDescriptorPool();				// Descriptor pool depends on the swap chain images.
	createDescriptorSets();				// Descriptor sets
}

void ModelData::cleanup_Pipeline_Descriptors()
{
	#ifdef DEBUG_MODELS
		std::cout << typeid(*this).name() << "::" << __func__ << " (" << name << ')' << std::endl;
	#endif

	// Graphics pipeline
	vkDestroyPipeline(e->c.device, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(e->c.device, pipelineLayout, nullptr);

	// Uniform buffers & memory
	vsUBO.destroyUBO();
	fsUBO.destroyUBO();

	// Descriptor pool & Descriptor set (When a descriptor pool is destroyed, all descriptor-sets allocated from the pool are implicitly/automatically freed and become invalid)
	vkDestroyDescriptorPool(e->c.device, descriptorPool, nullptr);
}

void ModelData::cleanup()
{
	#ifdef DEBUG_MODELS
		std::cout << typeid(*this).name() << "::" << __func__ << " (" << name << ')' << std::endl;
	#endif
	
	// Descriptor set layout
	vkDestroyDescriptorSetLayout(e->c.device, descriptorSetLayout, nullptr);

	// Index
	if (vert.indexCount)
	{
		vkDestroyBuffer(e->c.device, vert.indexBuffer, nullptr);
		vkFreeMemory(e->c.device, vert.indexBufferMemory, nullptr);
		e->c.memAllocObjects--;
	}

	// Vertex
	vkDestroyBuffer(e->c.device, vert.vertexBuffer, nullptr);
	vkFreeMemory(e->c.device, vert.vertexBufferMemory, nullptr);
	e->c.memAllocObjects--;
}

void ModelData::deleteLoader()
{
	if (resLoader)
	{
		delete resLoader;
		resLoader = nullptr;
	}
}

size_t ModelData::getActiveInstancesCount() { return activeInstances; }

bool ModelData::setActiveInstancesCount(size_t activeInstancesCount)
{
	#ifdef DEBUG_MODELS
		std::cout << typeid(*this).name() << "::" << __func__ << " (" << name << ')' << std::endl;
	#endif
	
	if (activeInstancesCount == activeInstances) return false;

	if (vsUBO.setNumActiveSubUbos(activeInstancesCount) == false)
		std::cerr << "The number of rendered instances (" << name << ") cannot be higher than " << vsUBO.maxNumSubUbos << std::endl;

	this->activeInstances = vsUBO.numActiveSubUbos;
	return true;
	{
		//vsUBO.resizeUBO(activeInstancesCount);

		//if (fullyConstructed)
		//{
		//	vsUBO.destroyUniformBuffers();
		//	vkDestroyDescriptorPool(e->c.device, descriptorPool, nullptr);	// Descriptor-sets are automatically freed when the descriptor pool is destroyed.
		//
		//	vsUBO.createUniformBuffers();		// Create a UBO with the new size
		//	createDescriptorPool();				// Required for creating descriptor sets
		//	createDescriptorSets();				// Contains the UBO
		//}
	}
}
