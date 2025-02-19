#include <iostream>
#include <map>				// std::multimap<key, value>
#include <set>				// std::set<uint32_t>
//#include <memory>			// std::unique_ptr, std::shared_ptr (used instead of RAII)
#include <stdexcept>
#include <array>

#include "polygonum/environment.hpp"
#include "polygonum/models.hpp"


bool QueueFamilyIndices::isComplete()
{
	return	graphicsFamily.has_value() && presentFamily.has_value();
}

Image::Image(VulkanCore& core, VkImage image, VkDeviceMemory memory, VkImageView view, VkSampler sampler) :
	c(core), image(image), memory(memory), view(view), sampler(sampler) { }

void Image::createFullImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImageAspectFlags aspectFlags)
{
	c.createImage(image, memory, width, height, mipLevels, numSamples, format, tiling, usage, properties);
	c.createImageView(view, image, format, aspectFlags, mipLevels);
}

void Image::createSampler(VkSamplerCreateInfo& samplerInfo)
{
	c.createSampler(sampler, samplerInfo);
}

void Image::destroy()
{
	c.destroyImage(this);
}

void VulkanCore::createImage(VkImage& destImage, VkDeviceMemory& destMemory, uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
{
	// Create image objects for letting the shader access the pixel values (better option than setting up the shader to access the pixel values in the buffer). Pixels within an image object are known as texels.
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;				// Kind of coordinate system the texels in the image are going to be addressed: 1D (to store an array of data or gradient...), 2D (textures...), 3D (to store voxel volumes...).
	imageInfo.extent.width = width;						// Number of texels in X									
	imageInfo.extent.height = height;					// Number of texels in Y
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = mipLevels;					// Number of levels (mipmaps)
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;							// Same format for the texels as the pixels in the buffer. This format is widespread, but if it is not supported by the graphics hardware, you should go with the best supported alternative.
	imageInfo.tiling = tiling;							// This cannot be changed later. VK_IMAGE_TILING_ ... LINEAR (texels are laid out in row-major order like our pixels array), OPTIMAL (texels are laid out in an implementation defined order for optimal access).
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;// VK_IMAGE_LAYOUT_ ... UNDEFINED (not usable by the GPU and the very first transition will discard the texels), PREINITIALIZED (not usable by the GPU, but the first transition will preserve the texels). We choose UNDEFINED because we're first going to transition the image to be a transfer destination and then copy texel data to it from a buffer object. There are few situations where PREINITIALIZED is necessary (example: when we want to use an image as a staging image in combination with the VK_IMAGE_TILING_LINEAR layout). 
	imageInfo.usage = usage;							// The image is going to be used as destination for the buffer copy, so it should be set up as a transfer destination; and we also want to be able to access the image from the shader to color our mesh.
	imageInfo.samples = numSamples;						// For multisampling. Only relevant for images that will be used as attachments.
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;	// The image will only be used by one queue family: the one that supports graphics (and therefore also) transfer operations.
	imageInfo.flags = 0;								// [Optional]  There are some optional flags for images that are related to sparse images (images where only certain regions are actually backed by memory). Example: If you were using a 3D texture for a voxel terrain, then you could use this to avoid allocating memory to store large volumes of "air" values.

	if (vkCreateImage(device, &imageInfo, nullptr, &destImage) != VK_SUCCESS)
		throw std::runtime_error("Failed to create image!");

	// Allocate memory for the image
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, destImage, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &destMemory) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate image memory!");

	memAllocObjects++;

	vkBindImageMemory(device, destImage, destMemory, 0);
}

void VulkanCore::createImageView(VkImageView& destImageView, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels)
{
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;			// 1D, 2D, 3D, or cube map
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;						// Image's purpose and which part of it should be accessed. Here, our images will be used as color targets without any mipmapping levels or multiple layers. 
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;	// Default color mapping
	viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	// Note about stereographic 3D applications: For them, you would create a swap chain with multiple layers, and then create multiple image views for each image (one for left eye and another for right eye).

	if (vkCreateImageView(device, &viewInfo, nullptr, &destImageView) != VK_SUCCESS)
		throw std::runtime_error("Failed to create texture image view!");
}

void VulkanCore::createSampler(VkSampler& destSampler, VkSamplerCreateInfo& samplerInfo)
{
	if (vkCreateSampler(device, &samplerInfo, nullptr, &destSampler) != VK_SUCCESS)
		throw std::runtime_error("Failed to create sampler!");
}

void VulkanCore::destroyImage(Image* image)
{
	if (image->view)
		vkDestroyImageView(device, image->view, nullptr);   // Resolve buffer	(VkImageView)
	if (image->image)
		vkDestroyImage(device, image->image, nullptr);   // Resolve buffer	(VkImage)
	if (image->memory) {
		vkFreeMemory(device, image->memory, nullptr);   // Resolve buffer	(VkDeviceMemory)
		memAllocObjects--;
	}
	if (image->sampler)
		vkDestroySampler(device, image->sampler, nullptr);
}

SwapChain::SwapChain(VulkanCore& core, uint32_t additionalSwapChainImages)
	: c(core), swapChain(nullptr), imageFormat(VK_FORMAT_UNDEFINED), extent(VkExtent2D{0,0}), additionalSwapChainImages(additionalSwapChainImages)
{
	createSwapChain();
}

void SwapChain::destroy()
{
	// Swap chain image views
	for (auto imageView : views)
		vkDestroyImageView(c.device, imageView, nullptr);

	// Swap chain
	vkDestroySwapchainKHR(c.device, swapChain, nullptr);
}

size_t SwapChain::numImages() { return images.size(); }

void DeviceData::fillWithDeviceData(VkPhysicalDevice physicalDevice)
{
	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
	vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
	
	deviceType = deviceProperties.deviceType;
	apiVersion = deviceProperties.apiVersion;
	driverVersion = deviceProperties.driverVersion;
	vendorID = deviceProperties.vendorID;
	deviceID = deviceProperties.deviceID;
	deviceType = deviceProperties.deviceType;
	deviceName = std::string(deviceProperties.deviceName);

	maxUniformBufferRange = deviceProperties.limits.maxUniformBufferRange;
	maxPerStageDescriptorUniformBuffers = deviceProperties.limits.maxPerStageDescriptorUniformBuffers;
	maxDescriptorSetUniformBuffers = deviceProperties.limits.maxDescriptorSetUniformBuffers;
	maxImageDimension2D = deviceProperties.limits.maxImageDimension2D;
	maxMemoryAllocationCount = deviceProperties.limits.maxMemoryAllocationCount;
	framebufferColorSampleCounts = deviceProperties.limits.framebufferColorSampleCounts;
	framebufferDepthSampleCounts = deviceProperties.limits.framebufferDepthSampleCounts;
	minUniformBufferOffsetAlignment = deviceProperties.limits.minUniformBufferOffsetAlignment;

	samplerAnisotropy = deviceFeatures.samplerAnisotropy;
	largePoints = deviceFeatures.largePoints;
	wideLines = deviceFeatures.wideLines;

	/// Find the right format for a depth image. Select a format with a depth component that supports usage as depth attachment. We don't need a specific format because we won't be directly accessing the texels from the program. It just needs to have a reasonable accuracy (usually, at least 24 bits). Several formats fit this requirement: VK_FORMAT_ ... D32_SFLOAT (32-bit signed float depth), D32_SFLOAT_S8_UINT (32-bit signed float depth and 8 bit stencil), D24_UNORM_S8_UINT (24-bit float depth and 8 bit stencil).
	depthFormat = findSupportedFormat(physicalDevice,
						{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
						VK_IMAGE_TILING_OPTIMAL,
						VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkFormat DeviceData::findSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates)
	{
		VkFormatProperties props;												// Contains 3 fields: linearTilingFeatures (linear tiling), optimalTilingFeatures (optimal tiling), bufferFeatures (buffers).
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);	// Query the support of a format

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
			return format;
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
			return format;
	}

	throw std::runtime_error("Failed to find supported format!");
}

void DeviceData::printData()
{
	std::cout
		<< "Device data:" << '\n'
		<< "   apiVersion: " << apiVersion << '\n'
		<< "   driverVersion: " << driverVersion << '\n'
		<< "   vendorID: " << vendorID << '\n'
		<< "   deviceID: " << deviceID << '\n'
		<< "   deviceType: " << deviceType << '\n'
		<< "   deviceName: " << deviceName << '\n'
			   
		<< "   maxUniformBufferRange: " << maxUniformBufferRange << '\n'
		<< "   maxPerStageDescriptorUniformBuffers: " << maxPerStageDescriptorUniformBuffers << '\n'
		<< "   maxDescriptorSetUniformBuffers: " << maxDescriptorSetUniformBuffers << '\n'
		<< "   maxImageDimension2D: " << maxImageDimension2D << '\n'
		<< "   maxMemoryAllocationCount: " << maxMemoryAllocationCount << '\n'
		<< "   framebufferColorSampleCounts: " << framebufferColorSampleCounts << '\n'
		<< "   framebufferDepthSampleCounts: " << framebufferDepthSampleCounts << '\n'
		<< "   minUniformBufferOffsetAlignment: " << minUniformBufferOffsetAlignment << '\n'
			   
		<< "   samplerAnisotropy: " << samplerAnisotropy << '\n'
		<< "   largePoints: " << largePoints << '\n'
		<< "   wideLines: " << wideLines << '\n'

		<< "   depthFormat: " << depthFormat << '\n';
}

Commander::Commander(VulkanCore& core, size_t swapChainImagesCount, size_t maxFramesInFlight) :
	c(core),
	lastFrame(0),
	swapChainImagesCount(swapChainImagesCount),
	maxFramesInFlight(maxFramesInFlight),
	commandPools(maxFramesInFlight),
	commandBuffers(maxFramesInFlight),
	mutCommandPool(maxFramesInFlight),
	mutFrame(maxFramesInFlight),
	updateCommandBuffer(false),
	commandsCount(0)
{
#if defined(DEBUG_RENDERER) || defined(DEBUG_COMMANDBUFFERS)
	std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
#endif

	createSynchronizers(swapChainImagesCount, maxFramesInFlight);
	createCommandPool(maxFramesInFlight);
	createCommandBuffers(swapChainImagesCount, maxFramesInFlight);
}

