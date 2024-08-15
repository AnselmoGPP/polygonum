#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <optional>					// std::optional<uint32_t> (Wrapper that contains no value until you assign something to it. Contains member has_value())

#include "models.hpp"
#include "input.hpp"
#include "timer.hpp"


// Prototypes ----------

class Renderer;
class LoadingWorker;
enum primitiveTopology;


// Definitions ----------

/// Used for the user to specify what primitive type represents the vertex data. 
enum primitiveTopology {
	point		= VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
	line		= VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
	triangle	= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
};


/// Reponsible for the loading thread and its processes.
class LoadingWorker
{
	std::vector<std::vector<std::list<ModelData>>>& models;
	std::list<ModelData>&	modelsToLoad;
	std::list<ModelData>&	modelsToDelete;
	std::list<Texture>&		textures;
	std::list<Shader>&		shaders;
	bool&					updateCommandBuffer;

	int						waitTime;				//!< Time (milliseconds) the loading-thread wait till next check.
	bool					runThread;				//!< Signals whether the secondary thread (loadingThread) should be running.
	std::thread				thread_loadModels;		//!< Thread for loading new models. Initiated in the constructor. Finished if glfwWindowShouldClose
	std::list<ModelData>	modelTP;				//!< Model To Process: A model is moved here temporarily for processing. After processing, it's tranferred to its final destination.

	/**
		@brief Load and delete models (including their shaders and textures)

		<ul> Process:
			<li>  Initializes and moves models from modelsToLoad to models </li>
			<li>  Deletes models from modelsToDelete </li>
				<li> Deletes shaders and textures with counter == 0 </li>
		</ul>
	*/
	void loadingThread();

public:
	LoadingWorker(int waitTime, std::vector<std::vector<std::list<ModelData>>>& models, std::list<ModelData>& modelsToLoad, std::list<ModelData>& modelsToDelete, std::list<Texture>& textures, std::list<Shader>& shaders, bool& updateCommandBuffer);
	~LoadingWorker();

	std::mutex mutModels;		//!< for Renderer::models
	std::mutex mutLoad;			//!< for Renderer::modelsToLoad
	std::mutex mutDelete;		//!< for Renderer::modelsToDelete
	std::mutex mutResources;	//!< for Renderer::shaders & Renderer::textures

	void start();
	void stop();
	bool isBeingProcessed(modelIter model);
};


// LOOK Restart the Renderer object after finishing the render loop
/**
*   @brief Responsible for making the rendering (render loop). Manages models, textures, input, camera...
* 
*	It creates a VulkanEnvironment and, when the user wants, a ModelData (newModel()).
*/
class Renderer
{
	// Hardcoded parameters
	const int MAX_FRAMES_IN_FLIGHT = 2;		//!< How many frames should be processed concurrently.

	// Main parameters
	VulkanEnvironment			e;
	IOmanager&					io;							//!< Input data
	TimerSet					timer;						//!< Time control

	std::vector<
		std::vector<
			std::list<ModelData>>> models;					//!< Sets of fully initialized models (one set per renderpass per subpass).
	std::list<ModelData>		modelsToLoad;				//!< Models waiting for being included in m (partially initialized).
	std::list<ModelData>		modelsToDelete;				//!< Iterators to the loaded models that have to be deleted from Vulkan.

	std::list<Texture>			textures;					//!< Set of textures
	std::list<Shader>			shaders;					//!< Set of shaders

	LoadingWorker				worker;

	//const size_t				numLayers;					//!< Number of layers (Painter's algorithm)
	std::vector<modelIter>		lastModelsToDraw;			//!< Models that must be moved to the last position in "models" in order to make them be drawn the last.

	// Member variables:
	std::vector<VkCommandBuffer> commandBuffers;			//!< <<< List. Opaque handle to command buffer object. One for each swap chain framebuffer.
	bool updateCommandBuffer;

