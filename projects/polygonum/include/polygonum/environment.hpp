#ifndef ENVIRONMENT_HPP
#define ENVIRONMENT_HPP

#include <optional>					// std::optional<uint32_t> (Wrapper that contains no value until you assign something to it. Contains member has_value())
#include <mutex>
#include <queue>
#include <cstdint>
//#include <cstdlib>			// EXIT_SUCCESS, EXIT_FAILURE
//#include <cstdint>			// UINT32_MAX
//#include <algorithm>			// std::min / std::max

#include "polygonum/toolkit.hpp"
#include "polygonum/models.hpp"
#include "polygonum/commons.hpp"
#include "polygonum/input.hpp"


// Macros & names ----------

#define VAL_LAYERS					// Enable Validation layers

#ifdef VAL_LAYERS
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = false;
#endif

// Prototypes ----------

class VulkanCore;
class Renderer;

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

class LoadingWorker;

// Common functions ----------

/// Creates a Vulkan buffer (VkBuffer and VkDeviceMemory).Used as friend in modelData, UBO and Texture.
void createBuffer(VulkanCore* c, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);


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
	Image(VulkanCore& core);

	void createFullImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImageAspectFlags aspectFlags);
	void destroy();

	//void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
	//void createImageView(VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

	static void createImage(VkImage& destImage, VkDeviceMemory& destImageMemory, VulkanCore& core, uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
	static void createImageView(VkImageView& destImageView, VulkanCore& core, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

	VkImage			image;		//!< Image object
	VkDeviceMemory	memory;		//!< Device memory object
	VkImageView		view;		//!< References a part of the image to be used (subset of its pixels). Required for being able to access it.
	VkSampler		sampler;	//!< Images are accessed through image views rather than directly

private:
	VulkanCore& c;
};

class SwapChain
{
	VulkanCore& c;

	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D chooseSwapExtent(IOmanager& io, const VkSurfaceCapabilitiesKHR& capabilities);

	const uint32_t additionalSwapChainImages;

public:
	SwapChain(VulkanCore& core, uint32_t additionalSwapChainImages);

	void createSwapChain();
	void destroy();
	size_t imagesCount();

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
	VulkanCore(int width, int height);

	const bool add_MSAA = false;					//!< Shader MSAA (MultiSample AntiAliasing). 
	const bool add_SS   = false;					//!< Sample shading. This can solve some problems from shader MSAA (example: only smoothens out edges of geometry but not the interior filling) (https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#primsrast-sampleshading).

	IOmanager io;

	VkInstance					instance;			//!< Opaque handle to an instance object. There is no global state in Vulkan and all per-application state is stored here.
	VkDebugUtilsMessengerEXT	debugMessenger;		//!< Opaque handle to a debug messenger object (the debug callback is part of it).
	VkSurfaceKHR				surface;			//!< Opaque handle to a surface object (abstract type of surface to present rendered images to)

	VkPhysicalDevice			physicalDevice;		//!< Opaque handle to a physical device object.
	VkSampleCountFlagBits		msaaSamples;		//!< Number of samples used for MSAA (MultiSampling AntiAliasing)
	VkDevice					device;				//!< Opaque handle to a logical device object.
	DeviceData					deviceData;			//!< Physical device properties and features.

	VkQueue						graphicsQueue;		//!< Opaque handle to a queue object (computer graphics).
	VkQueue						presentQueue;		//!< Opaque handle to a queue object (presentation to window surface).

	int memAllocObjects;							//!< Number of memory allocated objects (must be <= maxMemoryAllocationCount). Incremented each vkAllocateMemory call; decremented each vkFreeMemory call.

	//void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
	//VkImageView	createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
	SwapChainSupportDetails	querySwapChainSupport();
	QueueFamilyIndices findQueueFamilies();
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	void destroy();
	void queueWaitIdle(VkQueue queue, std::mutex* waitMutex);

private:

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
	uint32_t lastFrame;   //!< Frame to process next. Different frames can be processed concurrently.
	const size_t swapChainImagesCount;
	const size_t maxFramesInFlight;
	std::mutex mutGetNextFrame;

	VulkanCore* core;

	void createSynchronizers(VulkanCore* core, size_t numSwapchainImages, size_t numFrames);   //!< Create semaphores and fences for synchronizing the events occuring in each frame (drawFrame()).
	VkCommandBuffer	beginSingleTimeCommands(uint32_t frameIndex);
	void endSingleTimeCommands(uint32_t frameIndex, VkCommandBuffer commandBuffer);// <<< commandBuffer as argument?
	bool hasStencilComponent(VkFormat format);
	void clearDepthBuffer(VkCommandBuffer commandBuffer, const SwapChain& swapChain);   //!< [Not used] Used for clearing depth buffer between sets of draw commands in order to apply Painter's algorithm.

public:
	Commander(VulkanCore* core, size_t swapChainImagesCount, size_t maxFramesInFlight);

	std::vector<VkCommandPool> commandPools;   //!< commandPools[frame]. Opaque handle to a command pool object. It manages the memory that is used to store the buffers, and command buffers are allocated from them. One per frame (for better performance).
	std::vector<std::vector<VkCommandBuffer>> commandBuffers;			//!< commandBuffers[frame][swapchain images]

	std::vector<VkSemaphore> imageAvailableSemaphores;	//!< [frame]. Signals that an image has been acquired from the swap chain and is ready for rendering (CB execution). Each frame has a semaphore for concurrent processing. Allows multiple frames to be in-flight while still bounding the amount of work that piles up. One for each possible frame in flight.
	std::vector<VkSemaphore> renderFinishedSemaphores;	//!< [frame]. Signals that rendering has finished (CB has been executed) and is ready for presentation. Each frame has a semaphore for concurrent processing. Allows multiple frames to be in-flight while still bounding the amount of work that piles up. One for each possible frame in flight.
	std::vector<VkFence> framesInFlight;   //!< [frame]. Fence occupied during rendering (CB execution).
	std::vector<VkFence> imagesInFlight;   //!< [swapChain image]. Copy of the last fence used for this image. Maps frames in flight by their fences. Tracks for each swap chain image if a frame in flight is currently using it. One per swap chain image.

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
	void createCommandBuffers(ModelsManager& models, std::shared_ptr<RenderPipeline> renderPipeline, size_t swapChainImagesCount, size_t frameIndex);
	void createCommandPool(VulkanCore* core, size_t numFrames);
	uint32_t getNextFrame();   //!< Increment currentFrame by one, but loop around when reaching "maxFramesInFlight".

	// Single time commands
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	// Cleanup
	void freeCommandBuffers();
	void destroyCommandPool();
	void destroySynchronizers();
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
	RenderPipeline(VulkanCore& core, SwapChain& swapChain);

	std::vector<RenderPass> renderPasses;
	Subpass& getSubpass(unsigned renderPassIndex, unsigned subpassIndex);
	void createRenderPipeline(Commander& commands);   //!< Create render-passes, framebuffers, and attachments.
	void destroyRenderPipeline();   //!< Destroy render-passes, framebuffers, and attachments.

protected:
	VulkanCore& c;
	SwapChain& swapChain;

private:
	virtual void createRenderPass() = 0;   //!< A render-pass denotes more explicitly how your rendering happens. Specify subpasses and their attachments.
	virtual void createImageResources(Commander& commander) = 0;
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
	void createImageResources(Commander& commander) override;
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
	void createImageResources(Commander& commander) override;
	void destroyAttachments() override;
};

/// Reponsible for the loading thread and its processes.
class LoadingWorker
{
public:
	LoadingWorker(int waitTime);
	~LoadingWorker();

	static enum Task { none, construct, delet };   //!< Used in LoadingWorker::newTask().

	std::mutex mutModels;   //!< for Renderer::models
	std::mutex mutTasks;   //!< for LoadingWorker::tasks

	std::mutex mutModelTP;

	std::mutex mutLoad;			//!< for Renderer::modelsToLoad
	std::mutex mutDelete;		//!< for Renderer::modelsToDelete
	std::mutex mutResources;	//!< for Renderer::shaders & Renderer::textures

	void start(Renderer* renderer, ModelsManager* models, Commander* commandData);
	void stop();
	void newTask(key64 key, Task task);

private:
	std::queue<std::pair<key64, LoadingWorker::Task>> tasks;   //!< FIFO queue
	std::unordered_map<key64, ModelData> modelTP;   //!< Model To Process: A model is moved here temporarily for processing. After processing, it's tranferred to its final destination.

	//std::unordered_map<key64, ModelData>& models;
	//std::list<Texture>& textures;
	//std::list<Shader>& shaders;

	//bool&					updateCommandBuffer;

	int						waitTime;				//!< Time (milliseconds) the loading-thread wait till next check.
	bool					runThread;				//!< Signals whether the secondary thread (loadingThread) should be running.
	std::thread				thread_loadModels;		//!< Thread for loading new models. Initiated in the constructor. Finished if glfwWindowShouldClose

	/**
		@brief Load and delete models (including their shaders and textures)

		<ul> Process:
			<li>  Initializes and moves models from modelsToLoad to models </li>
			<li>  Deletes models from modelsToDelete </li>
				<li> Deletes shaders and textures with counter == 0 </li>
		</ul>
	*/
	void thread_loadData(Renderer* renderer, ModelsManager* models, Commander* commandData);
	void extractModel(ModelsManager& models, key64 key);   //!< Extract model from "models" to "modelTP"
	void returnModel(ModelsManager& models, key64 key);   //!< Extract model from "modelTP" to "models"

};

// LOOK Restart the Renderer object after finishing the render loop
/**
*   @brief Responsible for making the rendering (render loop). Manages models, textures, input, camera...
*
*	It creates a VulkanEnvironment and, when the user wants, a ModelData (newModel()).
*/
class Renderer
{
protected:
	const uint32_t ADDITIONAL_SWAPCHAIN_IMAGES = 3;   //!< Total number of swapchain images = swapChain_capabilities_minImageCount + ADDITIONAL_SWAPCHAIN_IMAGES
	const uint32_t MAX_FRAMES_IN_FLIGHT = 4;   //!< How many frames should be processed concurrently.

	friend ResourcesLoader;
	friend LoadingWorker;
	friend ModelData;
	friend UBO;
	friend VertexesLoader;
	friend Texture;
	friend TextureLoader;

	VulkanCore c;
	SwapChain swapChain;					// Final color. Swapchain elements.
	Commander commander;
	std::shared_ptr<RenderPipeline> rp;		//!< Render pipeline
	Timer timer, profiler;
	ModelsManager models;
	PointersManager<std::string, Texture> textures;			//!< Set of textures
	PointersManager<std::string, Shader> shaders;			//!< Set of shaders
	LoadingWorker worker;

	size_t renderedFramesCount; //!< Number of frames rendered
	int maxFPS; //!< Maximum FPS (= 30 by default).

	key64 lightingPass; //!< Optional (createLighting())
	key64 postprocessingPass; //!< Optional (createPostprocessing())

	// Main methods:

	/**
	*	Acquire image from swap chain, execute command buffer with that image as attachment in the framebuffer, and return the image to the swap chain for presentation.
	*	This method performs 3 operations asynchronously (the function call returns before the operations are finished, with undefined order of execution):
	*	<ul>
	*		<li>vkAcquireNextImageKHR: Acquire an image from the swap chain (imageAvailableSemaphores)</li>
	*		<li>vkQueueSubmit: Execute the command buffer with that image as attachment in the framebuffer (renderFinishedSemaphores, inFlightFences)</li>
	*		<li>vkQueuePresentKHR: Return the image to the swap chain for presentation</li>
	*	</ul>
	*	Each of the operations depend on the previous one finishing, so we need to synchronize the swap chain events.
	*	Two ways: semaphores (mainly designed to synchronize within or across command queues. Best fit here) and fences (mainly designed to synchronize your application itself with rendering operation).
	*	Synchronization examples: https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#swapchain-image-acquire-and-present
	*/
	/// Draw a frame: Wait for previous command buffer execution, acquire image from swapchain, update states and command buffer, submit command buffer for execution, and present result for display on screen.
	void drawFrame();

	/// Update uniforms, transformation matrices, add/delete new models/textures. Transformation matrices (MVP) will be generated each frame.
	void updateStates(uint32_t currentImage);

	/// Callback used by the client for updating states of their models
	void(*userUpdate) (Renderer& rend);

	void cleanup();   //!< Cleanup after render loop terminates
	void recreateSwapChain();   //!< Used in drawFrame() in case the window surface changes (like when window resizing), making swap chain no longer compatible with it. Here, we catch these events (when acquiring/submitting an image from/to the swap chain) and recreate the swap chain.

public:
	// LOOK what if firstModel.size() == 0
	/// Constructor. Requires a callback for user updates (update model matrix, add models, delete models...).
	Renderer(void(*graphicsUpdate)(Renderer&), int width, int height, UBOinfo globalUBO_vs, UBOinfo globalUBO_fs);
	virtual ~Renderer();

	UBO globalUBO_vs;
	UBO globalUBO_fs;

	void renderLoop();	//!< Create command buffer and start render loop.

	key64 newModel(ModelDataInfo& modelInfo);   //!< Create (partially) a new model in the list modelsToLoad. Used for rendering a model.
	void deleteModel(key64 key);   //!< Move model from list models (or modelsToLoad) to list modelsToDelete. If the model is being fully constructed (by the worker), it waits until it finishes. Note: When the app closes, it destroys Renderer. Thus, don't use this method at app-closing (like in an object destructor): if Renderer is destroyed first, the app may crash.
	ModelData* getModel(key64 key);

	void setInstances(key64 key, size_t numberOfRenders);
	void setInstances(std::vector<key64>& keys, size_t numberOfRenders);

	void setMaxFPS(int maxFPS);

	Timer& getTimer();		//!< Returns the timer object (provides access to time data).
	size_t getRendersCount(key64 key);
	size_t getFrameCount();
	size_t getFPS();
	size_t getModelsCount();
	size_t getCommandsCount();
	size_t loadedShaders();	//!< Returns number of shaders in Renderer:shaders
	size_t loadedTextures();	//!< Returns number of textures in Renderer:textures
	IOmanager& getIO();			//!< Get access to the IO manager
	int getMaxMemoryAllocationCount();			//!< Max. number of valid memory objects
	int getMemAllocObjects();					//!< Number of memory allocated objects (must be <= maxMemoryAllocationCount)

	void createLightingPass(unsigned numLights, std::string vertShaderPath, std::string fragShaderPath, std::string fragToolsHeader);
	void updateLightingPass(glm::vec3& camPos, Light* lights, unsigned numLights);
	void createPostprocessingPass(std::string vertShaderPath, std::string fragShaderPath);
	void updatePostprocessingPass();
};


#endif