void Commander::createSynchronizers(size_t numSwapchainImages, size_t numFrames)
{
	// Create synchronization objects (semaphores and fences for synchronizing the events occuring in each frame at drawFrame()).
	imageAvailableSemaphores.resize(numFrames);
	renderFinishedSemaphores.resize(numFrames);
	framesInFlight.resize(numFrames);
	imagesInFlight.resize(numSwapchainImages, { VK_NULL_HANDLE, 0 });

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;						// Reset to signaled state (CB finished execution)

	for (size_t i = 0; i < numFrames; i++)
		if (vkCreateSemaphore(c.device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(c.device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(c.device, &fenceInfo, nullptr, &framesInFlight[i]) != VK_SUCCESS)
			throw std::runtime_error("Failed to create synchronization objects for a frame!");
}

void Commander::updateCommandBuffers(ModelsManager& models, std::shared_ptr<RenderPipeline> renderPipeline, size_t swapChainImagesCount, size_t frameIndex)
{
#if defined(DEBUG_RENDERER) || defined(DEBUG_COMMANDBUFFERS)
	std::cout << typeid(*this).name() << "::" << __func__ << "(" << frameIndex << ") BEGIN" << std::endl;
#endif

	commandsCount = 0;
	VkDeviceSize offsets[] = { 0 };
	ModelData* model;
	std::vector<VkCommandBuffer>& CBs = commandBuffers[frameIndex];   // Take command buffers of this frame (one per swapchain).

	// Start command buffer recording
	for (size_t i = 0; i < CBs.size(); i++)		// for each SWAPCHAIN IMAGE
	{
#ifdef DEBUG_COMMANDBUFFERS
	std::cout << "  Command buffer " << i << std::endl;
#endif

		if(vkResetCommandBuffer(CBs[i], 0) != VK_SUCCESS)
			throw std::runtime_error("Failed to reset command buffer!");

		// Start command buffer recording
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;			// [Optional] VK_COMMAND_BUFFER_USAGE_ ... ONE_TIME_SUBMIT_BIT (the command buffer will be rerecorded right after executing it once), RENDER_PASS_CONTINUE_BIT (secondary command buffer that will be entirely within a single render pass), SIMULTANEOUS_USE_BIT (the command buffer can be resubmitted while it is also already pending execution).
		beginInfo.pInheritanceInfo = nullptr;		// [Optional] Only relevant for secondary command buffers. It specifies which state to inherit from the calling primary command buffers.

		if (vkBeginCommandBuffer(CBs[i], &beginInfo) != VK_SUCCESS)		// If a command buffer was already recorded once, this call resets it. It's not possible to append commands to a buffer at a later time.
			throw std::runtime_error("Failed to begin recording command buffer!");

		for (size_t rp = 0; rp < models.keys.size(); rp++)		// for each RENDER PASS (color pass, post-processing...)
		{
#ifdef DEBUG_COMMANDBUFFERS
	std::cout << "    Render pass " << rp << std::endl;
#endif

			vkCmdBeginRenderPass(CBs[i], &renderPipeline->renderPasses[rp].renderPassInfos[i], VK_SUBPASS_CONTENTS_INLINE);	// Start RENDER PASS. VK_SUBPASS_CONTENTS_INLINE (the render pass commands will be embedded in the primary command buffer itself and no secondary command buffers will be executed), VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS (the render pass commands will be executed from secondary command buffers).
			//vkCmdNextSubpass(CBs[i], VK_SUBPASS_CONTENTS_INLINE);						// Start SUBPASS

			for (size_t sp = 0; sp < models.keys[rp].size(); sp++)		// for each SUB-PASS
			{
				if (sp > 0) vkCmdNextSubpass(CBs[i], VK_SUBPASS_CONTENTS_INLINE);   // Start SUBPASS
				//clearDepthBuffer(CBs[i]);		// Already done in createRenderPass() (loadOp). Previously used for implementing layers (Painter's algorithm).

				for (key64 key : models.keys[rp][sp])		// for each MODEL
				{
#ifdef DEBUG_COMMANDBUFFERS
	std::cout << "        Model: " << models.data[key].name << std::endl;
#endif

					model = &models.data[key];
					if (model->getActiveInstancesCount() == 0) continue;

					vkCmdBindPipeline(CBs[i], VK_PIPELINE_BIND_POINT_GRAPHICS, model->graphicsPipeline);	// Second parameter: Specifies if the pipeline object is a graphics or compute pipeline.
					vkCmdBindVertexBuffers(CBs[i], 0, 1, &model->vert.vertexBuffer, offsets);

					if (model->vert.indexCount)		// has indices (it doesn't if data represents points)
						vkCmdBindIndexBuffer(CBs[i], model->vert.indexBuffer, 0, VK_INDEX_TYPE_UINT16);

					if (model->descriptorSets.size())	// has descriptor set (UBOs, textures, input attachments)
						vkCmdBindDescriptorSets(CBs[i], VK_PIPELINE_BIND_POINT_GRAPHICS, model->pipelineLayout, 0, 1, &model->descriptorSets[i], 0, 0);

					if (model->vert.indexCount)		// has indices
						vkCmdDrawIndexed(CBs[i], static_cast<uint32_t>(model->vert.indexCount), model->getActiveInstancesCount(), 0, 0, 0);
					else
						vkCmdDraw(CBs[i], model->vert.vertexCount, model->getActiveInstancesCount(), 0, 0);

					commandsCount++;
				}
			}

			vkCmdEndRenderPass(CBs[i]);
		}

		if (vkEndCommandBuffer(CBs[i]) != VK_SUCCESS)
			throw std::runtime_error("Failed to record command buffer!");
	}

	updateCommandBuffer = false;

#if defined(DEBUG_RENDERER) || defined(DEBUG_COMMANDBUFFERS)
	std::cout << typeid(*this).name() << "::" << __func__ << " END" << std::endl;
#endif
}

VulkanCore::VulkanCore(int width, int height)
	: io(width, height), physicalDevice(VK_NULL_HANDLE), msaaSamples(VK_SAMPLE_COUNT_1_BIT), memAllocObjects(0)
{
	#ifdef DEBUG_ENV_CORE
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	//initWindow();
	createInstance();
	valLayers.setupDebugMessenger(instance);
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
}

// (1) 
/// Describe application, select extensions and validation layers, create Vulkan instance (stores application state).
void VulkanCore::createInstance()
{
	#ifdef DEBUG_ENV_CORE
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	// [Optional] Tell the compiler some info about the instance to create (used for optimization)
	VkApplicationInfo appInfo{};

	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Renderer";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Graphox";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;
	appInfo.pNext = nullptr;					// pointer to extension information

	// Not optional. Tell the compiler the global extensions and validation layers we will use (applicable to the entire program, not a specific device)
	VkInstanceCreateInfo createInfo{};

	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
	if (enableValidationLayers)
	{
		createInfo.ppEnabledLayerNames = valLayers.requiredValidationLayers.data();
		createInfo.enabledLayerCount = static_cast<uint32_t>(valLayers.requiredValidationLayers.size());
		valLayers.populateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}

	auto extensions = ext.getRequiredExtensions_glfw_valLayers();
	createInfo.ppEnabledExtensionNames = extensions.data();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());

	// Check for extension support
	if (!ext.checkExtensionSupport(createInfo.ppEnabledExtensionNames, createInfo.enabledExtensionCount))
		throw std::runtime_error("Extensions requested, but not available!");

	// Create the instance
	if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
		throw std::runtime_error("Failed to create instance!");
}

bool ValLayers::checkValidationLayerSupport()
{
	// Number of layers available
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	// Names of the available layers
	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	// Print "requiredLayers" and "availableLayers"
	#ifdef DEBUG_ENV_INFO
		std::cout << "   Required validation layers: \n";
		for (size_t i = 0; i < requiredValidationLayers.size(); ++i)
			std::cout << "      " << requiredValidationLayers[i] << '\n';

		std::cout << "   Available validation layers: \n";
		for (size_t i = 0; i < layerCount; ++i)
			std::cout << "      " << availableLayers[i].layerName << '\n';
	#endif

	// Check if all the "requiredLayers" exist in "availableLayers"
	for (const char* reqLayer : requiredValidationLayers)
	{
		bool layerFound = false;
		for (const auto& layerProperties : availableLayers)
		{
			if (std::strcmp(reqLayer, layerProperties.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}
		if (!layerFound)
			return false;		// If any layer is not found, returns false
	}

	return true;				// If all layers are found, returns true
}

/**
*	@brief Specify the details about the messenger and its callback.
* 
*	Specify the details about the messenger and its callback
*	There are a lot more settings for the behavior of validation layers than just the flags specified in the
*	VkDebugUtilsMessengerCreateInfoEXT struct. The file "$VULKAN_SDK/Config/vk_layer_settings.txt" explains how to configure the layers.
*	@param createInfo Struct that this method will use for setting the type of messages to receive, and the callback function.
 */
void ValLayers::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo = {};
	// - Type of the struct
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	// - Specify the types of severities you would like your callback to be called for.
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	// - Specify the types of messages your callback is notified about.
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	// - Specify the pointer to the callback function.
	createInfo.pfnUserCallback = debugCallback;
	// - [Optional] Pass a pointer to the callback function through this parameter
	createInfo.pUserData = nullptr;
}

/**
	@brief Callback for handling ourselves the validation layer's debug messages and decide which kind of messages to see.

   The validation layers will print debug messages to the standard output by default.
   But by providing a callback we can handle them ourselves and decide which kind of messages to see.
   This callback function is added with the PFN_vkDebugUtilsMessengerCallbackEXT prototype.
   The VKAPI_ATTR and VKAPI_CALL ensure that the function has the right signature for Vulkan to call it.
   @param messageSeverity Specifies the severity of the message, which is one of the following flags (it's possible to use comparison operations between them):
		<ul>
			<li>VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: Diagnostic message.</li>
			<li>VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: Informational message (such as the creation of a resource).</li>
			<li>VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: Message about behavior (not necessarily an error, but very likely a bug).</li>
			<li>VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: Message about behavior that is invalid and may cause crashes.</li>
		</ul>
   @param messageType Can have the following values:
		<ul>
			<li>VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT: Some event happened that is unrelated to the specification or performance.</li>
			<li>VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT: Something happened that violates the specification or indicates a possible mistake.</li>
			<li>VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: Potential non-optimal use of Vulkan.</li>
		</ul>
   @param pCallbackData It refers to a VkDebugUtilsMessengerCallbackDataEXT struct containing the details of the message. Some important members:
		<ul>
			<li>pMessage: Debug message as null-terminated string.</li>
			<li>pObjects: Array of Vulkan object handles related to the message.</li>
			<li>objectCount: Number of objects in the array.</li>
		</ul>
   @param pUserData Pointer (specified during the setup of the callback) that allows you to pass your own data.
   @return Boolean indicating if the Vulkan call that triggered the validation layer message should be aborted. If true, the call is aborted with the VK_ERROR_VALIDATION_FAILED_EXT error.
 */
VKAPI_ATTR VkBool32 VKAPI_CALL ValLayers::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	//std::cerr << "Validation layer: (" << messageSeverity << '/' << messageType << ") " << pCallbackData->pMessage << std::endl;
	if (messageType != 1)			// Avoid GENERAL_BIT messages (unrelated to the specification or performance).
		std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

std::vector<const char*> Extensions::getRequiredExtensions_device()
{
	const std::vector<const char*> requiredDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	return requiredDeviceExtensions;
}

/// Get a list of required extensions (based on whether validation layers are enabled or not)
std::vector<const char*> Extensions::getRequiredExtensions_glfw_valLayers()
{
	// Get required extensions (glfwExtensions)
	const char** glfwExtensions;
	uint32_t glfwExtensionCount = 0;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	// Store them in a vector
	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	// Add additional optional extensions

	// > VK_EXT_DEBUG_UTILS_EXTENSION_NAME == "VK_EXT_debug_utils". 
	// This extension is needed, together with a debug messenger, to set up a callback to handle messages and associated details
	if (enableValidationLayers)
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	return extensions;
}

bool Extensions::checkExtensionSupport(const char* const* requiredExtensions, uint32_t reqExtCount)
{
	// Number of extensions available
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	// Names of the available extensions
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

	// Print "requiredExtensions" and "availableExtensions"
	#ifdef DEBUG_ENV_INFO
		std::cout << "   Required extensions: \n";
		for (size_t i = 0; i < reqExtCount; ++i)
			std::cout << "      " << requiredExtensions[i] << '\n';

		std::cout << "   Available extensions: \n";
		for (size_t i = 0; i < extensionCount; ++i)
			std::cout << "      " << availableExtensions[i].extensionName << '\n';
	#endif

	// Check if all the "requiredExtensions" exist in "availableExtensions"
	for (size_t i = 0; i < reqExtCount; ++i)
	{
		bool extensionFound = false;
		for (const auto& extensionProperties : availableExtensions)
		{
			if (std::strcmp(requiredExtensions[i], extensionProperties.extensionName) == 0)
			{
				extensionFound = true;
				break;
			}
		}
		if (!extensionFound)
			return false;		// If any extension is not found, returns false
	}

	return true;				// If all extensions are found, returns true
}

ValLayers::ValLayers() : instance(nullptr), debugMessenger(nullptr)
{
	// Check validation layer support
	if (enableValidationLayers && !checkValidationLayerSupport())
		throw std::runtime_error("Validation layers requested, but not available!");
}

// (2)
/**
	@brief Specify the details about the messenger and its callback, and create the debug messenger.

	Specify the details about the messenger and its callback (there are more ways to configure validation layer messages and debug callbacks), and create the debug messenger.
 */
void ValLayers::setupDebugMessenger(VkInstance instance)
{
	#ifdef DEBUG_ENV_CORE
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	if (!enableValidationLayers) return;

	this->instance = instance;

	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	populateDebugMessengerCreateInfo(createInfo);   // Fill in a structure with details about the messenger and its callback

	if (CreateDebugUtilsMessengerEXT(&createInfo, nullptr, &debugMessenger) != VK_SUCCESS)   // Create the debug messenger
		throw std::runtime_error("Failed to set up debug messenger!");
}

/**
 * Given a VkDebugUtilsMessengerCreateInfoEXT object, creates/loads the extension object (debug messenger) (VkDebugUtilsMessengerEXT) if it's available.
 * Because it is an extension function, it is not automatically loaded. So, we have to look up its address ourselves using vkGetInstanceProcAddr.
 * @param instance Vulkan instance (the debug messenger is specific to our Vulkan instance and its layers)
 * @param pCreateInfo VkDebugUtilsMessengerCreateInfoEXT object
 * @param pAllocator Optional allocator callback
 * @param pDebugMessenger Debug messenger object
 * @return Returns the extension object, or an error if is not available.
 */
VkResult ValLayers::CreateDebugUtilsMessengerEXT(const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	// Load the extension object if it's available (the extension function needs to be explicitly loaded)
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

	// vkGetInstanceProcAddr returns nullptr is the function couldn't be loaded.
	if (func != nullptr)
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	else
		return VK_ERROR_EXTENSION_NOT_PRESENT;
}

// (3)
/**
	@brief Create a window surface (interface for interacting with the window system)

	Create a window surface (interface for interacting with the window system). Requires to use WSI (Window System Integration), which is provided by GLFW.
 */
void VulkanCore::createSurface()
{
	#ifdef DEBUG_ENV_CORE
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	io.createWindowSurface(instance, nullptr, &surface);

	//if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
	//	throw std::runtime_error("Failed to create window surface!");
}

// (4)
/**
	@brief Look for and select a graphics card in the system that supports the features we need.

	Look for and select a graphics card in the system that supports the features we need (Vulkan support).
 */
void VulkanCore::pickPhysicalDevice()
{
	#ifdef DEBUG_ENV_CORE
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	// Get all devices with Vulkan support.

	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0) throw std::runtime_error("Failed to find GPUs with Vulkan support!");
	else
	{
		#ifdef DEBUG_ENV_INFO
			std::cout << "   Devices with Vulkan support: " << deviceCount << std::endl;
		#endif
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	// Look for the most suitable device and select it.

	std::multimap<int, VkPhysicalDevice> candidates;	// Automatically sorts candidates by score

	for (const auto& device : devices)					// Rate each device
	{
		int score = evaluateDevice(device);
		candidates.insert(std::make_pair(score, device));
	}

	if (candidates.rbegin()->first > 0)					// Check if the best candidate has score > 0
		physicalDevice = candidates.rbegin()->second;
	else
		throw std::runtime_error("Failed to find a suitable GPU!");

	// Save device properties and features.
	deviceData.fillWithDeviceData(physicalDevice);	
	msaaSamples = getMaxUsableSampleCount(!add_MSAA);

	#ifdef DEBUG_ENV_INFO
		deviceData.printData();
		std::cout << "   MSAA samples: " << msaaSamples << std::endl;
	#endif
}

/**
*	@brief Evaluate a device and check if it is suitable for the operations we want to perform.
*
*	Evaluate a device and check if it is suitable for the operations we want to perform.
*	@param device Device to evaluate
*	@param mode Mode of evaluation:
*	@return Score for the device (0 means that the device is not suitable). Dedicated GPUs supporting geometry shaders are favored.
*/
int VulkanCore::evaluateDevice(VkPhysicalDevice device)
{
	// Get basic device properties: Name, type, supported Vulkan version...
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);
	
	// Get optional features: Texture compression, 64 bit floats, multi-viewport rendering...
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	// Get queue families
	QueueFamilyIndices indices = findQueueFamilies(device);

	// Check whether required device extensions are supported 
	bool extensionsSupported = ext.checkDeviceExtensionSupport(device);

	// Check whether swap chain extension is compatible with the window surface (adequately supported)
	bool swapChainAdequate = false;
	if (extensionsSupported)
	{
		SwapChainSupportDetails swapChainSupport = SwapChain::querySwapChainSupport(device, surface);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();	// Adequate if there's at least one supported image format and one supported presentation mode.
	}

	// Score the device
	int score = 0;

	if (!indices.isComplete()) return 0;				// The queue families we want should exist (Computer graphics & Presentation to window surface).
	if (!extensionsSupported) return 0;					// The required device extensions should be supported.
	if (!swapChainAdequate) return 0;					// Swap chain extension support should be adequate (compatible with window surface).
	if (!deviceFeatures.geometryShader) return 0;		// Applications cannot function without geometry shaders.
	score += 1;

	if (deviceFeatures.samplerAnisotropy) score += 1;	// Anisotropic filtering
	if (deviceFeatures.largePoints) score += 1;
	if (deviceFeatures.wideLines) score += 1;
	if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;	// Discrete (dedicated) GPUs have better performance (and integrated GPUs, worse).
	score += deviceProperties.limits.maxImageDimension2D;									// Maximum size of textures.
	
	#ifdef DEBUG_ENV_INFO
		std::cout << "      (" << score << ") " << deviceProperties.deviceName << std::endl;
	#endif

	return score;
}

/**
 * Check which queue families are supported by the device and which one of these supports the commands that we want to use (in this case, graphics commands).
 * Queue families: Any operation (drawing, uploading textures...) requires commands commands to be submitted to a queue. There are different types of queues that originate from different queue families and each family of queues allows only a subset of commands (graphics commands, compute commands, memory transfer related commands...).
 * @param device Device to evaluate
 * @return Structure containing vector indices of the queue families we want.
 */
QueueFamilyIndices VulkanCore::findQueueFamilies(VkPhysicalDevice device)
{
	// Get queue families
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const auto& queueFamily : queueFamilies)
	{
		// Check queue families capable of presenting to our window surface
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
		if (presentSupport) indices.presentFamily = i;

		// Check queue families capable of computer graphics
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			indices.graphicsFamily = i;

		if (indices.isComplete()) break;
		i++;
	}

	return indices;
}

