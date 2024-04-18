#ifndef ENVIRONMENT_HPP
#define ENVIRONMENT_HPP

#include <vector>
#include <optional>					// std::optional<uint32_t> (Wrapper that contains no value until you assign something to it. Contains member has_value())
#include <mutex>

//#include "vulkan/vulkan.h"		// From LunarG SDK. Can be used for off-screen rendering
//#define GLFW_INCLUDE_VULKAN		// Makes GLFW load the Vulkan header with it
//#include "GLFW/glfw3.h"

#include "input.hpp"

#define VAL_LAYERS					// Enable Validation layers

#ifdef VAL_LAYERS
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = false;
#endif


// Prototypes ----------

class VulkanCore;
class VulkanEnvironment;

struct QueueFamilyIndices;
struct SwapChainSupportDetails;
struct Image;
struct SwapChain;
struct DeviceData;

class RenderingWorkflow;
class RP_DS;
class RP_DS_PP;

/// Common function. Creates a Vulkan buffer (VkBuffer and VkDeviceMemory).Used as friend in modelData, UBO and Texture.
void createBuffer(VulkanEnvironment* e, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);


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
struct Image
{
	Image();
	void destroy(VulkanEnvironment* e);

	VkImage			image;		//!< Image object
	VkDeviceMemory	memory;		//!< Device memory object
	VkImageView		view;		//!< References a part of the image to be used (subset of its pixels). Required for being able to access it.
	VkSampler		sampler;	//!< Images are accessed through image views rather than directly
};

struct SwapChain
{
	SwapChain();
	void destroy(VkDevice device);

	VkSwapchainKHR								swapChain;		//!< Swap chain object.
	std::vector<VkImage>						images;			//!< List. Opaque handle to an image object.
	std::vector<VkImageView>					views;			//!< List. Opaque handle to an image view object. It allows to use VkImage in the render pipeline. It's a view into an image; it describes how to access the image and which part of the image to access.

	VkFormat									imageFormat;	//!< VK_FORMAT_B8G8R8A8_SRGB
	VkExtent2D									extent;
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


class VulkanCore
{
public:
	VulkanCore(IOmanager& io);

	const bool add_MSAA = false;			//!< Shader MSAA (MultiSample AntiAliasing). 
	const bool add_SS   = false;			//!< Sample shading. This can solve some problems from shader MSAA (example: only smoothens out edges of geometry but not the interior filling) (https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#primsrast-sampleshading).

	VkInstance					instance;			//!< Opaque handle to an instance object. There is no global state in Vulkan and all per-application state is stored here.
	VkDebugUtilsMessengerEXT	debugMessenger;		//!< Opaque handle to a debug messenger object (the debug callback is part of it).
	VkSurfaceKHR				surface;			//!< Opaque handle to a surface object (abstract type of surface to present rendered images to)

	VkPhysicalDevice			physicalDevice;		//!< Opaque handle to a physical device object.
	VkSampleCountFlagBits		msaaSamples;		//!< Number of samples used for MSAA (MultiSampling AntiAliasing)
	VkDevice					device;				//!< Opaque handle to a device object.
	DeviceData					deviceData;			//!< Physical device properties and features.

	VkQueue						graphicsQueue;		//!< Opaque handle to a queue object (computer graphics).
	VkQueue						presentQueue;		//!< Opaque handle to a queue object (presentation to window surface).

	int memAllocObjects;							//!< Number of memory allocated objects (must be <= maxMemoryAllocationCount). Incremented each vkAllocateMemory call; decremented each vkFreeMemory call.

	SwapChainSupportDetails	querySwapChainSupport();
	QueueFamilyIndices findQueueFamilies();
	void destroy();

private:
	IOmanager& io;

