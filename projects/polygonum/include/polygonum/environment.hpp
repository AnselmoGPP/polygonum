#ifndef ENVIRONMENT_HPP
#define ENVIRONMENT_HPP

#include <optional>					// std::optional<uint32_t> (Wrapper that contains no value until you assign something to it. Contains member has_value())
#include <mutex>
#include <queue>
#include <cstdint>
#include <thread>
//#include <cstdlib>			// EXIT_SUCCESS, EXIT_FAILURE
//#include <cstdint>			// UINT32_MAX
//#include <algorithm>			// std::min / std::max

#include "polygonum/toolkit.hpp"
#include "polygonum/input.hpp"


// Forward declarations ----------

class ModelsManager;


// Macros & names ----------

#define VAL_LAYERS					// Enable Validation layers

#ifdef VAL_LAYERS
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = false;
#endif

// Prototypes ----------

class VulkanCore;

struct QueueFamilyIndices;
struct SwapChainSupportDetails;
struct Image;
struct SwapChain;
struct DeviceData;

class  Subpass;
class  RenderPass;
class  Commander;

class RenderPipeline;
class RP_DS;
class RP_DS_PP;


// Definitions ----------

/// Structure for storing vector indices of the queue families we want. 
/** Note that graphicsFamilyand presentFamily could refer to the same queue family, but we included them separately because sometimes they are in different queue families. */
struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;		///< Queue family capable of computer graphics.
	std::optional<uint32_t> presentFamily;		///< Queue family capable of presenting to our window surface.
	bool isComplete();							///< Checks whether all members have value.
};

/// Structure containing details about the swap chain that must be checked. 
/** Though a swap chain may be available, it may not be compatible with our window surface, so we need to query for some detailsand check them.This struct will contain these details. */
struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR		capabilities;		// Basic surface capabilities: min/max number of images in swap chain, and min/max width/height of images.
	std::vector<VkSurfaceFormatKHR> formats;			// Surface formats: pixel format, color space.
	std::vector<VkPresentModeKHR>	presentModes;		// Available presentation modes
};

/// Image used as attachment in a render pass. One per render pass.
class Image
{
public:
	Image(VulkanCore& core, VkImage image = nullptr, VkDeviceMemory memory = nullptr, VkImageView view = nullptr, VkSampler sampler = nullptr);

	void destroy();

	void createFullImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImageAspectFlags aspectFlags);   //!< Create image, memory, and view.
	void createSampler(VkSamplerCreateInfo& samplerInfo);
	
	VkImage			image;		//!< Image object
	VkDeviceMemory	memory;		//!< Device memory object
	VkImageView		view;		//!< References a part of the image to be used (subset of its pixels). Required for being able to access it (images are accessed through image views rather than directly).
	VkSampler		sampler;	//!< Sampler object (it applies filtering and transformations to an image). It is a distinct object that provides an interface to extract colors from an image. It can be applied to any image you want(1D, 2D or 3D).

private:
	VulkanCore& c;
};

class SwapChain
{
public:
	SwapChain(VulkanCore& core, uint32_t additionalSwapChainImages);

	void createSwapChain();
	void destroy();
	size_t numImages();
	static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

	VkSwapchainKHR								swapChain;		//!< Swap chain object.
	std::vector<VkImage>						images;			//!< List. Opaque handle to an image object.
	std::vector<VkImageView>					views;			//!< List. Opaque handle to an image view object. It allows to use VkImage in the render pipeline. It's a view into an image; it describes how to access the image and which part of the image to access.

	VkFormat									imageFormat;	//!< VK_FORMAT_B8G8R8A8_SRGB
	VkExtent2D									extent;

private:
	VulkanCore& c;

	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D chooseSwapExtent(IOmanager& io, const VkSurfaceCapabilitiesKHR& capabilities);

	const uint32_t additionalSwapChainImages;
};

struct DeviceData
{
	void fillWithDeviceData(VkPhysicalDevice physicalDevice);
	void printData();