void VulkanCore::destroy()
{
	vkDestroyDevice(device, nullptr);										// Logical device & device queues
	valLayers.DestroyDebugUtilsMessengerEXT();
	vkDestroySurfaceKHR(instance, surface, nullptr);						// Surface KHR
	vkDestroyInstance(instance, nullptr);									// Instance
	io.destroy();
}

void VulkanCore::queueWaitIdle(VkQueue queue, std::mutex* waitMutex)
{
	if(waitMutex) const std::lock_guard<std::mutex> lock(*waitMutex);
	vkQueueWaitIdle(queue);
}

//IOmanager* VulkanCore::getWindowManager() { return &io; }

/**
	Check whether all the required device extensions are supported.
	@param device Device to evaluate
	@return True if all the required device extensions are supported. False otherwise.
*/
bool Extensions::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	auto extensions = getRequiredExtensions_device();
	std::set<std::string> requiredExtensions(extensions.begin(), extensions.end());

	for (const auto& extension : availableExtensions)
		requiredExtensions.erase(extension.extensionName);

	return requiredExtensions.empty();
}

SwapChainSupportDetails SwapChain::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	SwapChainSupportDetails details;

	// Get the basic surface capabilities
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	// Get the supported surface formats
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
	if (formatCount != 0)
	{
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}

	// Get supported presentation modes
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
	if (presentModeCount != 0)
	{
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

/// Get the maximum number of samples (for MSAA) according to the physical device.
VkSampleCountFlagBits VulkanCore::getMaxUsableSampleCount(bool getMinimum)
{
	if (getMinimum) return VK_SAMPLE_COUNT_1_BIT;

	VkSampleCountFlags counts = deviceData.framebufferColorSampleCounts & deviceData.framebufferDepthSampleCounts;

	if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
	if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
	if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
	if (counts & VK_SAMPLE_COUNT_8_BIT ) { return VK_SAMPLE_COUNT_8_BIT;  }
	if (counts & VK_SAMPLE_COUNT_4_BIT ) { return VK_SAMPLE_COUNT_4_BIT;  }
	if (counts & VK_SAMPLE_COUNT_2_BIT ) { return VK_SAMPLE_COUNT_2_BIT;  }
	return VK_SAMPLE_COUNT_1_BIT;
}

// (5)
/// Set up a logical device (describes the features we want to use) to interface with the physical device.
void VulkanCore::createLogicalDevice()
{
	#ifdef DEBUG_ENV_CORE
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	// Get the queue families supported by the physical device.
	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

	// Describe the number of queues you want for each queue family
	std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };
	float queuePriority = 1.0f;
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;		// You can assign priorities to queues to influence the scheduling of command buffer execution using floats in the range [0.0, 1.0]. This is required even if there is only a single queue.

		queueCreateInfos.push_back(queueCreateInfo);
	}

	// Enable the features from the physical device that you will use (geometry shaders...)
	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.samplerAnisotropy = deviceData.samplerAnisotropy ? VK_TRUE : VK_FALSE;	// Anisotropic filtering is an optional device feature (most modern graphics cards support it, but we should check it in isDeviceSuitable)
	deviceFeatures.sampleRateShading = (add_SS ? VK_TRUE : VK_FALSE);						// Enable sample shading feature for the device
	deviceFeatures.wideLines = (deviceData.wideLines ? VK_TRUE : VK_FALSE);					// Enable line width configuration (in VkPipeline)

	// Describe queue parameters
	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pEnabledFeatures = &deviceFeatures;
	auto extensions = ext.getRequiredExtensions_device();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(valLayers.requiredValidationLayers.size());
		createInfo.ppEnabledLayerNames = valLayers.requiredValidationLayers.data();
	}
	else
		createInfo.enabledLayerCount = 0;

	// Create the logical device
	if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
		throw std::runtime_error("Failed to create logical device!");

	// Retrieve queue handles for each queue family (in this case, we created a single queue from each family, so we simply use index 0)
	vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
	vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

// (6)
/// Set up and create the swap chain.
void SwapChain::createSwapChain()
{
	#ifdef DEBUG_ENV_CORE
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	// 1. Create swap chain

	// 1.1. Get some properties
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(c.physicalDevice, c.surface);

	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);	// Surface formats (pixel format, color space)
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);	// Presentation modes
	VkExtent2D extent = chooseSwapExtent(c.io, swapChainSupport.capabilities);		// Basic surface capabilities

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + additionalSwapChainImages;		// How many images in the swap chain? We choose the minimum required + 1 (this way, we won't have to wait sometimes on the driver to complete internal operations before we can acquire another image to render to.

	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)	// Don't exceed max. number of images (if maxImageCount == 0, there is no maximum)
		imageCount = swapChainSupport.capabilities.maxImageCount;

	// 1.2. Configure the swap chain
	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = c.surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;								// Number of layers each image consists of (always 1, except for stereoscopic 3D applications)
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;	// Kind of operations we'll use the images in the swap chain for. VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT let us render directly to the swap chain. VK_IMAGE_USAGE_TRANSFER_DST_BIT let us render images to a separate image ifrst to perform operations like post-processing and use memory operation to transfer the rendered image to a swap chain image. 

	QueueFamilyIndices indices = c.findQueueFamilies(c.physicalDevice);
	uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	if (indices.graphicsFamily != indices.presentFamily)			// Specify how to handle swap chain images that will be used across multiple queue families. This will be the case if the graphics queue family is different from the presentation queue (draws on the images in the swap chain from the graphics queue and submits them on the presentation queue).
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;	// Best performance. An image is owned by one queue family at a time and ownership must be explicitly transferred before using it in another queue family.
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;	// Images can be used across multiple queue families without explicit ownership transfers.
		createInfo.queueFamilyIndexCount = 0;						// Optional
		createInfo.pQueueFamilyIndices = nullptr;					// Optional
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;	// currentTransform specifies that you don't want any transformation ot be applied to images in the swap chain.
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;				// Specify if the alpha channel should be used for blending with other windows in the window system. VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR makes it ignore the alpha channel.
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;												// If VK_TRUE, we don't care about colors of pixels that are obscured (example, because another window is in front of them).
	createInfo.oldSwapchain = VK_NULL_HANDLE;									// It's possible that your swap chain becomes invalid/unoptimized while the application is running (example: window resize), so your swap chain will need to be recreated from scratch and a reference to the old one must be specified in this field.

	// 1.3. Create swap chain
	if (vkCreateSwapchainKHR(c.device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
		throw std::runtime_error("Failed to create swap chain!");

	// 1.4. Retrieve the handles
	vkGetSwapchainImagesKHR(c.device, swapChain, &imageCount, nullptr);
	images.resize(imageCount);
	vkGetSwapchainImagesKHR(c.device, swapChain, &imageCount, images.data());
	
	#ifdef DEBUG_ENV_INFO
		std::cout << "   Swap chain images: " << images.size() << std::endl;
	#endif

	// 1.5. Save format and extent for future use
	imageFormat = surfaceFormat.format;
	this->extent = extent;

	// 2. Create swap chain image views (create a basic image view for every image in the swap chain so that we can use them as color targets later on).

	views.resize(images.size());

	for (uint32_t i = 0; i < images.size(); i++)
		c.createImageView(views[i], images[i], imageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
}

/// Chooses the surface format (color depth) for the swap chain.
VkSurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
	// Return our favourite surface format, if it exists
	for (const auto& availableFormat : availableFormats)
	{
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&				// Format: Color channel and types (example: VK_FORMAT_B8G8R8A8_SRGB is BGRA channels with 8 bit unsigned integer)
			availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)	// Color space: Indicates if the sRGB color space is supported or not (https://stackoverflow.com/questions/12524623/what-are-the-practical-differences-when-working-with-colors-in-a-linear-vs-a-no) (https://stackoverflow.com/questions/66401081/vulkan-swapchain-format-unorm-vs-srgb).
			return availableFormat;
	}
	
	// Otherwise, return the first format founded (other ways: rank the available formats on how "good" they are)
	std::cout << "Favorite surface format not found" << std::endl;
	return availableFormats[0];
}

