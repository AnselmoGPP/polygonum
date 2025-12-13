#include <iostream>

#include "polygonum/renderer.hpp"
#include "polygonum/importer.hpp"

ModelDataInfo::ModelDataInfo()
	: name("noName"),
	numInstances(0),
	maxNumInstances(UINT32_MAX),
	topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST),
	vertexType({vaPos, vaNorm, vaUv}),
	vertexesLoader(nullptr),
	shadersInfo(std::vector<ShaderLoader*>()),
	transparency(false),
	renderPassIndex(0),
	subpassIndex(0),
	cullMode(VK_CULL_MODE_BACK_BIT)
{ }


ModelData::ModelData(Renderer* renderer, ModelDataInfo& modelInfo)
	: r(renderer),
	name(modelInfo.name),
	primitiveTopology(modelInfo.topology),
	vertexType(modelInfo.vertexType),
	hasTransparencies(modelInfo.transparency),
	cullMode(modelInfo.cullMode),
	renderPassIndex(modelInfo.renderPassIndex),
	subpassIndex(modelInfo.subpassIndex),
	bindSets(modelInfo.bindSets),
	fullyConstructed(false),
	ready(false)
{
	#ifdef DEBUG_MODELS
		std::cout << typeid(*this).name() << "::" << __func__ << " (" << name << ')' << std::endl;
	#endif

	setNumInstances(modelInfo.numInstances);

	resLoader = new ResourcesLoader(modelInfo.vertexesLoader, modelInfo.shadersInfo);
}

ModelData::~ModelData()
{
	#ifdef DEBUG_MODELS
		std::cout << typeid(*this).name() << "::" << __func__ << " (" << name << ')' << std::endl;
	#endif

	if (fullyConstructed)
	{
		// Pipeline & Descriptors
		cleanup_pipeline_and_descriptors();

		// Descriptor set layout
		for(auto& set : descriptorSetLayouts)
			vkDestroyDescriptorSetLayout(r->c.device, set, nullptr);

		// Index buffer
		if (vert.indexCount)
			r->c.destroyBuffer(r->c.device, vert.indexBuffer, vert.indexBufferMemory);

		// Vertex buffer
		r->c.destroyBuffer(r->c.device, vert.vertexBuffer, vert.vertexBufferMemory);
	}

	// Resources loader
	deleteLoader();
}

ModelData::ModelData(ModelData&& other) noexcept
	: r(std::move(other.r)),
	primitiveTopology(std::move(other.primitiveTopology)),
	vertexType(std::move(other.vertexType)),
	hasTransparencies(std::move(other.hasTransparencies)),
	cullMode(std::move(other.cullMode)),
	numInstances(std::move(other.numInstances)),
	pipelineLayout(std::move(other.pipelineLayout)),
	graphicsPipeline(std::move(other.graphicsPipeline)),
	bindSets(std::move(other.bindSets)),
	shaders(std::move(other.shaders)),
	vert(std::move(other.vert)),
	descriptorSetLayouts(std::move(other.descriptorSetLayouts)),
	descriptorPool(std::move(other.descriptorPool)),
	descriptorSets(std::move(other.descriptorSets)),
	renderPassIndex(std::move(other.renderPassIndex)),
	subpassIndex(std::move(other.subpassIndex)),
	resLoader(std::move(other.resLoader)),
	fullyConstructed(std::move(other.fullyConstructed)),
	ready(std::move(other.ready)),
	name(std::move(other.name))
{
	other.r = nullptr;
	other.resLoader = nullptr;
}