	const std::vector<const char*> requiredValidationLayers = { "VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*> requiredDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };	//!< Swap chain: Queue of images that are waiting to be presented to the screen. Our application will acquire such an image to draw to it, and then return it to the queue. Its general purpose is to synchronize the presentation of images with the refresh rate of the screen.

	void initWindow();
	void createInstance();
	void setupDebugMessenger();
	void createSurface();
	void pickPhysicalDevice();
	void createLogicalDevice();

	bool checkValidationLayerSupport(const std::vector<const char*>& requiredLayers);
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	std::vector<const char*> getRequiredExtensions();
	bool checkExtensionSupport(const char* const* requiredExtensions, uint32_t reqExtCount);
	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
	int evaluateDevice(VkPhysicalDevice device);
	VkSampleCountFlagBits getMaxUsableSampleCount(bool getMinimum);
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	SwapChainSupportDetails	querySwapChainSupport(VkPhysicalDevice device);
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
	void createFramebuffers(VulkanEnvironment& e);			//!< Define the swap chain framebuffers and their attachments. Framebuffers directly depend on the swap chain images.
	void createRenderPassInfo(VulkanEnvironment& e);		//!< Create the VkRenderPassBeginInfo objects used at vkCmdBeginRenderPass() for creating the command buffer.
	void destroy(VulkanEnvironment& e);						//!< Destroy framebuffers and render-pass.

	VkRenderPass renderPass;
	std::vector<Subpass> subpasses;	
	std::vector<std::vector<VkImageView*>> attachments;			//!< One set of attachments per swapchain image.
	std::vector<VkFramebuffer> framebuffers;					//!< One per swap chain image. List. Opaque handle to a framebuffer object (set of attachments, including the final image to render). Access: swapChainFramebuffers[numSwapChainImages][attachment]. First attachment: main color. Second attachment: post-processing
	std::vector<VkRenderPassBeginInfo> renderPassInfos;			//!< One per swap chain image.
	std::vector<VkClearValue> clearValues;						//!< One per attachment.
};

/**
	@brief Set the system of render passes and framebuffers used.

	Tells Vulkan the framebuffer attachments that will be used while rendering (color, depth, multisampled images). A render-pass denotes more explicitly how your rendering happens. Specify subpasses and their attachments.
	- Subpasses : A single render pass can consist of multiple subpasses, which are subsequent rendering operations that depend on the contents of framebuffers in previous passes (example : a sequence of post-processing effects applied one after another). Grouping them into one render pass may give better performance.
	- Attachment references: Every subpass references one or more of the attachments.
		- Input attachments: Input images.
		- Color attachments: Output images.
		- Depth/stencil attachment: Depth/stencil buffer.
		- Resolve attachment: Used for resolving the final image from a multisampled image.
*/
class RenderPipeline
{
public:
	RenderPipeline(VulkanEnvironment& e);

	std::vector<RenderPass> renderPasses;
	Subpass& getSubpass(unsigned renderPassIndex, unsigned subpassIndex);

	friend VulkanEnvironment;

protected:
	VulkanEnvironment& e;

	void createRenderPipeline();		//!< Create render-passes, framebuffers, and attachments.
	void destroyRenderPipeline();		//!< Destroy render-passes, framebuffers, and attachments.

private:
	virtual void destroyAttachments() = 0;
	virtual void createRenderPass() = 0;		//!< A render-pass denotes more explicitly how your rendering happens. Specify subpasses and their attachments.
	virtual void createImageResources() = 0;
};

/// Render pipeline containing Deferred shading (lighting pass + geometry pass)
class RP_DS : public RenderPipeline
{
public:
	RP_DS(VulkanEnvironment& e);

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

/// Render pipeline containing Deferred shading (lighting pass + geometry pass) + forward shading + post-processing
class RP_DS_PP : public RenderPipeline
{
public:
	RP_DS_PP(VulkanEnvironment& e);

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

class VulkanEnvironment
{
	IOmanager& io;

public:
	VulkanEnvironment(IOmanager& io);
	~VulkanEnvironment();

	VulkanCore c;

	void			createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
	uint32_t		findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	void			transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);	//!< Submit a pipeline barrier. It specifies when a transition happens: when the pipeline finishes (source) and the next one starts (destination). No command may start before it finishes transitioning. Commands come at the top of the pipeline (first stage), shaders are executed in order, and commands retire at the bottom of the pipeline (last stage), when execution finishes. This barrier will wait for everything to finish and block any work from starting.
	VkCommandBuffer	beginSingleTimeCommands();
	void			endSingleTimeCommands(VkCommandBuffer commandBuffer);
	VkImageView		createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

	void			recreate_RenderPipeline_SwapChain();
	void			cleanup_RenderPipeline_SwapChain();
	void			cleanup();

	// Main member variables:
	VkCommandPool commandPool;				//!< Opaque handle to a command pool object. It manages the memory that is used to store the buffers, and command buffers are allocated from them. 

	std::shared_ptr<RenderPipeline> rp;		//!< Render pipeline
	SwapChain swapChain;					// Final color. Swapchain elements.

	std::mutex mutQueue;					//!< Controls that vkQueueSubmit is not used in two threads simultaneously (Environment -> endSingleTimeCommands(), and Renderer -> createCommandBuffers)
	std::mutex mutCommandPool;				//!< Command pool cannot be used simultaneously in 2 different threads. Problem: It is used at command buffer creation (Renderer, 1st thread, at updateCB), and beginSingleTimeCommands and endSingleTimeCommands (Environment, 2nd thread, indirectly used in loadAndCreateTexture & fullConstruction), and indirectly sometimes (command buffer).

private:
	void createSwapChain();
	void createSwapChainImageViews();

	void createCommandPool();

	// Helper methods:
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	bool hasStencilComponent(VkFormat format);
};

#endif