/// Chooses the swap extent (resolution of images in swap chain) for the swap chain.
/** The swap extent is set here. The swap extent is the resolution (in pixels) of the swap chain images, which is almost always equal to the resolution of the window where we are drawing (use {WIDHT, HEIGHT}), except when you're using a high DPI display (then, use glfwGetFramebufferSize). */
VkExtent2D SwapChain::chooseSwapExtent(IOmanager& io, const VkSurfaceCapabilitiesKHR& capabilities)
{
	// If width and height is set to the maximum value of UINT32_MAX, it indicates that the surface size will be determined by the extent of a swapchain targeting the surface. 
	if (capabilities.currentExtent.width != UINT32_MAX)
		return capabilities.currentExtent;

	// Set width and height (useful when you're using a high DPI display)
	else
	{
		int width, height;
		io.getFramebufferSize(&width, &height);

		VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

		actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}

/**
*	@brief Chooses the presentation mode (conditions for "swapping" images to the screen) for the swap chain.
* 
* Presentation mode represents the conditions for showing images to the screen. Four possible modes available in Vulkan:
* 		<ul>
			<li>VK_PRESENT_MODE_IMMEDIATE_KHR: Images submitted by your application are transferred to the screen right away (may cause tearing).</li>
			<li>VK_PRESENT_MODE_FIFO_KHR: The swap chain is a FIFO queue. The display takes images from the front. The program inserts images at the back. This is most similar to vertical sync as found in modern games. This is the only mode guaranteed to be available.</li>
			<li>VK_PRESENT_MODE_FIFO_RELAXED_KHR: Like the second mode, with one more property: If the application is late and the queue was empty at the last vertical blank (moment when the display is refreshed), instead of waiting for the next vertical blank, the image is transferred right away when it finally arrives (may cause tearing).</li>
			<li>VK_PRESENT_MODE_MAILBOX_KHR: Like the second mode, but instead of blocking the application when the queue is full, the images are replaced with the newer ones. This can be used to implement triple buffering, avoiding tearing with much less latency issues than standard vertical sync that uses double buffering.</li>
		</ul>
	This functions will choose VK_PRESENT_MODE_MAILBOX_KHR if available. Otherwise, it will choose VK_PRESENT_MODE_FIFO_KHR.
*/
VkPresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
	// Choose VK_PRESENT_MODE_MAILBOX_KHR if available
	for (const auto& mode : availablePresentModes)
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
			return mode;

	// Otherwise, choose VK_PRESENT_MODE_FIFO_KHR
	return VK_PRESENT_MODE_FIFO_KHR;
}


uint32_t Commander::getNextFrame()
{
	const std::lock_guard<std::mutex> lock(mutGetNextFrame);

	lastFrame = (lastFrame + 1) % maxFramesInFlight;   // By using the modulo operator (%), the frame index loops around after every MAX_FRAMES_IN_FLIGHT enqueued frames.
	return lastFrame;
}

size_t Commander::numFrames() {	return maxFramesInFlight; }

void Commander::freeCommandBuffers()
{
	for (uint32_t i = 0; i < commandBuffers.size(); i++)
	{
		const std::lock_guard<std::mutex> lock(mutCommandPool[i]);
		vkFreeCommandBuffers(c.device, commandPools[i], static_cast<uint32_t>(commandBuffers[i].size()), commandBuffers[i].data());
	}
}

void Commander::destroyCommandPool()
{
	for (VkCommandPool& commandPool : commandPools)
		vkDestroyCommandPool(c.device, commandPool, nullptr);
}

void Commander::destroySynchronizers()
{
	for (size_t i = 0; i < framesInFlight.size(); i++)   // Semaphores (render & image available) & fences (in flight)
	{
		vkDestroySemaphore(c.device, renderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(c.device, imageAvailableSemaphores[i], nullptr);
		vkDestroyFence(c.device, framesInFlight[i], nullptr);
	}
}

/**
* 	@brief Finds the right type of memory to use, depending upon the requirements of the buffer and our own application requiremnts.
*	
*	Graphic cards can offer different types of memory to allocate from. Each type of memory varies in terms of allowed operations and performance characteristics.
*	@param typeFilter Specifies the bit field of memory types that are suitable.
*	@param properties Specifies the bit field of the desired properties of such memory types.
*	@return Index of a memory type suitable for the buffer that also has all of the properties we need.
*/
uint32_t VulkanCore::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	// Query info about the available types of memory.
	VkPhysicalDeviceMemoryProperties memProperties;				// This struct has 2 arrays memoryTypes and memoryHeaps (this one are distinct memory resources, like dedicated VRAM and swap space in RAM for when VRAM runs out). Right now we'll concern with the type of memory and not the heap it comes from.
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	// Find a memory type suitable for the buffer, and to .
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if (typeFilter & (1 << i) &&													// Find the index of a suitable memory type for our buffer by iterating over the memory types and checking if the corresponding bit is set to 1.
			(memProperties.memoryTypes[i].propertyFlags & properties) == properties)	// Check that the memory type has some properties.
			return i;
	}

	throw std::runtime_error("Failed to find suitable memory type!");
}

void Commander::createCommandPool(size_t numFrames)
{
#if defined(DEBUG_RENDERER) || defined(DEBUG_COMMANDBUFFERS)
	std::cout << typeid(*this).name() << "::" << __func__ << " BEGIN" << std::endl;
#endif

	QueueFamilyIndices queueFamilyIndices = c.findQueueFamilies(c.physicalDevice);

	// Command buffers are executed by submitting them on one of the device queues we retrieved (graphics queue, presentation queue, etc.). Each command pool can only allocate command buffers that are submitted on a single type of queue.
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;	// [Optional]  VK_COMMAND_POOL_CREATE_ ... TRANSIENT_BIT (command buffers are rerecorded with new commands very often - may change memory allocation behavior), RESET_COMMAND_BUFFER_BIT (command buffers can be rerecorded individually, instead of reseting all of them together). Not necessary if we just record the command buffers at the beginning of the program and then execute them many times in the main loop.
	
	commandPools.resize(numFrames);

	for(VkCommandPool& commandPool : commandPools)
		if (vkCreateCommandPool(c.device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
			throw std::runtime_error("Failed to create command pool!");

#if defined(DEBUG_RENDERER) || defined(DEBUG_COMMANDBUFFERS)
	std::cout << typeid(*this).name() << "::" << __func__ << " END" << std::endl;
#endif
}

void Commander::createCommandBuffers(size_t numSwapChainImages, size_t numFrames)
{
	for (size_t i = 0; i < numFrames; i++)
	{
		const std::lock_guard<std::mutex> lock(mutCommandPool[i]);

		commandBuffers[i].resize(numSwapChainImages);

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPools[i];
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;   // VK_COMMAND_BUFFER_LEVEL_ ... PRIMARY (can be submitted to a queue for execution, but cannot be called from other command buffers), SECONDARY (cannot be submitted directly, but can be called from primary command buffers - useful for reusing common operations from primary command buffers).
		allocInfo.commandBufferCount = (uint32_t)commandBuffers[i].size();   // Number of buffers to allocate.

		if (vkAllocateCommandBuffers(c.device, &allocInfo, commandBuffers[i].data()) != VK_SUCCESS)
			throw std::runtime_error("Failed to allocate command buffers!");
	}
}

void Commander::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels)
{
#if defined(DEBUG_RENDERER) || defined(DEBUG_COMMANDBUFFERS)
	std::cout << typeid(*this).name() << "::" << __func__ << " BEGIN" << std::endl;
#endif

	uint32_t frameIndex = getNextFrame();
	const std::lock_guard<std::mutex> lock(mutFrame[frameIndex]);

	VkCommandBuffer commandBuffer = beginSingleTimeCommands(frameIndex);

	VkImageMemoryBarrier barrier{};			// One of the most common way to perform layout transitions is using an image memory barrier. A pipeline barrier like that is generally used to synchronize access to resources, like ensuring that a write to a buffer completes before reading from it, but it can also be used to transition image layouts and transfer queue family ownership when VK_SHARING_MODE_EXCLUSIVE is used. There is an equivalent buffer memory barrier to do this for buffers.
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;											// Specify layout transition (it's possible to use VK_IMAGE_LAYOUTR_UNDEFINED if you don't care about the existing contents of the image).
	barrier.newLayout = newLayout;											// Specify layout transition
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;					// If you are using the barrier to transfer queue family ownership, then these two fields ...
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;					// ... should be the indices of the queue families; otherwise, set this to VK_QUEUE_FAMILY_IGNORED.
	barrier.image = image;
	//barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;		// subresourceRange specifies the part of the image that is affected.
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mipLevels;						// If the image has no mipmapping levels, then levelCount = 1
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;								// If the image is not an array, then layerCount = 1

	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		if (hasStencilComponent(format))
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	else
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	// Set the access masks and pipeline stages based on the layouts in the transition. There are 2 transitions we need to handle:
	//		- From somewhere (undefined) to transfer destination: Transfer writes that don't need to wait on anything.
	//		- From transfer destinations to shader reading: Shader reads should wait on transfer writes (specifically the shader reads in the fragment shader, because that's where we're going to use the texture).
	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;					// The depth buffer is read from  in the VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT stage to perform depth tests 
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;		// The depth buffer is written to in the VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT  stage when a new fragment is drawn
	}
	else
		throw std::invalid_argument("Unsupported layout transition!");

	// Submit a pipeline barrier
	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage,		// Specify in which pipeline stage the operations occur that should happen before the barrier 
		destinationStage,	// Specify the pipeline stage in which operations will wait on the barrier
		0,					// This is either 0 (nothing) or VK_DEPENDENCY_BY_REGION_BIT (turns the barrier into a per-region condition, which means that the implementation is allowed to already begin reading from the parts of a resource that were written so far, for example).
		0, nullptr,			// Array of pipeline barriers of type memory barriers
		0, nullptr,			// Array of pipeline barriers of type buffer memory barriers
		1, &barrier);		// Array of pipeline barriers of type image memory barriers

	endSingleTimeCommands(frameIndex, commandBuffer);
	
	/*
		Note:
		The pipeline stages that you are allowed to specify before and after the barrier depend on how you use the resource before and after the barrier.
		Allowed values: https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#synchronization-access-types-supported
		Example: If you're going to read from a uniform after the barrier, you would specify a usage of VK_ACCESS_UNIFORM_READ_BIT and the earliest shader
		that will read from the uniform as pipeline stage (for example, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT).
		----------
		Transfer writes must occur in the pipeline transfer stage. Since the writes don't have to wait on anything, you may specify an empty access mask and the
		earliest possible pipeline stage VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT for the pre-barrier operations. Note that VK_PIPELINE_STAGE_TRANSFER_BIT is not a real
		stage within the graphics and compute pipelines, but a pseudo-stage where transfers happen (more about pseudo-stages:
		https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkPipelineStageFlagBits.html).

		The image will be written in the same pipeline stage and subsequently read by the fragment shader, which is why we specify shader reading access in the
		fragment shader pipeline stage.

		If we need to do more transitions in the future, then we'll extend the function.
		Note that command buffer submission results in implicit VK_ACCESS_HOST_WRITE_BIT synchronization at the beginning. Since this function (transitionImageLayout)
		executes a command buffer with only a single command, you could use this implicit synchronization and set srcAccessMask to 0 if you ever needed a
		VK_ACCESS_HOST_WRITE_BIT dependency in a layout transition. It's up to you if you want to be explicit about it or not, but I prefer not to rely on these
		OpenGL-like "hidden" operations.

		VK_IMAGE_LAYOUT_GENERAL: Special type of image layout that supports all operations, although it doesn't necessarily offer the best performance for any
		operation. It is required for some special cases (using an image as both input and output, reading an image after it has left the preinitialized layout, etc.).

		All of the helper functions that submit commands so far have been set up to execute synchronously by waiting for the queue to become idle. For practical
		applications it is recommended to combine these operations in a single command buffer and execute them asynchronously for higher throughput (especially the
		transitions and copy in the createTextureImage function). Exercise: Try to experiment with this by creating a setupCommandBuffer that the helper functions
		record commands into, and add a flushSetupCommands to execute the commands that have been recorded so far. It's best to do this after the texture mapping works
		to check if the texture resources are still set up correctly.
	*/