ModelData& ModelData::operator=(ModelData&& other) noexcept
{
	if (this == &other) return *this;
	
	// Free existing resources
	deleteLoader();

	// Transfer resources ownership
	r = other.r;
	primitiveTopology = other.primitiveTopology;
	hasTransparencies = other.hasTransparencies;
	cullMode = other.cullMode;
	numInstances = other.numInstances;
	pipelineLayout = other.pipelineLayout;
	graphicsPipeline = other.graphicsPipeline;
	descriptorSetLayouts = other.descriptorSetLayouts;
	descriptorPool = other.descriptorPool;
	renderPassIndex = other.renderPassIndex;
	subpassIndex = other.subpassIndex;
	resLoader = other.resLoader;
	fullyConstructed = other.fullyConstructed;
	ready = other.ready;

	vertexType = std::move(other.vertexType);
	shaders = std::move(other.shaders);
	bindSets = std::move(other.bindSets);
	vert = std::move(other.vert);
	descriptorSets = std::move(other.descriptorSets);
	name = std::move(other.name);

	// Leave other in valid state
	other.r = nullptr;
	other.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
	other.hasTransparencies = false;
	other.cullMode = VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
	other.bindSets.clear();
	other.pipelineLayout = other.pipelineLayout;
	other.graphicsPipeline = other.graphicsPipeline;
	other.descriptorSetLayouts = other.descriptorSetLayouts;
	other.descriptorPool = other.descriptorPool;
	other.renderPassIndex = 0;
	other.subpassIndex = 0;
	other.resLoader = nullptr;
	other.fullyConstructed = false;
	other.ready = false;
	other.name = "";
	
	other.vertexType = VertexType();
	other.shaders.clear();
	other.bindSets.clear();
	other.vert = VertexData();
	other.descriptorSets.clear();
	
	return *this;
}

void ModelData::fullConstruction(Renderer& ren)
{
	#ifdef DEBUG_MODELS
		std::cout << typeid(*this).name() << "::" << __func__ << " (" << name << ')' << std::endl;
	#endif
	
	if (resLoader) {
		resLoader->loadResources(*this, ren);
		deleteLoader();
	} else std::cout << "Error: No loading info data" << std::endl;

	//binds.createTextures();
	for(BindingSet& set : bindSets) set.createBindings(r);

	createDescriptorSetLayout();
	createGraphicsPipeline();
	
	//binds.createBuffers();
	createDescriptorPool();
	createDescriptorSets();
	
	fullyConstructed = true;
}