	VkPhysicalDeviceProperties	deviceProperties;	//!< Device properties: Name, type, supported Vulkan version...
	VkPhysicalDeviceFeatures	deviceFeatures;		//!< Device features: Texture compression, 64 bit floats, multi-viewport rendering...

	// Properties (redundant)
	uint32_t apiVersion;
	uint32_t driverVersion;
	uint32_t vendorID;
	uint32_t deviceID;
	VkPhysicalDeviceType deviceType;
	std::string deviceName;

	uint32_t maxUniformBufferRange;						//!< Max. uniform buffer object size (https://community.khronos.org/t/uniform-buffer-not-big-enough-how-to-handle/103981)
	uint32_t maxPerStageDescriptorUniformBuffers;
	uint32_t maxDescriptorSetUniformBuffers;
	uint32_t maxImageDimension2D;						//!< Useful for selecting a physical device
	uint32_t maxMemoryAllocationCount;					//!< Max. number of valid memory objects
	VkSampleCountFlags framebufferColorSampleCounts;	//!< Useful for getting max. number of MSAA
	VkSampleCountFlags framebufferDepthSampleCounts;	//!< Useful for getting max. number of MSAA
	VkDeviceSize minUniformBufferOffsetAlignment;		//!< Useful for aligning dynamic descriptor sets (usually == 32 or 256)

	// Features (redundant)
	VkBool32 samplerAnisotropy;							//!< Does physical device supports Anisotropic Filtering (AF)?
	VkBool32 largePoints;
	VkBool32 wideLines;

	// Others
	VkFormat depthFormat;

private:
	/// Take a list of candidate formats in order from most desirable to least desirable, and check which is the first one that is supported. The support of a format depends on the tiling mode and on the usage (features).
	VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
};

class ValLayers
{
public:
	ValLayers();

	const std::vector<const char*> requiredValidationLayers = { "VK_LAYER_KHRONOS_validation" };

	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);   // Fill a createInfo struct for creating debugMessenger.
	void setupDebugMessenger(VkInstance instance);   // Create the debugMessenger (and save instance).
	void DestroyDebugUtilsMessengerEXT();   // Destroy the debugMessenger. Call it before destroying the Vulkan instance.

private:
	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;		//!< Opaque handle to a debug messenger object (the debug callback is part of it).

	bool checkValidationLayerSupport();
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
	VkResult CreateDebugUtilsMessengerEXT(const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
};

class Extensions
{
public:
	std::vector<const char*> getRequiredExtensions_device();   //!< Required device extensions. Used for creating logical device. Swap chain: Queue of images that are waiting to be presented to the screen. Our application will acquire such an image to draw to it, and then return it to the queue. Its general purpose is to synchronize the presentation of images with the refresh rate of the screen.
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);   // Check if device extensions are supported. Used for evaluating device. 

	std::vector<const char*> getRequiredExtensions_glfw_valLayers();   // Required GLFW (and Validation layers) extensions. Used for creating Vulkan instance. 
	bool checkExtensionSupport(const char* const* requiredExtensions, uint32_t reqExtCount);   // Check if extensions (for GLFW and Validation layers) are supported. For creating Vulkan instance. 
};

class VulkanCore
{
public:
	VulkanCore(int width, int height);

	const bool add_MSAA = false;					//!< Shader MSAA (MultiSample AntiAliasing). 
	const bool add_SS   = false;					//!< Sample shading. This can solve some problems from shader MSAA (example: only smoothens out edges of geometry but not the interior filling) (https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#primsrast-sampleshading).

	IOmanager io;

	VkInstance					instance;			//!< Opaque handle to an instance object. There is no global state in Vulkan and all per-application state is stored here.
	VkSurfaceKHR				surface;			//!< Opaque handle to a surface object (abstract type of surface to present rendered images to)

	VkPhysicalDevice			physicalDevice;		//!< Opaque handle to a physical device object.
	VkDevice					device;				//!< Opaque handle to a logical device object.
	DeviceData					deviceData;			//!< Physical device properties and features.
	VkSampleCountFlagBits		msaaSamples;		//!< Number of samples used for MSAA (MultiSampling AntiAliasing)