#if defined(DEBUG_RENDERER) || defined(DEBUG_COMMANDBUFFERS)
	std::cout << typeid(*this).name() << "::" << __func__ << " END" << std::endl;
#endif
}

/**
	@brief Copies some amount of data (size) from srcBuffer into dstBuffer. Used in createVertexBuffer() and createIndexBuffer().

	Memory transfer operations are executed using command buffers (like drawing commands), so we allocate a temporary command buffer. You may wish to create a separate command pool for these kinds of short-lived buffers, because the implementation could apply memory allocation optimizations. You should use the VK_COMMAND_POOL_CREATE_TRANSIENT_BIT flag during command pool generation in that case.
*/
void Commander::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
#if defined(DEBUG_RENDERER) || defined(DEBUG_COMMANDBUFFERS)
	std::cout << typeid(*this).name() << "::" << __func__ << " BEGIN" << std::endl;
#endif

	uint32_t frameIndex = getNextFrame();
	const std::lock_guard<std::mutex> lock(mutFrame[frameIndex]);

	VkCommandBuffer commandBuffer = beginSingleTimeCommands(frameIndex);

	// Specify buffers and the size of the contents you will transfer (it's not possible to specify VK_WHOLE_SIZE here, unlike vkMapMemory command).
	VkBufferCopy copyRegion{};
	copyRegion.size = size;
	copyRegion.srcOffset = 0;	// Optional
	copyRegion.dstOffset = 0;	// Optional

	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	endSingleTimeCommands(frameIndex, commandBuffer);
	
#if defined(DEBUG_RENDERER) || defined(DEBUG_COMMANDBUFFERS)
	std::cout << typeid(*this).name() << "::" << __func__ << " END" << std::endl;
#endif
}

void Commander::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
#if defined(DEBUG_RENDERER) || defined(DEBUG_COMMANDBUFFERS)
	std::cout << typeid(*this).name() << "::" << __func__ << " BEGIN" << std::endl;
#endif

	uint32_t frameIndex = getNextFrame();
	const std::lock_guard<std::mutex> lock(mutFrame[frameIndex]);

	VkCommandBuffer commandBuffer = beginSingleTimeCommands(frameIndex);

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

	endSingleTimeCommands(frameIndex, commandBuffer);
	
#if defined(DEBUG_RENDERER) || defined(DEBUG_COMMANDBUFFERS)
	std::cout << typeid(*this).name() << "::" << __func__ << " END" << std::endl;
#endif
}

void Commander::generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
#if defined(DEBUG_RENDERER) || defined(DEBUG_COMMANDBUFFERS)
	std::cout << typeid(*this).name() << "::" << __func__ << " BEGIN" << std::endl;
#endif

	// Check if the image format supports linear blitting. We are using vkCmdBlitImage, but it's not guaranteed to be supported on all platforms because it requires our texture image format to support linear filtering, so we check it with vkGetPhysicalDeviceFormatProperties.
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(c.physicalDevice, imageFormat, &formatProperties);
	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
	{
		throw std::runtime_error("Texture image format does not support linear blitting!");
		// Two alternatives:
		//		- Implement a function that searches common texture image formats for one that does support linear blitting.
		//		- Implement the mipmap generation in software with a library like stb_image_resize. Each mip level can then be loaded into the image in the same way that you loaded the original image.
		// It's uncommon to generate the mipmap levels at runtime anyway. Usually they are pregenerated and stored in the texture file alongside the base level to improve loading speed. <<<<<
	}

	uint32_t frameIndex = getNextFrame();
	const std::lock_guard<std::mutex> lock(mutFrame[frameIndex]);

	VkCommandBuffer commandBuffer = beginSingleTimeCommands(frameIndex);

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

	endSingleTimeCommands(frameIndex, commandBuffer);
	
#if defined(DEBUG_RENDERER) || defined(DEBUG_COMMANDBUFFERS)
	std::cout << typeid(*this).name() << "::" << __func__ << " END" << std::endl;
#endif
}

/// Tells if the chosen depth format contains a stencil component.
bool Commander::hasStencilComponent(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

/**
*	Allocate the command buffer and start recording it. Used for a command buffer that will be used only once.
*	Command buffers generated using beginSingleTimeCommands (allocation, start recording) and endSingleTimeCommands (end recording, submit) are freed once its execution completes.
*	@return Returns a Vulkan command buffer object.
*/
VkCommandBuffer Commander::beginSingleTimeCommands(uint32_t frameIndex)
{
	// <<< Current fences make this functions wait until object is submitted
	// <<< Do we have to coordinate this with main fences?
	// It used the commandPool. Now it needs one of the command pools, so a frameIndex should be generated, and thus, the corresponding fence should be used.
	// Why a fence from renderer is not used here now? 
	// When are the functions containing these methods used? Under what circumstances?

	// Allocate the command buffer.
	const std::lock_guard<std::mutex> lock2(mutCommandPool[frameIndex]);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPools[frameIndex];
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(c.device, &allocInfo, &commandBuffer);

	// Start recording the command buffer.
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;		// Good practice to tell the driver about our intent <<< (see createCommandBuffers > beginInfo.flags)

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

/**
*	Stop recording a command buffer and submit it to the queue. Used together with beginSingleTimeCommands().
*/
void Commander::endSingleTimeCommands(uint32_t frameIndex, VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);		// Stop recording (this command buffer only contains the copy command, so we can stop recording now).
	
	// Execute the command buffer (only contains the copy command) to complete the transfer of buffers.
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[frameIndex] };
	VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[frameIndex] };

	VkSubmitInfo submitInfo{};   // No semaphores needed because an image isn't acquired nor rendered to.
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkWaitForFences(c.device, 1, &framesInFlight[frameIndex], VK_TRUE, UINT64_MAX);	// Wait for signaled state
	vkResetFences(c.device, 1, &framesInFlight[frameIndex]);						// Reset to unsignaled state (CB didn't finish execution).

	{
		const std::lock_guard<std::mutex> lock(mutQueue);
		vkQueueSubmit(c.graphicsQueue, 1, &submitInfo, framesInFlight[frameIndex]);	// VK_NULL_HANDLE);
		//vkQueueWaitIdle(c.graphicsQueue);									// Wait to this transfer to complete. Two ways to do this: vkQueueWaitIdle (Wait for the transfer queue to become idle. Execute one transfer at a time) or vkWaitForFences (Use a fence. Allows to schedule multiple transfers simultaneously and wait for all of them complete. It may give the driver more opportunities to optimize).
	}
	
	vkWaitForFences(c.device, 1, &framesInFlight[frameIndex], VK_TRUE, UINT64_MAX);	// Wait for signaled state

	// Clean up the command buffer used.
	//vkFreeCommandBuffers(c.device, commandPools[frameIndex], 1, &commandBuffer);
}

/**
 * Cleans up the VkDebugUtilsMessengerEXT object.
 * @param instance Vulkan instance (the debug messenger is specific to our Vulkan instance and its layers)
 * @param debugMessenger Debug messenger object
 * @param pAllocator Optional allocator callback
 */
void ValLayers::DestroyDebugUtilsMessengerEXT()
{
	if (instance == nullptr) return;

	// Similarly to vkCreateDebugUtilsMessengerEXT, the extension function needs to be explicitly loaded.
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
		func(instance, debugMessenger, nullptr);
}

void VulkanCore::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	// Create buffer.
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;									// For multiple purposes use a bitwise or.
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;			// Like images in the swap chain, buffers can also be owned by a specific queue family or be shared between multiple at the same time. Since the buffer will only be used from the graphics queue, we use EXCLUSIVE.
	bufferInfo.flags = 0;										// Used to configure sparse buffer memory.

	if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)	// vkCreateBuffer creates a new buffer object and returns it to a pointer to a VkBuffer provided by the caller.
		throw std::runtime_error("Failed to create buffer!");

	// Get buffer requirements.
	VkMemoryRequirements memRequirements;		// Members: size (amount of memory in bytes. May differ from bufferInfo.size), alignment (offset in bytes where the buffer begins in the allocated region. Depends on bufferInfo.usage and bufferInfo.flags), memoryTypeBits (bit field of the memory types that are suitable for the buffer).
	vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

	// Allocate memory for the buffer.
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);		// Properties parameter: We need to be able to write our vertex data to that memory. The properties define special features of the memory, like being able to map it so we can write to it from the CPU.

	if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate buffer memory!");

	memAllocObjects++;

	vkBindBufferMemory(device, buffer, bufferMemory, 0);	// Associate this memory with the buffer. If the offset (4th parameter) is non-zero, it's required to be divisible by memRequirements.alignment.
}

void VulkanCore::destroyBuffer(VkDevice device, VkBuffer buffer, VkDeviceMemory memory)
{
	vkDestroyBuffer(device, buffer, nullptr);
	vkFreeMemory(device, memory, nullptr);
	memAllocObjects--;
}

RenderPipeline::RenderPipeline(VulkanCore& core, SwapChain& swapChain, Commander& commander)
	: c(core), swapChain(swapChain), commander(commander) { }

Subpass& RenderPipeline::getSubpass(unsigned renderPassIndex, unsigned subpassIndex)
{
	return renderPasses[renderPassIndex].subpasses[subpassIndex];
}

void RenderPipeline::createRenderPipeline()
{
	createRenderPass();				// Recreate render pass because it depends on the format of the swap chain images.
	createImageResources();

	for (RenderPass& renderPass : renderPasses)
	{
		renderPass.createFramebuffers(c, swapChain);
		renderPass.createRenderPassInfo(swapChain);
	}
}

void RenderPipeline::destroyRenderPipeline()
{
	destroyAttachments();

	for (RenderPass& renderPass : renderPasses)
		renderPass.destroy(c);
}