// (9)
void ModelData::createDescriptorSetLayout()
{
	#ifdef DEBUG_MODELS
		std::cout << typeid(*this).name() << "::" << __func__ << " (" << name << ')' << std::endl;
	#endif

	bool inputAttsAdded = false;

	for(const auto& set : bindSets)   // each set
	{
		// Describe all bindings.

		std::vector<VkDescriptorSetLayoutBinding> bindings;
		uint32_t bindNumber = 0;

		//  1. Vertex shader bindings
		
		//   1.1. Global UBO (vertex shader)
		for(const auto& bind : set.vsGlobal)   // each binding
		{
			VkDescriptorSetLayoutBinding vsGlobalUboLayoutBinding{};
			vsGlobalUboLayoutBinding.binding = bindNumber++;
			vsGlobalUboLayoutBinding.descriptorType = bind->type;
			vsGlobalUboLayoutBinding.descriptorCount = bind->numDescriptors;
			vsGlobalUboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
			vsGlobalUboLayoutBinding.pImmutableSamplers = nullptr;
			
			bindings.push_back(vsGlobalUboLayoutBinding);
		}

		//    1.2. Uniform buffer descriptor (vertex shader)
		for (const auto& bind : set.vsLocal)
		{
			VkDescriptorSetLayoutBinding vsUboLayoutBinding{};
			vsUboLayoutBinding.binding = bindNumber++;
			vsUboLayoutBinding.descriptorType = bind.type;				// VK_DESCRIPTOR_TYPE_ ... UNIFORM_BUFFER, UNIFORM_BUFFER_DYNAMIC
			vsUboLayoutBinding.descriptorCount = bind.numDescriptors;	// In case you want to specify an array of UBOs <<< (example: for specifying a transformation for each bone in a skeleton for skeletal animation).
			vsUboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;	// Tell in which shader stages the descriptor will be referenced. This field can be a combination of VkShaderStageFlagBits values or the value VK_SHADER_STAGE_ALL_GRAPHICS.
			vsUboLayoutBinding.pImmutableSamplers = nullptr;			// [Optional] Only relevant for image sampling related descriptors.
			
			bindings.push_back(vsUboLayoutBinding);
		}

		//    1.2. Samplers (vertex shader). Combined image sampler descriptor (set of textures) (it lets shaders access an image resource through a sampler object)
		for (const auto& texSet : set.vsTextures)
		{
			VkDescriptorSetLayoutBinding samplerLayoutBinding{};
			samplerLayoutBinding.binding = bindNumber++;
			samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			samplerLayoutBinding.descriptorCount = static_cast<uint32_t>(texSet.size());
			samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;			// We want to use the combined image sampler descriptor in the fragment shader. It's possible to use texture sampling in the vertex shader (example: to dynamically deform a grid of vertices by a heightmap).
			samplerLayoutBinding.pImmutableSamplers = nullptr;

			bindings.push_back(samplerLayoutBinding);
		}

		//  2. Fragment shader bindings

		//    2.1. Global UBO (fragment shader)
		for (const auto& bind : set.fsGlobal)
		{
			VkDescriptorSetLayoutBinding fsGlobalUboLayoutBinding{};
			fsGlobalUboLayoutBinding.binding = bindNumber++;
			fsGlobalUboLayoutBinding.descriptorType = bind->type;
			fsGlobalUboLayoutBinding.descriptorCount = bind->numDescriptors;
			fsGlobalUboLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			fsGlobalUboLayoutBinding.pImmutableSamplers = nullptr;
			
			bindings.push_back(fsGlobalUboLayoutBinding);
		}

		//    2.2. Uniform buffer descriptor (fragment shader)
		for (const auto& bind : set.fsLocal)
		{
			VkDescriptorSetLayoutBinding fsUboLayoutBinding{};
			fsUboLayoutBinding.binding = bindNumber++;
			fsUboLayoutBinding.descriptorType = bind.type;
			fsUboLayoutBinding.descriptorCount = bind.numDescriptors;
			fsUboLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			fsUboLayoutBinding.pImmutableSamplers = nullptr;
			
			bindings.push_back(fsUboLayoutBinding);
		}
		
		//    2.3. Samplers (fragment shader)
		for (const auto& texSet : set.fsTextures)
		{
			VkDescriptorSetLayoutBinding samplerLayoutBinding{};
			samplerLayoutBinding.binding = bindNumber++;
			samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			samplerLayoutBinding.descriptorCount = static_cast<uint32_t>(texSet.size());
			samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;			// We want to use the combined image sampler descriptor in the fragment shader. It's possible to use texture sampling in the vertex shader (example: to dynamically deform a grid of vertices by a heightmap).
			samplerLayoutBinding.pImmutableSamplers = nullptr;

			bindings.push_back(samplerLayoutBinding);
		}
		
		//    2.4. Input attachments
		if(r->rp->getSubpass(renderPassIndex, subpassIndex).inputAtts.size() && !inputAttsAdded)
		{
			inputAttsAdded = true;
			VkDescriptorSetLayoutBinding inputAttachmentLayoutBinding{};
			inputAttachmentLayoutBinding.binding = bindNumber++;
			inputAttachmentLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;	// VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
			inputAttachmentLayoutBinding.descriptorCount = static_cast<uint32_t>(r->rp->getSubpass(renderPassIndex, subpassIndex).inputAtts.size());
			inputAttachmentLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			inputAttachmentLayoutBinding.pImmutableSamplers = nullptr;

			bindings.push_back(inputAttachmentLayoutBinding);
		}

		// Create all descriptor set layouts (one per binding set).

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();
		
		VkDescriptorSetLayout descSetLayout;
		if (vkCreateDescriptorSetLayout(r->c.device, &layoutInfo, nullptr, &descSetLayout) != VK_SUCCESS)
			throw std::runtime_error("Failed to create descriptor set layout!");
		descriptorSetLayouts.push_back(descSetLayout);
	}
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
	pipelineLayoutInfo.setLayoutCount = descriptorSetLayouts.size();
	pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = 0;					// <<< Push constants are another way of passing dynamic values to shaders.
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(r->c.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
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
	viewport.width = (float)r->swapChain.extent.width;
	viewport.height = (float)r->swapChain.extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	// Scissor rectangle: Defines in which region pixels will actually be stored. Pixels outside the scissor rectangles will be discarded by the rasterizer. It works like a filter rather than a transformation.
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = r->swapChain.extent;

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
	multisampling.rasterizationSamples = r->c.msaaSamples;	// VK_SAMPLE_COUNT_1_BIT);	// <<<
	multisampling.sampleShadingEnable = (r->c.add_SS ? VK_TRUE : VK_FALSE);	// Enable sample shading in the pipeline
	if (r->c.add_SS)
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
	if (!hasTransparencies)	// No alpha blending implemented
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

	std::vector<VkPipelineColorBlendAttachmentState> setColorBlendAttachments(r->rp->getSubpass(renderPassIndex, subpassIndex).colorAttsCount, colorBlendAttachment);

	//	- Global color blending settings. Set blend constants that you can use as blend factors in the aforementioned calculations.
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;					// VK_FALSE: Blending method of mixing values.  VK_TRUE: Blending method of bitwise values combination (this disables the previous structure, like blendEnable = VK_FALSE).
	colorBlending.logicOp = VK_LOGIC_OP_COPY;				// Optional
	colorBlending.attachmentCount = r->rp->getSubpass(renderPassIndex, subpassIndex).colorAttsCount;
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
	//pipelineInfo.pNext;
	//pipelineInfo.flags				= VK_PIPELINE_CREATE_DERIVATIVE_BIT;	// Required for using basePipelineHandle and basePipelineIndex members
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	//pipelineInfo.pTessellationState;
	//pipelineInfo.pTessellationState;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;		// [Optional]
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr;					// [Optional] <<< dynamic state not passed. If the bound graphics pipeline state was created with the VK_DYNAMIC_STATE_VIEWPORT dynamic state enabled then vkCmdSetViewport must have been called in the current command buffer prior to this drawing command. 
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = r->rp->renderPasses[renderPassIndex].renderPass;// It's possible to use other render passes with this pipeline instead of this specific instance, but they have to be compatible with "renderPass" (https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#renderpass-compatibility).
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;		// [Optional] Specify the handle of an existing pipeline.
	pipelineInfo.basePipelineIndex = -1;					// [Optional] Reference another pipeline that is about to be created by index.
	
	if (vkCreateGraphicsPipelines(r->c.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
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

	bool inputAttsAdded = false;
	uint32_t numSwapchainImages = static_cast<uint32_t>(r->swapChain.images.size());
	std::vector<VkDescriptorPoolSize> poolSizes;

	// Describe all bindings

	for (const auto& set : bindSets)   // each set
	{
		for (const auto& buf : set.vsGlobal)   // each binding
		{
			VkDescriptorPoolSize pool;
			pool.type = buf->type;   // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER or VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
			pool.descriptorCount = numSwapchainImages * buf->numDescriptors;   // Number of descriptors of this type to allocate
			poolSizes.push_back(pool);
		}

		for (const auto& buf : set.vsLocal)
		{
			VkDescriptorPoolSize pool;
			pool.type = buf.type;
			pool.descriptorCount = numSwapchainImages * buf.numDescriptors;
			poolSizes.push_back(pool);
		}

		for (const auto& texSet : set.vsTextures)
		{
			VkDescriptorPoolSize pool;
			pool.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			pool.descriptorCount = numSwapchainImages * texSet.size();   // <<< optional: texture bindings have no need to be created numSwapchainImages times.
			poolSizes.push_back(pool);
		}

		for (const auto& buf : set.fsGlobal)
		{
			VkDescriptorPoolSize pool;
			pool.type = buf->type;
			pool.descriptorCount = numSwapchainImages * buf->numDescriptors;
			poolSizes.push_back(pool);
		}

		for (const auto& buf : set.fsLocal)
		{
			VkDescriptorPoolSize pool;
			pool.type = buf.type;
			pool.descriptorCount = numSwapchainImages * buf.numDescriptors;
			poolSizes.push_back(pool);
		}

		for (const auto& texSet : set.fsTextures)
		{
			VkDescriptorPoolSize pool;
			pool.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			pool.descriptorCount = numSwapchainImages * texSet.size();
			poolSizes.push_back(pool);
		}

		if (r->rp->getSubpass(renderPassIndex, subpassIndex).inputAtts.size() && !inputAttsAdded)
		{
			inputAttsAdded = true;

			VkDescriptorPoolSize pool;
			pool.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
			pool.descriptorCount = numSwapchainImages * r->rp->getSubpass(renderPassIndex, subpassIndex).inputAtts.size();
			poolSizes.push_back(pool);
		}
	}

	// Create descriptor pool.

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = numSwapchainImages;	// Max. number of individual descriptor sets that may be allocated
	poolInfo.flags = 0;						// Determine if individual descriptor sets can be freed (VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) or not (0). Since we aren't touching the descriptor set after its creation, we put 0 (default).

	if (vkCreateDescriptorPool(r->c.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
		throw std::runtime_error("Failed to create descriptor pool!");
}

// (23)
void ModelData::createDescriptorSets()
{
	#ifdef DEBUG_MODELS
		std::cout << typeid(*this).name() << "::" << __func__ << " (" << name << ')' << std::endl;
	#endif

	uint32_t numSets = static_cast<uint32_t>(bindSets.size());
	uint32_t numSwapChainImgs = static_cast<uint32_t>(r->swapChain.images.size());
	uint32_t numInputAtts = static_cast<uint32_t>(r->rp->getSubpass(renderPassIndex, subpassIndex).inputAtts.size());

	descriptorSets.resize(numSwapChainImgs, std::vector<VkDescriptorSet>(numSets));

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;							// Descriptor pool to allocate from
	allocInfo.descriptorSetCount = numSets;								// Number of descriptor sets to allocate
	allocInfo.pSetLayouts = descriptorSetLayouts.data();				// Descriptor layout to base them on

	// Allocate descriptor set handles. Once per swap chain image.
	for (unsigned i = 0; i < numSwapChainImgs; i++)
		if (vkAllocateDescriptorSets(r->c.device, &allocInfo, descriptorSets[i].data()) != VK_SUCCESS)
			throw std::runtime_error("Failed to allocate descriptor sets!");
	
	// Populate each descriptor set.
	for (size_t i = 0; i < numSwapChainImgs; i++)   // each swapchain image
		for (unsigned j = 0; j < bindSets.size(); j++)   // each set
		{
			// Determine the buffer for each descriptor.
			std::vector<VkDescriptorBufferInfo> globalBufferInfo_vs;   // all descriptors of current set ([sc.img][set]).
			std::vector<VkDescriptorBufferInfo> bufferInfo_vs;
			std::vector<VkDescriptorBufferInfo> globalBufferInfo_fs;
			std::vector<VkDescriptorBufferInfo> bufferInfo_fs;
			std::vector<VkDescriptorImageInfo > imageInfo_vs;
			std::vector<VkDescriptorImageInfo > imageInfo_fs;
			std::vector<VkDescriptorImageInfo > inputAttachInfo;

			// - Global UBO vertex shader
			for (const auto& buff : bindSets[j].vsGlobal)   // each binding
				for (unsigned k = 0; k < buff->numDescriptors; k++)   // each descriptor
				{
					VkDescriptorBufferInfo descriptorInfo;
					descriptorInfo.buffer = buff->bindingBuffers[i];
					descriptorInfo.range = buff->descriptorSize;
					descriptorInfo.offset = k * buff->descriptorSize;
					globalBufferInfo_vs.push_back(descriptorInfo);
				}

			// - UBO vertex shader
			for (const auto& buf : bindSets[j].vsLocal)
				for (unsigned k = 0; k < buf.numDescriptors; k++)
				{
					VkDescriptorBufferInfo descriptorInfo;
					descriptorInfo.buffer = buf.bindingBuffers[i];
					descriptorInfo.range = buf.descriptorSize;
					descriptorInfo.offset = k * buf.descriptorSize;
					bufferInfo_vs.push_back(descriptorInfo);
				}

			// - VS textures
			for (const auto& texSet : bindSets[j].vsTextures)
				for (const auto& tex : texSet)
				{
					VkDescriptorImageInfo descriptorImageInfo;
					descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					descriptorImageInfo.imageView = tex->texture.view;
					descriptorImageInfo.sampler = tex->texture.sampler;
					imageInfo_vs.push_back(descriptorImageInfo);
				}

			// - Global UBO fragment shader
			for (const auto& buf : bindSets[j].fsGlobal)
				for (unsigned k = 0; k < buf->numDescriptors; k++)
				{
					VkDescriptorBufferInfo descriptorInfo;
					descriptorInfo.buffer = buf->bindingBuffers[i];
					descriptorInfo.range = buf->descriptorSize;
					descriptorInfo.offset = k * buf->descriptorSize;
					globalBufferInfo_fs.push_back(descriptorInfo);
				}

			// - UBO fragment shader
			for (const auto& buf : bindSets[j].fsLocal)
				for (unsigned k = 0; k < buf.numDescriptors; k++)
				{
					VkDescriptorBufferInfo descriptorInfo;
					descriptorInfo.buffer = buf.bindingBuffers[i];
					descriptorInfo.range = buf.descriptorSize;
					descriptorInfo.offset = k * buf.descriptorSize;
					bufferInfo_fs.push_back(descriptorInfo);
				}

			// - FS textures
			for (const auto& texSet : bindSets[j].fsTextures)
				for (const auto& tex : texSet)
				{
					VkDescriptorImageInfo descriptorImageInfo;
					descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					descriptorImageInfo.imageView = tex->texture.view;
					descriptorImageInfo.sampler = tex->texture.sampler;
					imageInfo_fs.push_back(descriptorImageInfo);
				}

			// - Input attachments (only in 1st set)
			if(!j)
				for (unsigned k = 0; k < numInputAtts; k++)
				{
					VkDescriptorImageInfo descriptorImageInfo;
					descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					descriptorImageInfo.imageView = r->rp->getSubpass(renderPassIndex, subpassIndex).inputAtts[k]->view;
					descriptorImageInfo.sampler = r->rp->getSubpass(renderPassIndex, subpassIndex).inputAtts[k]->sampler;
					inputAttachInfo.push_back(descriptorImageInfo);
				}

			// Determine each set.
			std::vector<VkWriteDescriptorSet> descriptorWrites;
			uint32_t binding = 0;

			// - Global UBO vertex shader
			for (const auto& buf : bindSets[j].vsGlobal)   // each binding
			{
				VkWriteDescriptorSet descriptor;
				descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptor.dstSet = descriptorSets[i][j];
				descriptor.dstBinding = binding++;
				descriptor.dstArrayElement = 0;
				descriptor.descriptorType = buf->type;
				descriptor.descriptorCount = buf->numDescriptors;
				descriptor.pBufferInfo = globalBufferInfo_vs.data();
				descriptor.pImageInfo = nullptr;
				descriptor.pTexelBufferView = nullptr;
				descriptor.pNext = nullptr;

				descriptorWrites.push_back(descriptor);
			}

			for (const auto& ubo : bindSets[j].vsLocal)
			{
				VkWriteDescriptorSet descriptor;
				descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptor.dstSet = descriptorSets[i][j];
				descriptor.dstBinding = binding++;
				descriptor.dstArrayElement = 0;
				descriptor.descriptorType = ubo.type;
				descriptor.descriptorCount = ubo.numDescriptors;
				descriptor.pBufferInfo = bufferInfo_vs.data();
				descriptor.pImageInfo = nullptr;
				descriptor.pTexelBufferView = nullptr;
				descriptor.pNext = nullptr;

				descriptorWrites.push_back(descriptor);
			}

			for (const auto& texSet : bindSets[j].vsTextures)
			{
				VkWriteDescriptorSet descriptor;
				descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptor.dstSet = descriptorSets[i][j];
				descriptor.dstBinding = binding++;
				descriptor.dstArrayElement = 0;
				descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				descriptor.descriptorCount = static_cast<uint32_t>(texSet.size());   // LOOK maybe this can be used instead of the for-loop
				descriptor.pBufferInfo = nullptr;
				descriptor.pImageInfo = imageInfo_vs.data();
				descriptor.pTexelBufferView = nullptr;
				descriptor.pNext = nullptr;

				descriptorWrites.push_back(descriptor);
			}

			for (const auto& ubo : bindSets[j].fsGlobal)
			{
				VkWriteDescriptorSet descriptor;
				descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptor.dstSet = descriptorSets[i][j];
				descriptor.dstBinding = binding++;
				descriptor.dstArrayElement = 0;
				descriptor.descriptorType = ubo->type;
				descriptor.descriptorCount = ubo->numDescriptors;
				descriptor.pBufferInfo = globalBufferInfo_fs.data();
				descriptor.pImageInfo = nullptr;
				descriptor.pTexelBufferView = nullptr;
				descriptor.pNext = nullptr;

				descriptorWrites.push_back(descriptor);
			}

			for (const auto& ubo : bindSets[j].fsLocal)
			{
				VkWriteDescriptorSet descriptor;
				descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptor.dstSet = descriptorSets[i][j];
				descriptor.dstBinding = binding++;
				descriptor.dstArrayElement = 0;
				descriptor.descriptorType = ubo.type;
				descriptor.descriptorCount = ubo.numDescriptors;
				descriptor.pBufferInfo = bufferInfo_fs.data();
				descriptor.pImageInfo = nullptr;
				descriptor.pTexelBufferView = nullptr;
				descriptor.pNext = nullptr;

				descriptorWrites.push_back(descriptor);
			}

			for (const auto& texSet : bindSets[j].fsTextures)
			{
				VkWriteDescriptorSet descriptor;
				descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptor.dstSet = descriptorSets[i][j];
				descriptor.dstBinding = binding++;
				descriptor.dstArrayElement = 0;
				descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				descriptor.descriptorCount = static_cast<uint32_t>(texSet.size());   // LOOK maybe this can be used instead of the for-loop
				descriptor.pBufferInfo = nullptr;
				descriptor.pImageInfo = imageInfo_fs.data();
				descriptor.pTexelBufferView = nullptr;
				descriptor.pNext = nullptr;

				descriptorWrites.push_back(descriptor);
			}

			if (!j && r->rp->getSubpass(renderPassIndex, subpassIndex).inputAtts.size())
			{
				VkWriteDescriptorSet descriptor;
				descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptor.dstSet = descriptorSets[i][j];
				descriptor.dstBinding = binding++;
				descriptor.dstArrayElement = 0;
				descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;	// VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
				descriptor.descriptorCount = static_cast<uint32_t>(inputAttachInfo.size());
				descriptor.pBufferInfo = nullptr;
				descriptor.pImageInfo = inputAttachInfo.data();
				descriptor.pTexelBufferView = nullptr;
				descriptor.pNext = nullptr;

				descriptorWrites.push_back(descriptor);
			}

			vkUpdateDescriptorSets(r->c.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);	// Accepts 2 kinds of arrays as parameters: VkWriteDescriptorSet, VkCopyDescriptorSet.
		}
}

void ModelData::recreate_pipeline_and_descriptors()
{
	#ifdef DEBUG_MODELS
		std::cout << typeid(*this).name() << "::" << __func__ << " (" << name << ')' << std::endl;
	#endif

	createGraphicsPipeline();   // Recreate graphics pipeline because viewport and scissor rectangle size is specified during graphics pipeline creation (this can be avoided by using dynamic state for the viewport and scissor rectangles).

	//binds.createBuffers();   //<<< Necessary?   Uniform buffers depend on the number of swap chain images.
	createDescriptorPool();   // Descriptor pool depends on the swap chain images.
	createDescriptorSets();   // Descriptor sets
}

void ModelData::cleanup_pipeline_and_descriptors()
{
	#ifdef DEBUG_MODELS
		std::cout << typeid(*this).name() << "::" << __func__ << " (" << name << ')' << std::endl;
	#endif
	
	// Graphics pipeline
	vkDestroyPipeline(r->c.device, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(r->c.device, pipelineLayout, nullptr);

	// Uniform buffers & memory
	//binds.destroyBuffers();

	// Descriptor pool & Descriptor set (When a descriptor pool is destroyed, all descriptor-sets allocated from the pool are implicitly/automatically freed and become invalid)
	vkDestroyDescriptorPool(r->c.device, descriptorPool, nullptr);
}

void ModelData::deleteLoader()
{
	if (resLoader)
	{
		delete resLoader;
		resLoader = nullptr;
	}
}

bool ModelData::setNumInstances(uint32_t count)
{
	#ifdef DEBUG_MODELS
		std::cout << typeid(*this).name() << "::" << __func__ << " (" << name << ')' << std::endl;
	#endif

	if (count == numInstances) return false;
		
	numInstances = count;
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

uint32_t ModelData::getNumInstances() const { return numInstances; }

ModelsManager::ModelsManager(const std::shared_ptr<RenderPipeline>& renderPipeline) :
	newKey(0)
{
	keys.resize(renderPipeline->renderPasses.size());
	for (size_t rp = 0; rp < keys.size(); rp++)
		keys[rp].resize(renderPipeline->renderPasses[rp].subpasses.size());
}

void ModelsManager::distributeKeys()
{
	for (auto& rp : keys)
		for (auto& sp : rp)
			sp.clear();

	for (auto it = data.begin(); it != data.end(); it++)
		if (it->second.getNumInstances() && it->second.ready)
			keys[it->second.renderPassIndex][it->second.subpassIndex].push_back(it->first);
}

key64 ModelsManager::getNewKey()
{
	do
	{
		if (data.find(++newKey) == data.end())
			return newKey;
	} while (true);
}

void ModelsManager::cleanup_pipelines_and_descriptors(std::mutex* waitMutex)
{
	if (waitMutex)
		const std::lock_guard<std::mutex> lock(*waitMutex);

	for (auto it = data.begin(); it != data.end(); it++)
		it->second.cleanup_pipeline_and_descriptors();
}

void ModelsManager::create_pipelines_and_descriptors(std::mutex* waitMutex)
{
	if (waitMutex)
		const std::lock_guard<std::mutex> lock(*waitMutex);

	for (auto it = data.begin(); it != data.end(); it++)
		it->second.recreate_pipeline_and_descriptors();
}

ModelSet::ModelSet(Renderer& ren, std::vector<key64> keyList, uint32_t numInstances, uint32_t maxNumInstances)
	: r(&ren), numInstances(numInstances), maxNumInstances(maxNumInstances)
{
	if (keyList.empty()) throw std::runtime_error("ModelSet must contain one or more models");

	for (const auto key : keyList)
		models.insert(key);

	setNumInstances(numInstances);
}

ModelSet::ModelSet(Renderer& ren, key64 key, uint32_t numInstances, uint32_t maxNumInstances)
	: r(&ren), numInstances(numInstances), maxNumInstances(maxNumInstances)
{
	models.insert(key);

	setNumInstances(numInstances);
}

void ModelSet::setNumInstances(uint32_t count)
{
	if (count == numInstances) return;

	if (count > maxNumInstances)
	{
		std::string firstModelName = r->getModel(*models.begin())->name;
		std::cerr << "The number of rendered instances (" << firstModelName << ") cannot be higher than " << maxNumInstances << std::endl;
		count = maxNumInstances;
	}

	for (auto it = models.begin(); it != models.end(); it++)
		r->getModel(*it)->setNumInstances(count);

	numInstances = count;
}

uint32_t ModelSet::getNumInstances() const { return numInstances; }

bool ModelSet::fullyConstructed()
{
	for (auto it = models.begin(); it != models.end(); it++)
		if (r->getModel(*it)->fullyConstructed == false) return false;

	return true;
}

bool ModelSet::ready()
{
	for (auto it = models.begin(); it != models.end(); it++)
		if (r->getModel(*it)->ready == false) return false;

	return true;
}

size_t ModelSet::size() { return models.size(); }

std::unordered_set<key64>::iterator ModelSet::begin() { return models.begin(); }

std::unordered_set<key64>::iterator ModelSet::end() { return models.end(); }