	VkQueue						graphicsQueue;		//!< Opaque handle to a queue object (computer graphics).
	VkQueue						presentQueue;		//!< Opaque handle to a queue object (presentation to window surface).

	int memAllocObjects;							//!< Number of memory allocated objects (must be <= maxMemoryAllocationCount). Incremented each vkAllocateMemory call; decremented each vkFreeMemory call. Increments when creating an image or buffer; decrements when destroyed.

	void queueWaitIdle(VkQueue queue, std::mutex* waitMutex);
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	void destroy();

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);   //!< Creates a Vulkan buffer (VkBuffer and VkDeviceMemory).Used as friend in modelData, UBO and Texture.
	void destroyBuffer(VkDevice device, VkBuffer buffer, VkDeviceMemory memory);

	void createImage(VkImage& destImage, VkDeviceMemory& destMemory, uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
	void createImageView(VkImageView& destImageView, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
	void createSampler(VkSampler& destSampler, VkSamplerCreateInfo& samplerInfo);
	void destroyImage(Image* image);

private:
	//const std::vector<const char*> requiredDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };	//!< Swap chain: Queue of images that are waiting to be presented to the screen. Our application will acquire such an image to draw to it, and then return it to the queue. Its general purpose is to synchronize the presentation of images with the refresh rate of the screen.
	ValLayers valLayers;
	Extensions ext;

	void createInstance();
	void createSurface();
	void pickPhysicalDevice();
	void createLogicalDevice();

	int evaluateDevice(VkPhysicalDevice device);
	VkSampleCountFlagBits getMaxUsableSampleCount(bool getMinimum);
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};

class Subpass
{
public:
	Subpass(std::vector<Image*> inputAttachments, uint32_t colorAttachmentsCount)
		: inputAtts(inputAttachments), colorAttsCount(colorAttachmentsCount) { }

	std::vector<Image*> inputAtts;	//!< Input attachments (input images) per subpass
	uint32_t colorAttsCount;		//!< Number of color attachments (output images) per subpass
};

class RenderPass
{
public:
	RenderPass(std::vector<Subpass> subpasses) : subpasses(subpasses) { }

	void createRenderPass(VkDevice& device, std::vector<VkAttachmentDescription>& allAttachments, std::vector<VkAttachmentReference>& inputAttachments, std::vector<VkAttachmentReference>& colorAttachments, VkAttachmentReference* depthAttachment);
	void createFramebuffers(VulkanCore& c, SwapChain& swapChain);   //!< Define the swap chain framebuffers and their attachments. Framebuffers directly depend on the swap chain images.
	void createRenderPassInfo(SwapChain& swapChain);		//!< Create the VkRenderPassBeginInfo objects used at vkCmdBeginRenderPass() for creating the command buffer.
	void destroy(VulkanCore& c);						//!< Destroy framebuffers and render-pass.

	VkRenderPass renderPass;
	std::vector<Subpass> subpasses;	
	std::vector<std::vector<VkImageView*>> attachments;		//!< One set of attachments per swapchain image.
	std::vector<VkFramebuffer> framebuffers;				//!< One per swap chain image. List. Opaque handle to a framebuffer object (set of attachments, including the final image to render). Access: swapChainFramebuffers[numSwapChainImages][attachment]. First attachment: main color. Second attachment: post-processing
	std::vector<VkRenderPassBeginInfo> renderPassInfos;		//!< One per swap chain image.
	std::vector<VkClearValue> clearValues;					//!< One per attachment.
};

class Commander
{
public:
	Commander(VulkanCore& core, size_t swapChainImagesCount, size_t maxFramesInFlight);

	std::vector<VkCommandPool> commandPools;   //!< commandPools[frame]. Opaque handle to a command pool object. It manages the memory that is used to store the buffers, and command buffers are allocated from them. One per frame (for better performance).
	std::vector<std::vector<VkCommandBuffer>> commandBuffers;			//!< commandBuffers[frame][swapchain images]

	std::vector<VkSemaphore> imageAvailableSemaphores;	//!< [frame]. Signals that an image has been acquired from the swap chain and is ready for rendering (CB execution). Each frame has a semaphore for concurrent processing. Allows multiple frames to be in-flight while still bounding the amount of work that piles up. One for each possible frame in flight.
	std::vector<VkSemaphore> renderFinishedSemaphores;	//!< [frame]. Signals that rendering has finished (CB has been executed) and is ready for presentation. Each frame has a semaphore for concurrent processing. Allows multiple frames to be in-flight while still bounding the amount of work that piles up. One for each possible frame in flight.
	std::vector<VkFence> framesInFlight;   //!< [frame]. Fence occupied during rendering (CB execution).
	std::vector<std::pair<VkFence, size_t>> imagesInFlight;   //!< [swapChain image]. std::pair<fence, associated_frameIndex>, Copy of the last fence used for this image. Maps frames in flight by their fences. Tracks for each swap chain image if a frame in flight is currently using it. One per swap chain image.

	std::mutex mutQueue;					//!< Controls that vkQueueSubmit is not used in two threads simultaneously (Environment -> endSingleTimeCommands(), and Renderer -> createCommandBuffers)
	std::vector<std::mutex> mutCommandPool;   //!< [frame]. The same command pool cannot be used simultaneously in 2 different threads. Problem: It is used at command buffer creation (Renderer, 1st thread, at updateCB), and beginSingleTimeCommands and endSingleTimeCommands (Environment, 2nd thread, indirectly used in loadAndCreateTexture & fullConstruction), and indirectly sometimes (command buffer).
	std::vector<std::mutex> mutFrame;   //!< [frame]. This prevents two threads from drawing (acquire-update-submit-present) for the same frame.

	bool updateCommandBuffer;
	size_t commandsCount;				//!< Number of drawing commands sent to the command buffer. For debugging purposes.

	/*
		@brief Allocates command buffers and record drawing commands in them.

		Commands issued depends upon: SwapChainImages · Layer · Model · numRenders
		Bindings: pipeline > vertex buffer > indices > descriptor set > draw
		Render same model with different descriptors (used here):
		<ul>
			<li>You technically don't have multiple uniform buffers; you just have one. But you can use the offset(s) provided to vkCmdBindDescriptorSets to shift where in that buffer the next rendering command(s) will get their data from. Basically, you rebind your descriptor sets, but with different pDynamicOffset array values.</li>
			<li>Your pipeline layout has to explicitly declare those descriptors as being dynamic descriptors. And every time you bind the set, you'll need to provide the offset into the buffer used by that descriptor.</li>
			<li>More: https://stackoverflow.com/questions/45425603/vulkan-is-there-a-way-to-draw-multiple-objects-in-different-locations-like-in-d </li>
		</ul>

		Another option: Instance rendering allows to perform a single draw call. As I understood, UBO can be passed in the following ways:
		<ul>
			<li>In a non-dynamic descriptor (problem: shader has to contain the declaration of one UBO for each instance).</li>
			<li>As a buffer's attribute (problem: it is non-modifyable).</li>
			<li>In a dynamic descriptor (solves both previous problems), but requires many draw calls (defeats the whole purpose of instance rendering).</li>
			<li>https://stackoverflow.com/questions/54619507/whats-the-correct-way-to-implement-instanced-rendering-in-vulkan </li>
			<li>https://www.reddit.com/r/vulkan/comments/hhoktq/rendering_multiple_objects/ </li>
		</ul>
	*/
	void updateCommandBuffers(ModelsManager& models, std::shared_ptr<RenderPipeline> renderPipeline, size_t swapChainImagesCount, size_t frameIndex);
	void createCommandPool(size_t numFrames);   //!< Commands in Vulkan (drawing, memory transfers, etc.) are not executed directly using function calls, you have to record all of the operations you want to perform in command buffer objects. After setting up the drawing commands, just tell Vulkan to execute them in the main loop.
	void createCommandBuffers(size_t numSwapChainImages, size_t numFrames);
	uint32_t getNextFrame();   //!< Increment currentFrame by one, but loop around when reaching "maxFramesInFlight".
	size_t numFrames();

	// Single time commands
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	// Cleanup
	void freeCommandBuffers();
	void destroyCommandPool();
	void destroySynchronizers();

private:
	VulkanCore& c;

	uint32_t lastFrame;   //!< Frame to process next. Different frames can be processed concurrently.
	const size_t swapChainImagesCount;
	const size_t maxFramesInFlight;
	std::mutex mutGetNextFrame;

	void createSynchronizers(size_t numSwapchainImages, size_t numFrames);   //!< Create semaphores and fences for synchronizing the events occuring in each frame (drawFrame()).
	VkCommandBuffer	beginSingleTimeCommands(uint32_t frameIndex);
	void endSingleTimeCommands(uint32_t frameIndex, VkCommandBuffer commandBuffer);// <<< commandBuffer as argument?
	bool hasStencilComponent(VkFormat format);
	void clearDepthBuffer(VkCommandBuffer commandBuffer, const SwapChain& swapChain);   //!< [Not used] Used for clearing depth buffer between sets of draw commands in order to apply Painter's algorithm.
};

/**
	@brief Set the system of render passes, subpasses, and framebuffers used.

	It tells Vulkan the framebuffer attachments that will be used while rendering (input, color/output, depth, multisampled images). A render-pass denotes more explicitly how your rendering happens. Specify subpasses and their attachments.
	- Subpasses : A single render pass can consist of multiple subpasses, which are subsequent rendering operations that depend on the contents of framebuffers in previous passes (example : a sequence of post-processing effects applied one after another). Grouping them into one render pass may give better performance.
	- Attachment references: Every subpass references one or more of the attachments.
		- Input attachments (IA): Input images.
		- Color attachments (CA): Output images.
		- Depth/stencil attachment (DA): Depth/stencil buffer.
		- Resolve attachment (RA): Used for resolving the final image from a multisampled image.
*/
class RenderPipeline
{
public:
	RenderPipeline(VulkanCore& core, SwapChain& swapChain, Commander& commander);

	std::vector<RenderPass> renderPasses;
	Subpass& getSubpass(unsigned renderPassIndex, unsigned subpassIndex);
	void createRenderPipeline();   //!< Create render-passes, framebuffers, and attachments.
	void destroyRenderPipeline();   //!< Destroy render-passes, framebuffers, and attachments.

protected:
	VulkanCore& c;
	SwapChain& swapChain;
	Commander& commander;

private:
	virtual void createRenderPass() = 0;   //!< A render-pass denotes more explicitly how your rendering happens. Specify subpasses and their attachments.
	virtual void createImageResources() = 0;
	virtual void destroyAttachments() = 0;
};

/// Render pipeline containing Deferred shading (lighting pass + geometry pass)
class RP_DS : public RenderPipeline
{
public:
	RP_DS(VulkanCore& core, SwapChain& swapChain, Commander& commander);

	Image position;
	Image albedo;
	Image normal;
	Image specRoug;
	Image depth;
	//Image finalColor;	// swapchain.image[i]

protected:
	void createRenderPass() override;
	void createImageResources() override;
	void destroyAttachments() override;
};

/**
  Render pipeline containing Deferred shading(lighting pass + geometry pass) + forward shading + post - processing

  RP1::SP1 (Geometry pass):
    IA (0)
    DA (1): depth
    CA (4): position, albedo, normal, specRough
  RP2::SP1 (Lighting pass):
    IA (4): CAs from RP1
    DA (0)
    CA (1): color
  RP3::SP1 (Forward pass):
    IA (0):
    DA (1): depth (from RP1)
    CA (1): color (from RP2)
  RP4::SP1 (Post-processing pass):
    IA (2): color (from RP3), depth (from RP3)
    DA (0)
    CA (1): color (swapchain)
*/
class RP_DS_PP : public RenderPipeline
{
public:
	RP_DS_PP(VulkanCore& core, SwapChain& swapChain, Commander& commander);

	Image position;
	Image albedo;
	Image normal;
	Image specRoug;
	Image depth;
	Image color;
	//Image finalColor;	// swapchain.image[i]

protected:
	void createRenderPass() override;
	void createImageResources() override;
	void destroyAttachments() override;
};


#endif