void RenderPass::createRenderPass(VkDevice& device, std::vector<VkAttachmentDescription>& allAttachments, std::vector<VkAttachmentReference>& inputAttachments, std::vector<VkAttachmentReference>& colorAttachments, VkAttachmentReference* depthAttachment)
{
	VkSubpassDescription subpassDescription{};
	subpassDescription.flags;
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;		// VK_PIPELINE_BIND_POINT_GRAPHICS: This is a graphics subpass
	subpassDescription.inputAttachmentCount = static_cast<uint32_t>(inputAttachments.size());
	subpassDescription.pInputAttachments = inputAttachments.data();				// Attachments read from a shader.
	subpassDescription.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
	subpassDescription.pColorAttachments = colorAttachments.data();				// Attachment for color. The index of the attachment in this array is directly referenced from the fragment shader with the directive "layout(location = 0) out vec4 outColor".
	subpassDescription.pResolveAttachments = nullptr;							// Attachments used for resolving multisampling color attachments.
	subpassDescription.pDepthStencilAttachment = depthAttachment;				// Attachment for depth and stencil data. A subpass can only use a single depth (+ stencil) attachment.
	subpassDescription.preserveAttachmentCount;
	subpassDescription.pPreserveAttachments;									// Attachment not used by this subpass, but for which the data must be preserved.

	VkSubpassDependency subpassDependency{};
	subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;																				// VK_SUBPASS_EXTERNAL: Refers to the implicit subpass before or after the render pass depending on whether it is specified in srcSubpass or dstSubpass.
	subpassDependency.dstSubpass = 0;																								// Index of our subpass. The dstSubpass must always be higher than srcSubpass to prevent cycles in the dependency graph (unless one of the subpasses is VK_SUBPASS_EXTERNAL).
	subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;	// Stage where to wait (for the swap chain to finish reading from the image).
	subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;	// Stage where to wait. 
	subpassDependency.srcAccessMask = 0;																							// Operations that wait.
	subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;			// Operations that wait (they involve the writing of the color attachment).

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(allAttachments.size());
	renderPassInfo.pAttachments = allAttachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;		// Array of subpasses
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &subpassDependency;		// Array of dependencies.

	if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
		throw std::runtime_error("Failed to create render pass!");
}

void RenderPass::createFramebuffers(VulkanCore& c, SwapChain& swapChain)
{
	size_t swapchainImageCount = attachments.size();

	// Get attachments (from pointers) for each swap chain image.
	std::vector<std::vector<VkImageView>> attachments_2(swapchainImageCount);
	for (size_t i = 0; i < attachments_2.size(); i++)
	{
		attachments_2[i].resize(attachments[i].size());
		for (size_t j = 0; j < attachments_2[i].size(); j++)
			attachments_2[i][j] = *attachments[i][j];		// pointer derreferenced
	}
	
	// Create framebuffers
	framebuffers.resize(swapchainImageCount);
	for (unsigned i = 0; i < framebuffers.size(); i++)		// per swap chain image
	{
		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;							// A framebuffer can only be used with the render passes that it is compatible with, which roughly means that they use the same number and type of attachments.
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments_2[i].size());
		framebufferInfo.pAttachments = attachments_2[i].data();				// Objects that should be bound to the respective attachment descriptions in the render pass pAttachment array.
		framebufferInfo.width = swapChain.extent.width;
		framebufferInfo.height = swapChain.extent.height;
		framebufferInfo.layers = 1;											// Number of layers in image arrays. If your swap chain images are single images, then layers = 1.

		if (vkCreateFramebuffer(c.device, &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS)
			throw std::runtime_error("Failed to create framebuffer!");
	}
}

void RenderPass::createRenderPassInfo(SwapChain& swapChain)
{
	renderPassInfos.resize(swapChain.numImages());

	for (unsigned i = 0; i < renderPassInfos.size(); i++)
	{
		renderPassInfos[i].sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfos[i].renderPass = renderPass;
		renderPassInfos[i].framebuffer = framebuffers[i];
		renderPassInfos[i].renderArea.offset = { 0, 0 };
		renderPassInfos[i].renderArea.extent = swapChain.extent;						// Size of the render area (where shader loads and stores will take place). Pixels outside this region will have undefined values. It should match the size of the attachments for best performance.
		renderPassInfos[i].clearValueCount = static_cast<uint32_t>(clearValues.size());	// Clear values to use for VK_ATTACHMENT_LOAD_OP_CLEAR, which we ...
		renderPassInfos[i].pClearValues = clearValues.data();							// ... used as load operation for the color attachment and depth buffer.
	}
}

void RenderPass::destroy(VulkanCore& c)
{
	vkDestroyRenderPass(c.device, renderPass, nullptr);

	for (VkFramebuffer& framebuffer : framebuffers)
		vkDestroyFramebuffer(c.device, framebuffer, nullptr);
}

RP_DS::RP_DS(VulkanCore& core, SwapChain& swapChain, Commander& commander) 
	: RenderPipeline(core, swapChain, commander), position(c), albedo(c), normal(c), specRoug(c), depth(c)
{
	// Render passes -------------------------

	renderPasses = {
		RenderPass({Subpass({ }, 4)}),
		RenderPass({Subpass({&position, &albedo, &normal, &specRoug}, 1)})
	};

	// Attachments -------------------------

	std::vector<std::vector<std::vector<VkImageView*>>> allAttachments(swapChain.numImages());	// Attachments per swapchain image, per render pass.
	for (unsigned i = 0; i < allAttachments.size(); i++)
		allAttachments[i] = {
			std::vector<VkImageView*>{ &position.view, &albedo.view, &normal.view, &specRoug.view, &depth.view },			// RP1. Color attachment differs for every swap chain image, but the same depth image can be used by all of them because only a single subpass is running at the same time due to our semaphores.
			std::vector<VkImageView*>{ &position.view, &albedo.view, &normal.view, &specRoug.view, &swapChain.views[i] }	// RP2.
	};

	for (unsigned i = 0; i < renderPasses.size(); i++)
		for (unsigned j = 0; j < swapChain.numImages(); j++)
			renderPasses[i].attachments.push_back(allAttachments[j][i]);

	// clearValues -------------------------

	VkClearColorValue background = { 0.20f, 0.59f, 1.00f, 1.00f };
	VkClearColorValue zeros = { 0.0f, 0.0f, 0.0f, 0.0f };
	size_t i = 0;

	renderPasses[i].clearValues.resize(5);						// One per attachment (MSAA color buffer, resolve color buffer, depth buffer...). The order of clearValues should be identical to the order of your attachments.
	renderPasses[i].clearValues[0].color = zeros;				// Color buffer (position). Background color (alpha = 1 means 100% opacity)
	renderPasses[i].clearValues[1].color = background;			// Color buffer (albedo)
	renderPasses[i].clearValues[2].color = zeros;				// Color buffer (normal)
	renderPasses[i].clearValues[3].color = zeros;				// Color buffer (specularity & roughness)
	renderPasses[i].clearValues[4].depthStencil = { 1.0f, 0 };	// Depth buffer. Depth buffer range in Vulkan is [0.0, 1.0], where 1.0 lies at the far view plane and 0.0 at the near view plane. The initial value at each point in the depth buffer should be the furthest possible depth (1.0).

	i++;
	renderPasses[i].clearValues.resize(5);
	renderPasses[i].clearValues[0].color = zeros;				// Input attachment (position)
	renderPasses[i].clearValues[1].color = background;			// Input attachment (albedo)
	renderPasses[i].clearValues[2].color = zeros;				// Input attachment (normal)
	renderPasses[i].clearValues[3].color = zeros;				// Input attachment (specularity & roughness)
	renderPasses[i].clearValues[4].color = background;			// Final color buffer

	createRenderPipeline();
}

