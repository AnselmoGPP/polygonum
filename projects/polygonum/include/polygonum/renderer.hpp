#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "polygonum/environment.hpp"
#include "polygonum/models.hpp"


/// Reponsible for the loading thread and its processes.
class LoadingWorker
{
public:
	LoadingWorker(Renderer* renderer);
	~LoadingWorker();

	enum Task { none, construct, delet };   //!< Used in LoadingWorker::newTask().

	std::mutex mutModels;   //!< for Renderer::models
	std::mutex mutResources;   //!< for Renderer::shaders & Renderer::textures

	std::mutex mutTasks;   //!< for LoadingWorker::tasks
	std::condition_variable cond;   //!< for wake up or sleep the loading thread

	void start();
	void stop();
	void newTask(key64 key, Task task);
	void waitIdle();   //!< Wait for loading thread to be idle
	size_t numTasks();

private:
	Renderer& r;

	std::queue<std::pair<key64, LoadingWorker::Task>> tasks;   //!< FIFO queue
	std::unordered_map<key64, ModelData> modelTP;   //!< Model To Process: A model is moved here temporarily for processing. After processing, it's tranferred to its final destination.

	bool		stopThread;				//!< Signals whether the secondary thread (loadingThread) should be running.
	std::thread	thread_loadModels;		//!< Thread for loading new models. Initiated in the constructor. Finished if glfwWindowShouldClose

	/**
		@brief Load and delete models (including their shaders and textures)

		<ul> Process:
			<li>  Initializes and moves models from modelsToLoad to models </li>
			<li>  Deletes models from modelsToDelete </li>
				<li> Deletes shaders and textures with counter == 0 </li>
		</ul>
	*/
	void thread_loadData(Renderer& renderer, ModelsManager& models, Commander& commander);
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
	const uint32_t ADDITIONAL_SWAPCHAIN_IMAGES = 1;   //!< (1) Total number of swapchain images = swapChain_capabilities_minImageCount + ADDITIONAL_SWAPCHAIN_IMAGES
	const uint32_t MAX_FRAMES_IN_FLIGHT = 10;   //!< (2) How many frames should be processed concurrently.

	friend ResourcesLoader;
	friend LoadingWorker;
	friend ModelData;
	friend UBO;
	friend VertexesLoader;
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

	/// Callback used by the client for updating states of their models.
	void(*userUpdate) (Renderer& rend);

	/// Copy data from UBOs to GPU memory. This is not the most efficient way to pass frequently changing values to the shader. Push constants are more efficient for passing a small buffer of data to shaders.
	void updateUBOs(uint32_t imageIndex);

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