	std::vector<VkSemaphore>	imageAvailableSemaphores;	//!< Signals that an image has been acquired from the swap chain and is ready for rendering. Each frame has a semaphore for concurrent processing. Allows multiple frames to be in-flight while still bounding the amount of work that piles up. One for each possible frame in flight.
	std::vector<VkSemaphore>	renderFinishedSemaphores;	//!< Signals that rendering has finished (CB has been executed) and presentation can happen. Each frame has a semaphore for concurrent processing. Allows multiple frames to be in-flight while still bounding the amount of work that piles up. One for each possible frame in flight.
	std::vector<VkFence>		framesInFlight;				//!< Similar to semaphores, but fences actually wait in our own code. Used to perform CPU-GPU synchronization. One per frame in flight.
	std::vector<VkFence>		imagesInFlight;				//!< Maps frames in flight by their fences. Tracks for each swap chain image if a frame in flight is currently using it. One per swap chain image.
	VkFence lastFence;										//!< Signals that the last command buffer submitted finished execution.

	size_t						currentFrame;				//!< Frame to process next (0 or 1).
	size_t						commandsCount;				//!< Number of drawing commands sent to the command buffer. For debugging purposes.
	size_t						frameCount;					//!< Number of frames rendered

	modelIter					lightingPass;				//!< Optional (createLighting())
	modelIter					postprocessingPass;			//!< Optional (createPostprocessing())

	// Main methods:

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
	void createCommandBuffers();

	/// Create semaphores and fences for synchronizing the events occuring in each frame (drawFrame()).
	void createSyncObjects();

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
	void drawFrame();

	/// Cleanup after render loop terminates
	void cleanup();

	// Update uniforms, transformation matrices, add/delete new models/textures, and submit command buffer. Transformation matrices (MVP) will be generated each frame.
	void updateStates(uint32_t currentImage);

	/// Callback used by the client for updating states of their models
	void(*userUpdate) (Renderer& rend);

	/// Used in drawFrame(). The window surface may change, making the swap chain no longer compatible with it (example: window resizing). Here, we catch these events (when acquiring/submitting an image from/to the swap chain) and recreate the swap chain.
	void recreateSwapChain();

	/// Used in recreateSwapChain()
	void cleanupSwapChain();	

	/// Used for clearing depth buffer between sets of draw commands in order to apply Painter's algorithm.
	void clearDepthBuffer(VkCommandBuffer commandBuffer);

public:
	// LOOK what if firstModel.size() == 0
	/// Constructor. Requires a callback for updating model matrix, adding models, deleting models, etc.
	Renderer(void(*graphicsUpdate)(Renderer&), IOmanager& io, UBOinfo globalUBO_vs = UBOinfo(), UBOinfo globalUBO_fs = UBOinfo());
	~Renderer();
	
	UBO globalUBO_vs;	//!< Max. number of active instances
	UBO globalUBO_fs;

	void renderLoop();	//!< Create command buffer and start render loop.

	/// Create (partially) a new model in the list modelsToLoad. Used for rendering a model.
	modelIter newModel(ModelDataInfo& modelInfo);

	/// Move model from list models (or modelsToLoad) to list modelsToDelete. If the model is being fully constructed (by the worker), it waits until it finishes. Note: When the app closes, it destroys Renderer. Thus, don't use this method at app-closing (like in an object destructor): if Renderer is destroyed first, the app may crash.
	void deleteModel(modelIter model);

	void setInstances(modelIter model, size_t numberOfRenders);

	/// Make a model the last to be drawn within its own layer. Useful for transparent objects.
	void toLastDraw(modelIter model) { /* <<< NOT WORKING */ };

	void createLightingPass(unsigned numLights, std::string vertShaderPath, std::string fragShaderPath, std::string fragToolsHeader);
	void updateLightingPass(glm::vec3& camPos, Light* lights, unsigned numLights);
	void createPostprocessingPass(std::string vertShaderPath, std::string fragShaderPath);
	void updatePostprocessingPass();

	// Getters
	TimerSet&	getTimer();		//!< Returns the timer object (provides access to time data).
	size_t		getRendersCount(modelIter model);
	size_t		getFrameCount();
	size_t		getModelsCount();
	size_t		getCommandsCount();
	size_t		loadedModels();		//!< Returns number of models in Renderer:models
	size_t		loadedShaders();	//!< Returns number of shaders in Renderer:shaders
	size_t		loadedTextures();	//!< Returns number of textures in Renderer:textures
	IOmanager&  getIO();			//!< Get access to the IO manager
	int			getMaxMemoryAllocationCount();			//!< Max. number of valid memory objects
	int			getMemAllocObjects();					//!< Number of memory allocated objects (must be <= maxMemoryAllocationCount)
};

#endif