void RP_DS::createRenderPass()
{
	#ifdef DEBUG_ENV_CORE
		std::cout << "   " << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	/*
		RP1's color attachments (writing): position, albedo, normal, specRoug
		RP2's input attachments (reading): position, albedo, normal, specRoug
	*/

	// Attachments -------------------------

	VkAttachmentDescription positionAtt_1{};	// RP1::SP1
	VkAttachmentReference positionAttRef_1{};

	VkAttachmentDescription albedoAtt_1{};
	VkAttachmentReference albedoAttRef_1{};

	VkAttachmentDescription normalAtt_1{};
	VkAttachmentReference normalAttRef_1{};

	VkAttachmentDescription specRougAtt_1{};
	VkAttachmentReference specRougAttRef_1{};

	VkAttachmentDescription depthAtt11{};
	VkAttachmentReference depthAttRef11{};

	VkAttachmentDescription positionAtt_2{};	// RP2::SP1
	VkAttachmentReference positionAttRef_2{};

	VkAttachmentDescription albedoAtt_2{};
	VkAttachmentReference albedoAttRef_2{};

	VkAttachmentDescription normalAtt_2{};
	VkAttachmentReference normalAttRef_2{};

	VkAttachmentDescription specRougAtt_2{};
	VkAttachmentReference specRougAttRef_2{};

	VkAttachmentDescription finalColorAtt_2{};
	VkAttachmentReference finalColorAttRef_2{};
	
	// Input attachment (position) to RP1::SP1
	positionAtt_1.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	positionAtt_1.samples = VK_SAMPLE_COUNT_1_BIT;							// Single color buffer attachment, or many (multisampling).
	positionAtt_1.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;					// What to do with the data (color and depth) in the attachment before rendering: VK_ATTACHMENT_LOAD_OP_ ... LOAD (preserve existing contents of the attachment), CLEAR (clear values to a constant at the start of a new frame), DONT_CARE (existing contents are undefined).
	positionAtt_1.storeOp = VK_ATTACHMENT_STORE_OP_STORE;					// What to do with the data (color and depth) in the attachment after rendering:  VK_ATTACHMENT_STORE_OP_ ... STORE (rendered contents will be stored in memory and can be read later), DON_CARE (contents of the framebuffer will be undefined after rendering).
	positionAtt_1.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;			// What to do with the stencil data in the attachment before rendering.
	positionAtt_1.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;		// What to do with the stencil data in the attachment after rendering.
	positionAtt_1.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;				// Layout before the render pass. Textures and framebuffers in Vulkan are represented by VkImage objects with a certain pixel format, however the layout of the pixels in memory need to be transitioned to specific layouts suitable for the operation that they're going to be involved in next (read more below).
	positionAtt_1.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// Layout to automatically transition after the render pass finishes. VK_IMAGE_LAYOUT_ ... UNDEFINED (we don't care what previous layout the image was in, and the contents of the image are not guaranteed to be preserved), COLOR_ATTACHMENT_OPTIMAL (images used as color attachment), PRESENT_SRC_KHR (images to be presented in the swap chain), TRANSFER_DST_OPTIMAL (Images to be used as destination for a memory copy operation).

	positionAttRef_1.attachment = 0;										// Specify which attachment to reference by its index in the attachment descriptions array.
	positionAttRef_1.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;		// Specify the layout we would like the attachment to have during a subpass that uses this reference. The layout VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL will give us the best performance.

	// Input attachment (albedo) to RP1::SP1
	albedoAtt_1.format = swapChain.imageFormat;
	albedoAtt_1.samples = VK_SAMPLE_COUNT_1_BIT;
	albedoAtt_1.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	albedoAtt_1.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	albedoAtt_1.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	albedoAtt_1.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	albedoAtt_1.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	albedoAtt_1.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	albedoAttRef_1.attachment = 1;
	albedoAttRef_1.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Input attachment (normal) to RP1::SP1
	normalAtt_1.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	normalAtt_1.samples = VK_SAMPLE_COUNT_1_BIT;
	normalAtt_1.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	normalAtt_1.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	normalAtt_1.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	normalAtt_1.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	normalAtt_1.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	normalAtt_1.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	normalAttRef_1.attachment = 2;
	normalAttRef_1.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Input attachment (specularity & roughness) to RP1::SP1
	specRougAtt_1.format = swapChain.imageFormat;
	specRougAtt_1.samples = VK_SAMPLE_COUNT_1_BIT;
	specRougAtt_1.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	specRougAtt_1.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	specRougAtt_1.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	specRougAtt_1.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	specRougAtt_1.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	specRougAtt_1.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	specRougAttRef_1.attachment = 3;
	specRougAttRef_1.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Depth buffer of RP1::SP1
	depthAtt11.format = c.deviceData.depthFormat;						// Should be same format as the depth image
	depthAtt11.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAtt11.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAtt11.storeOp = VK_ATTACHMENT_STORE_OP_STORE;					// VK_ATTACHMENT_STORE_OP_DONT_CARE: Here, we don't care because it will not be used after drawing has finished
	depthAtt11.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAtt11.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAtt11.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;					// We don't care about previous depth contents
	depthAtt11.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	depthAttRef11.attachment = 4;
	depthAttRef11.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// Input attachment (position) to RP2::SP1
	positionAtt_2 = positionAtt_1;
	positionAtt_2.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	positionAtt_2.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	positionAttRef_2 = positionAttRef_1;
	positionAttRef_2.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	positionAttRef_2.attachment = 0;

	// Input attachment (albedo) to RP2::SP1
	albedoAtt_2 = albedoAtt_1;
	albedoAtt_2.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	albedoAtt_2.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	albedoAttRef_2 = albedoAttRef_1;
	albedoAttRef_2.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	albedoAttRef_2.attachment = 1;

	// Input attachment (normal) to RP2::SP1
	normalAtt_2 = normalAtt_1;
	normalAtt_2.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	normalAtt_2.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	normalAttRef_2 = normalAttRef_1;
	normalAttRef_2.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	normalAttRef_2.attachment = 2;

	// Input attachment (specularity & roughness) to RP2::SP1
	specRougAtt_2 = specRougAtt_1;
	specRougAtt_2.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	specRougAtt_2.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	specRougAttRef_2 = specRougAttRef_1;
	specRougAttRef_2.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	specRougAttRef_2.attachment = 3;

	// Final color (output to screen) of RP2::SP1
	finalColorAtt_2.format = swapChain.imageFormat;
	finalColorAtt_2.samples = VK_SAMPLE_COUNT_1_BIT;
	finalColorAtt_2.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	finalColorAtt_2.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	finalColorAtt_2.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	finalColorAtt_2.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	finalColorAtt_2.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	finalColorAtt_2.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	
	finalColorAttRef_2.attachment = 4;
	finalColorAttRef_2.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Render pass 1 (Geometry pass) -------------------------

	VkSubpassDescription subpass11{};
	subpass11.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;	// VK_PIPELINE_BIND_POINT_GRAPHICS: This is a graphics subpass
	std::vector<VkAttachmentReference> outputAttachments11 = { positionAttRef_1, albedoAttRef_1, normalAttRef_1, specRougAttRef_1 };	// Put together all the attachments that your render-pass will contain, in the same order you specified when creating the references (VkAttachmentReference).
	subpass11.colorAttachmentCount = static_cast<uint32_t>(outputAttachments11.size());
	subpass11.pColorAttachments = outputAttachments11.data();		// Attachment for color. The index of the attachment in this array is directly referenced from the fragment shader with the directive "layout(location = 0) out vec4 outColor".
	subpass11.pResolveAttachments = nullptr;						// Attachments used for resolving multisampling color attachments.
	subpass11.pDepthStencilAttachment = &depthAttRef11;				// Attachment for depth and stencil data. A subpass can only use a single depth (+ stencil) attachment.
	subpass11.inputAttachmentCount = 0;
	subpass11.pInputAttachments = nullptr;							// Attachments read from a shader.
	subpass11.preserveAttachmentCount;
	subpass11.pPreserveAttachments;									// Attachment not used by this subpass, but for which the data must be preserved.

	VkSubpassDependency dependency11{};
	dependency11.srcSubpass = VK_SUBPASS_EXTERNAL;																			// VK_SUBPASS_EXTERNAL: Refers to the implicit subpass before or after the render pass depending on whether it is specified in srcSubpass or dstSubpass.
	dependency11.dstSubpass = 0;																								// Index of our subpass. The dstSubpass must always be higher than srcSubpass to prevent cycles in the dependency graph (unless one of the subpasses is VK_SUBPASS_EXTERNAL).
	dependency11.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;	// Stage where to wait (for the swap chain to finish reading from the image).
	dependency11.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;	// Stage where to wait. 
	dependency11.srcAccessMask = 0;																							// Operations that wait.
	dependency11.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;			// Operations that wait (they involve the writing of the color attachment).

	VkRenderPassCreateInfo renderPassInfo1{};
	renderPassInfo1.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	std::vector<VkAttachmentDescription> attachments1 = { positionAtt_1, albedoAtt_1, normalAtt_1, specRougAtt_1, depthAtt11 };	// Put together all the attachments that your render-pass will contain, in the same order you specified when creating the references (VkAttachmentReference).
	renderPassInfo1.attachmentCount = static_cast<uint32_t>(attachments1.size());
	renderPassInfo1.pAttachments = attachments1.data();
	renderPassInfo1.subpassCount = 1;
	renderPassInfo1.pSubpasses = &subpass11;				// Array of subpasses
	renderPassInfo1.dependencyCount = 1;
	renderPassInfo1.pDependencies = &dependency11;			// Array of dependencies.

	if (vkCreateRenderPass(c.device, &renderPassInfo1, nullptr, &renderPasses[0].renderPass) != VK_SUCCESS)
		throw std::runtime_error("Failed to create render pass!");

	// Render pass 2 (Lightning pass) -------------------------

	VkSubpassDescription subpass21{};
	subpass21.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass21.colorAttachmentCount = 1;
	subpass21.pColorAttachments = &finalColorAttRef_2;
	subpass21.pResolveAttachments = nullptr;
	subpass21.pDepthStencilAttachment = nullptr;
	std::vector<VkAttachmentReference> inputAttachments21 = { positionAttRef_2, albedoAttRef_2, normalAttRef_2, specRougAttRef_2 };
	subpass21.inputAttachmentCount = static_cast<uint32_t>(inputAttachments21.size());
	subpass21.pInputAttachments = inputAttachments21.data();	// <<< Can't input attachments read per-sample? Only per-pixel?
	subpass21.preserveAttachmentCount;
	subpass21.pPreserveAttachments;

	VkSubpassDependency dependency21{};
	dependency21.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency21.dstSubpass = 0;
	dependency21.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency21.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency21.srcAccessMask = 0;
	dependency21.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassInfo2{};
	renderPassInfo2.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	std::vector<VkAttachmentDescription> attachments2 = { positionAtt_2, albedoAtt_2, normalAtt_2, specRougAtt_2, finalColorAtt_2 };
	renderPassInfo2.attachmentCount = static_cast<uint32_t>(attachments2.size());
	renderPassInfo2.pAttachments = attachments2.data();
	renderPassInfo2.subpassCount = 1;
	renderPassInfo2.pSubpasses = &subpass21;				// Array of subpasses
	renderPassInfo2.dependencyCount = 1;
	renderPassInfo2.pDependencies = &dependency21;			// Array of dependencies.

	if (vkCreateRenderPass(c.device, &renderPassInfo2, nullptr, &renderPasses[1].renderPass) != VK_SUCCESS)
		throw std::runtime_error("Failed to create render pass!");
}

void RP_DS::createImageResources()
{
	#ifdef DEBUG_ENV_CORE
		std::cout << "   " << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_NEAREST;
	samplerInfo.minFilter = VK_FILTER_NEAREST;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 1.0f;

	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerInfo.minLod = 0.f;
	samplerInfo.maxLod = 0.f;
	samplerInfo.mipLodBias = 0.0f;

	// Position -------------------------------------
	/*
	c.createImage(
		position.image, position.memory,
		swapChain.extent.width, swapChain.extent.height,
		1, VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	c.createImageView(position.view, position.image, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	c.createSampler(position.sampler, samplerInfo);
	*/
	position.createFullImage(
		swapChain.extent.width,
		swapChain.extent.height,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT );

	position.createSampler(samplerInfo);

	// Albedo -------------------------------------

	albedo.createFullImage(
		swapChain.extent.width,
		swapChain.extent.height,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		swapChain.imageFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT );

	albedo.createSampler(samplerInfo);

	// Normal -------------------------------------

	normal.createFullImage(
		swapChain.extent.width,
		swapChain.extent.height,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT );

	normal.createSampler(samplerInfo);

	// specRoug -------------------------------------

	specRoug.createFullImage(
		swapChain.extent.width,
		swapChain.extent.height,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		swapChain.imageFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT );

	specRoug.createSampler(samplerInfo);

	// Depth -------------------------------------

	depth.createFullImage(
		swapChain.extent.width,
		swapChain.extent.height,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		c.deviceData.depthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_ASPECT_DEPTH_BIT );

	// Explicitly transition the layout of the image to a depth attachment (there is no need of doing this because we take care of this in the render pass, but this is here for completeness).
	commander.transitionImageLayout(depth.image, c.deviceData.depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);

	depth.createSampler(samplerInfo);
}

void RP_DS::destroyAttachments()
{
	position.destroy();
	albedo.destroy();
	normal.destroy();
	specRoug.destroy();
	depth.destroy();
}

RP_DS_PP::RP_DS_PP(VulkanCore& core, SwapChain& swapChain, Commander& commander)
	: RenderPipeline(core, swapChain, commander), position(c), albedo(c), normal(c), specRoug(c), depth(c), color(c)
{
	renderPasses = {
		RenderPass({ Subpass({ }, 4) }),
		RenderPass({ Subpass({&position, &albedo, &normal, &specRoug}, 1) }),
		RenderPass({ Subpass({ }, 1) }),
		RenderPass({ Subpass({&color, &depth}, 1) })
	};

	// Attachments (order convention: input attachments, depth attachment, color attachments)
	std::vector<std::vector<std::vector<VkImageView*>>> allAttachments(swapChain.numImages());	// Attachments per swapchain image, per render pass.
	for (unsigned i = 0; i < allAttachments.size(); i++)
		allAttachments[i] = {
			std::vector<VkImageView*>{ &depth.view, &position.view, &albedo.view, &normal.view, &specRoug.view },	// RP1. Color attachment differs for every swap chain image, but the same depth image can be used by all of them because only a single subpass is running at the same time due to our semaphores.
			std::vector<VkImageView*>{ &position.view, &albedo.view, &normal.view, &specRoug.view, &color.view },	// RP2.
			std::vector<VkImageView*>{ &depth.view, &color.view },													// RP3.
			std::vector<VkImageView*>{ &color.view, &depth.view, &swapChain.views[i] }								// RP4.
		};
	
	for (unsigned i = 0; i < renderPasses.size(); i++)
		for (unsigned j = 0; j < swapChain.numImages(); j++)
			renderPasses[i].attachments.push_back(allAttachments[j][i]);

	// clearValues -------------------------

	VkClearColorValue background = { 0.20f, 0.59f, 1.00f, 1.00f };
	VkClearColorValue zeros = { 0.0f, 0.0f, 0.0f, 0.0f };
	size_t i = 0;

	renderPasses[i].clearValues.resize(5);				// One per attachment (MSAA color buffer, resolve color buffer, depth buffer...). The order of clearValues should be identical to the order of your attachments.
	renderPasses[i].clearValues[0].depthStencil = { 1.0f, 0 };	// Depth buffer. Depth buffer range in Vulkan is [0.0, 1.0], where 1.0 lies at the far view plane and 0.0 at the near view plane. The initial value at each point in the depth buffer should be the furthest possible depth (1.0).
	renderPasses[i].clearValues[1].color = zeros;				// Color buffer (position). Background color (alpha = 1 means 100% opacity)
	renderPasses[i].clearValues[2].color = background;			// Color buffer (albedo)
	renderPasses[i].clearValues[3].color = zeros;				// Color buffer (normal)
	renderPasses[i].clearValues[4].color = zeros;				// Color buffer (specularity & roughness)

	i++;
	renderPasses[i].clearValues.resize(5);
	renderPasses[i].clearValues[0].color = zeros;				// Input attachment (position)
	renderPasses[i].clearValues[1].color = background;			// Input attachment (albedo)
	renderPasses[i].clearValues[2].color = zeros;				// Input attachment (normal)
	renderPasses[i].clearValues[3].color = zeros;				// Input attachment (specularity & roughness)
	renderPasses[i].clearValues[4].color = background;			// Color attachment (color)

	i++;
	renderPasses[i].clearValues.resize(2);
	renderPasses[i].clearValues[0].depthStencil = { 1.0f, 0 };	// Depth buffer.
	renderPasses[i].clearValues[1].color = background;			// Color attachment (color)

	i++;
	renderPasses[i].clearValues.resize(3);
	renderPasses[i].clearValues[0].color = zeros;				// Input attachment (color)
	renderPasses[i].clearValues[1].depthStencil = { 1.0f, 0 };	// Input attachment (depth buffer)
	renderPasses[i].clearValues[2].color = zeros;				// Color attachment (color)

	createRenderPipeline();
}

void RP_DS_PP::createRenderPass()
{
	#ifdef DEBUG_ENV_CORE
		std::cout << "   " << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	// Attachments -------------------------

	VkAttachmentDescription defaultAtt{};
	defaultAtt.format = swapChain.imageFormat;							// Should be same format as the depth image
	defaultAtt.samples = VK_SAMPLE_COUNT_1_BIT;								// Single color buffer attachment, or many (multisampling).
	defaultAtt.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;					// What to do with the data (color and depth) in the attachment before rendering: VK_ATTACHMENT_LOAD_OP_ ... LOAD (preserve existing contents of the attachment), CLEAR (clear values to a constant at the start of a new frame), DONT_CARE (existing contents are undefined).
	defaultAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;						// What to do with the data (color and depth) in the attachment after rendering:  VK_ATTACHMENT_STORE_OP_ ... STORE (rendered contents will be stored in memory and can be read later), DON_CARE (contents of the framebuffer will be undefined after rendering).
	defaultAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;				// What to do with the stencil data in the attachment before rendering.
	defaultAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;			// What to do with the stencil data in the attachment after rendering.
	defaultAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;					// Layout before the render pass. Textures and framebuffers in Vulkan are represented by VkImage objects with a certain pixel format, however the layout of the pixels in memory need to be transitioned to specific layouts suitable for the operation that they're going to be involved in next (read more below). Note: we don't care about previous depth contents.
	defaultAtt.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;		// Layout to automatically transition after the render pass finishes. VK_IMAGE_LAYOUT_ ... UNDEFINED (we don't care what previous layout the image was in, and the contents of the image are not guaranteed to be preserved), COLOR_ATTACHMENT_OPTIMAL (images used as color attachment), PRESENT_SRC_KHR (images to be presented in the swap chain), TRANSFER_DST_OPTIMAL (Images to be used as destination for a memory copy operation).

	// RP1::SP1::depth/stencilAttachment (depth)
	VkAttachmentDescription depthAtt11 = defaultAtt;
	depthAtt11.format = c.deviceData.depthFormat;							// Should be same format as the depth image
	depthAtt11.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;						// VK_ATTACHMENT_STORE_OP_DONT_CARE: Here, we don't care because it will not be used after drawing has finished.
	depthAtt11.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttRef11{};
	depthAttRef11.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttRef11.attachment = 0;

	// RP1::SP1::colorAttachment (position)
	VkAttachmentDescription caPosAtt11 = defaultAtt;
	caPosAtt11.format = VK_FORMAT_R32G32B32A32_SFLOAT;				
	caPosAtt11.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	caPosAtt11.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference caPosAttRef11{};
	caPosAttRef11.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;		// Specify the layout we would like the attachment to have during a subpass that uses this reference. The layout VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL will give us the best performance.
	caPosAttRef11.attachment = 1;											// Specify which attachment to reference by its index in the attachment descriptions array.

	// RP1::SP1::colorAttachment (albedo)
	VkAttachmentDescription caAlbedoAtt11 = defaultAtt;
	caAlbedoAtt11.format = swapChain.imageFormat;
	caAlbedoAtt11.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	caAlbedoAtt11.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference caAlbedoAttRef11{};
	caAlbedoAttRef11.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	caAlbedoAttRef11.attachment = 2;

	// RP1::SP1::colorAttachment (normal)
	VkAttachmentDescription caNormalAtt11 = defaultAtt;
	caNormalAtt11.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	caNormalAtt11.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	caNormalAtt11.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference caNormalAttRef11{};
	caNormalAttRef11.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	caNormalAttRef11.attachment = 3;

	// RP1::SP1::colorAttachment (specularity & roughness)
	VkAttachmentDescription caSpecRougAtt11 = defaultAtt;
	caSpecRougAtt11.format = swapChain.imageFormat;
	caSpecRougAtt11.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	caSpecRougAtt11.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference caSpecRougAttRef11{};
	caSpecRougAttRef11.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	caSpecRougAttRef11.attachment = 4;

	// RP1::SP1::attachments
	std::vector<VkAttachmentDescription> allAttachments11 = { depthAtt11, caPosAtt11, caAlbedoAtt11, caNormalAtt11, caSpecRougAtt11 };		// Put together all the attachments that your render-pass will contain, in the same order you specified when creating the references (VkAttachmentReference).
	std::vector<VkAttachmentReference> inputAttachments11 = { };
	VkAttachmentReference* depthAttachment11 = &depthAttRef11;
	std::vector<VkAttachmentReference> colorAttachments11 = { caPosAttRef11, caAlbedoAttRef11, caNormalAttRef11, caSpecRougAttRef11 };	// Put together all the attachments that your render-pass will contain, in the same order you specified when creating the references (VkAttachmentReference).


	// RP2::SP1::inputAttachment (position)
	VkAttachmentDescription iaPosAtt21 = defaultAtt;
	iaPosAtt21.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	iaPosAtt21.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	iaPosAtt21.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference iaPosAttRef21{};
	iaPosAttRef21.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	iaPosAttRef21.attachment = 0;

	// RP2::SP1::inputAttachment (albedo)
	VkAttachmentDescription iaAlbedoAtt21 = defaultAtt;
	iaAlbedoAtt21.format = swapChain.imageFormat;
	iaAlbedoAtt21.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	iaAlbedoAtt21.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference iaAlbedoAttRef21{};
	iaAlbedoAttRef21.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	iaAlbedoAttRef21.attachment = 1;

	// RP2::SP1::inputAttachment (normal)
	VkAttachmentDescription iaNormalAtt21 = defaultAtt;
	iaNormalAtt21.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	iaNormalAtt21.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	iaNormalAtt21.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference iaNormalAttRef21{};
	iaNormalAttRef21.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	iaNormalAttRef21.attachment = 2;

	// RP2::SP1::inputAttachment (specularity & roughness)
	VkAttachmentDescription iaSpecRougAtt21 = defaultAtt;
	iaSpecRougAtt21.format = swapChain.imageFormat;
	iaSpecRougAtt21.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	iaSpecRougAtt21.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference iaSpecRougAttRef21{};
	iaSpecRougAttRef21.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	iaSpecRougAttRef21.attachment = 3;

	// RP2::SP1::colorAttachment (color)
	VkAttachmentDescription caColorAtt21 = defaultAtt;
	caColorAtt21.format = swapChain.imageFormat;
	caColorAtt21.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	caColorAtt21.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference caColorAttRef21{};
	caColorAttRef21.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	caColorAttRef21.attachment = 4;

	// RP2::SP1::attachments
	std::vector<VkAttachmentDescription> allAttachments21 = { iaPosAtt21, iaAlbedoAtt21, iaNormalAtt21, iaSpecRougAtt21, caColorAtt21 };
	std::vector<VkAttachmentReference> inputAttachments21 = { iaPosAttRef21, iaAlbedoAttRef21, iaNormalAttRef21, iaSpecRougAttRef21 };
	VkAttachmentReference* depthAttachment21 = nullptr;
	std::vector<VkAttachmentReference> colorAttachments21 = { caColorAttRef21 };


	// RP3::SP1::depth/stencilAttachment (depth)
	VkAttachmentDescription depthAtt31 = defaultAtt;
	depthAtt31.format = c.deviceData.depthFormat;
	depthAtt31.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAtt31.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttRef31{};
	depthAttRef31.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttRef31.attachment = 0;

	// RP3::SP1::colorAttachment (color)
	VkAttachmentDescription caColorAtt31 = defaultAtt;
	caColorAtt31.format = swapChain.imageFormat;
	caColorAtt31.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	caColorAtt31.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference caColorAttRef31{};
	caColorAttRef31.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	caColorAttRef31.attachment = 1;

	// RP3::SP1::attachments
	std::vector<VkAttachmentDescription> allAttachments31 = { depthAtt31, caColorAtt31 };
	std::vector<VkAttachmentReference> inputAttachments31 = { };
	VkAttachmentReference* depthAttachment31 = &depthAttRef31;
	std::vector<VkAttachmentReference> colorAttachments31 = { caColorAttRef31 };


	// RP4::SP1::inputAttachment (color)
	VkAttachmentDescription iaColorAtt41 = defaultAtt;
	iaColorAtt41.format = swapChain.imageFormat;
	iaColorAtt41.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	iaColorAtt41.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference iaColorAttRef41{};
	iaColorAttRef41.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	iaColorAttRef41.attachment = 0;

	// RP4::SP1::inputAttachment (depth)
	VkAttachmentDescription iaDepthAtt41 = defaultAtt;
	iaDepthAtt41.format = c.deviceData.depthFormat;
	iaDepthAtt41.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	iaDepthAtt41.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference iaDepthAttRef41{};
	iaDepthAttRef41.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	iaDepthAttRef41.attachment = 1;
	
	// RP4::SP1::colorAttachment (finalColor)
	VkAttachmentDescription caColorAtt41 = defaultAtt;
	caColorAtt41.format = swapChain.imageFormat;
	caColorAtt41.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	caColorAtt41.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference caColorAttRef41{};
	caColorAttRef41.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	caColorAttRef41.attachment = 2;

	// RP4::SP1::attachments
	std::vector<VkAttachmentDescription> allAttachments41 = { iaColorAtt41, iaDepthAtt41, caColorAtt41 };
	std::vector<VkAttachmentReference> inputAttachments41 = { iaColorAttRef41, iaDepthAttRef41 };
	VkAttachmentReference* depthAttachment41 = { };
	std::vector<VkAttachmentReference> colorAttachments41 = { caColorAttRef41 };


	// Create render passes -------------------------

	renderPasses[0].createRenderPass(c.device, allAttachments11, inputAttachments11, colorAttachments11, depthAttachment11);
	renderPasses[1].createRenderPass(c.device, allAttachments21, inputAttachments21, colorAttachments21, depthAttachment21);
	renderPasses[2].createRenderPass(c.device, allAttachments31, inputAttachments31, colorAttachments31, depthAttachment31);
	renderPasses[3].createRenderPass(c.device, allAttachments41, inputAttachments41, colorAttachments41, depthAttachment41);
}

void RP_DS_PP::createImageResources()
{
	#ifdef DEBUG_ENV_CORE
		std::cout << "   " << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_NEAREST;
	samplerInfo.minFilter = VK_FILTER_NEAREST;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 1.0f;

	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerInfo.minLod = 0.f;
	samplerInfo.maxLod = 0.f;
	samplerInfo.mipLodBias = 0.0f;

	// Position -------------------------------------

	position.createFullImage(
		swapChain.extent.width,
		swapChain.extent.height,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT );

	position.createSampler(samplerInfo);

	// Albedo -------------------------------------

	albedo.createFullImage(
		swapChain.extent.width,
		swapChain.extent.height,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		swapChain.imageFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT
	);

	albedo.createSampler(samplerInfo);

	// Normal -------------------------------------

	normal.createFullImage(
		swapChain.extent.width,
		swapChain.extent.height,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT
	);

	normal.createSampler(samplerInfo);

	// specRoug -------------------------------------

	specRoug.createFullImage(
		swapChain.extent.width,
		swapChain.extent.height,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		swapChain.imageFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT );

	specRoug.createSampler(samplerInfo);

	// Depth -------------------------------------

	depth.createFullImage(
		swapChain.extent.width,
		swapChain.extent.height,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		c.deviceData.depthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_ASPECT_DEPTH_BIT );

	// Explicitly transition the layout of the image to a depth attachment (there is no need of doing this because we take care of this in the render pass, but this is here for completeness).
	commander.transitionImageLayout(depth.image, c.deviceData.depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);

	depth.createSampler(samplerInfo);

	// Color -------------------------------------

	color.createFullImage(
		swapChain.extent.width,
		swapChain.extent.height,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		swapChain.imageFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT
	);

	color.createSampler(samplerInfo);
}

void RP_DS_PP::destroyAttachments()
{
	position.destroy();
	albedo.destroy();
	normal.destroy();
	specRoug.destroy();
	depth.destroy();
	color.destroy();
}

void Commander::clearDepthBuffer(VkCommandBuffer commandBuffer, const SwapChain& swapChain)
{
	VkClearAttachment attachmentToClear;
	attachmentToClear.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	attachmentToClear.clearValue.depthStencil = { 1.0f, 0 };

	VkClearRect rectangleToClear;
	rectangleToClear.rect.offset = { 0, 0 };
	rectangleToClear.rect.extent = swapChain.extent;
	rectangleToClear.baseArrayLayer = 0;
	rectangleToClear.layerCount = 1;

	vkCmdClearAttachments(commandBuffer, 1, &attachmentToClear, 1, &rectangleToClear);
}
