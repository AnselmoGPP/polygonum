#include <iostream>

#include "polygonum/renderer.hpp"


void Renderer::recreateSwapChain()
{
#ifdef DEBUG_RENDERER
	std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
#endif
	
	// 1. Get window size.
	int width = 0, height = 0;
	do {
		c.io.waitEvents(); // Wait for an event (resize, focus change, etc.)
		c.io.getFramebufferSize(&width, &height);
	} while (width == 0 || height == 0);

	std::cout << "New window size: " << width << ", " << height << std::endl;

	// 2. Wait for device, graphics queue, and worker to be idle (so we don't touch resources that are in use).
	vkDeviceWaitIdle(c.device);
	c.queueWaitIdle(c.graphicsQueue, &commander.mutQueue);
	worker.waitIdle();

	// 3. Destroy swapchain and related resources.
	commander.freeCommandBuffers();
	models.cleanup_pipelines_and_descriptors(&worker.mutModels);
	rp->destroyRenderPipeline();
	swapChain.destroy();
	
	// 4. Create swapchain and related resources.
	swapChain.createSwapChain();				// Recreate the swap chain.

	rp->createRenderPipeline();

	models.create_pipelines_and_descriptors(&worker.mutModels);
	
	uint32_t frameIndex = commander.getNextFrame();
	commander.createCommandBuffers(swapChain.numImages(), commander.numFrames());   // Command buffers directly depend on the swap chain images.
	commander.imagesInFlight.resize(swapChain.numImages(), { VK_NULL_HANDLE, 0 });
}

Renderer::~Renderer()
{
#ifdef DEBUG_RENDERER
	std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
#endif
}

void Renderer::drawFrame()
{
	/*
		1. Wait for vkQueueSubmit(graphicsQueue) finish commands execution (framesInFlight).
		2. Acquire a swapchain image (vkAcquireNextImageKHR) and signal semaphore (imageAvailable) once it's acquired.
		3. Wait if image is used (imagesInFlight), and mark it as used by this frame (imagesInFlight = framesInFlight).
		4. Update states:
		  4.1. Wait for FPS
		  4.2. User updates
		  4.3. Update UBOs
		  4.4. Update command buffer.
		5. Submit command buffer (vkQueueSubmit(graphicsQueue)) for execution. Synchronizers: fence (framesInFlight), waitSemaphore (imageAvailable), signalSemaphore (renderFinished).
		6. Present image for display on screen (vkQueuePresentKHR(presentQueue)). Synchronizers: waitSemaphore (renderFinished).
	*/

#if defined(DEBUG_RENDERER) || defined(DEBUG_RENDERLOOP)
	std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
#endif

#if defined(DEBUG_REND_PROFILER)
	PRINT("----------\nBegin drawFrame: ", profiler.updateTime() * 1000.f);
#endif

	// 0. Wait until this frame is available to work with.
	size_t frameIndex = commander.getNextFrame();

	const std::lock_guard<std::mutex> lock(commander.mutFrame[frameIndex]);

#if defined(DEBUG_REND_PROFILER)
	PRINT("lock_guard(mutFrame): ", profiler.updateTime() * 1000.f);
#endif

	// 1. Wait for a previous command buffer execution (i.e., the frame to be finished). If VK_TRUE, wait for all fences; otherwise, wait for any.
	vkWaitForFences(c.device, 1, &commander.framesInFlight[frameIndex], VK_TRUE, UINT64_MAX);

#if defined(DEBUG_REND_PROFILER)
	PRINT("vkWaitForFences: ", profiler.updateTime() * 1000.f);
#endif

	// 2. Acquire the next available swapchain image. Semaphore will be signaled once it's acquired.
	uint32_t imageIndex;		// Swap chain image index (0, 1, 2)
	VkResult result = vkAcquireNextImageKHR(c.device, swapChain.swapChain, UINT64_MAX, commander.imageAvailableSemaphores[frameIndex], VK_NULL_HANDLE, &imageIndex);		// Swap chain is an extension feature. imageIndex: index to the VkImage in our swapChainImages.
	if (result == VK_ERROR_OUT_OF_DATE_KHR) 					// VK_ERROR_OUT_OF_DATE_KHR: The swap chain became incompatible with the surface and can no longer be used for rendering. Usually happens after window resize.
	{
		std::cout << "VK_ERROR_OUT_OF_DATE_KHR" << std::endl;
		recreateSwapChain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)	// VK_SUBOPTIMAL_KHR: The swap chain can still be used to successfully present to the surface, but the surface properties are no longer matched exactly.
		throw std::runtime_error("Failed to acquire swap chain image!");

#if defined(DEBUG_REND_PROFILER)
	PRINT("vkAcquireNextImageKHR: ", profiler.updateTime() * 1000.f);
#endif


	// 3. Check if this image is being used. If used, wait. Then, mark it as used by this frame.
	VkFence& imageInFlight = commander.imagesInFlight[imageIndex].first;
	size_t associatedFrameIndex = commander.imagesInFlight[imageIndex].second;
	if (imageInFlight != VK_NULL_HANDLE)   // Check if a previous frame is using this image (i.e. there is its fence to wait on)
	{
		if(associatedFrameIndex != frameIndex)
			const std::lock_guard<std::mutex> lock(commander.mutFrame[associatedFrameIndex]);
		vkWaitForFences(c.device, 1, &commander.imagesInFlight[imageIndex].first, VK_TRUE, UINT64_MAX);
	}
	
	commander.imagesInFlight[imageIndex] = { commander.framesInFlight[frameIndex], frameIndex };   // Mark the image as now being in use by this frame
	
#if defined(DEBUG_REND_PROFILER)
	PRINT("vkWaitForFences: ", profiler.updateTime() * 1000.f);
	PRINT("Update states:");
#endif

	// 4.1. Wait for FPS
	timer.updateTime();
	waitForFPS(timer, maxFPS);
	timer.reUpdateTime();

#if defined(DEBUG_REND_PROFILER)
	PRINT("  waitForFPS: ", profiler.updateTime() * 1000.f);
#endif

	// 4.2. User updates
	userUpdate(*((Renderer*)this));   // Update model matrices and other things (user defined)

#if defined(DEBUG_REND_PROFILER)
	PRINT("  userUpdate: ", profiler.updateTime() * 1000.f);
#endif

	// 4.3. Update UBOs
	updateUBOs(imageIndex);

#if defined(DEBUG_REND_PROFILER)
	PRINT("  Copy UBOs: ", profiler.updateTime() * 1000.f);
#endif

	// 4.4. Update command buffer.
	if (true)//if (updateCommandBuffer)
	{
		vkResetFences(c.device, 1, &commander.framesInFlight[frameIndex]);	// Reset the fence to the unsignaled state.
		commander.updateCommandBuffers(models, rp, swapChain.numImages(), frameIndex);
	}

#if defined(DEBUG_REND_PROFILER)
	PRINT("  Update command buffer: ", profiler.updateTime() * 1000.f);
#endif

	// 5. Submit command buffer to the graphics queue for commands execution (rendering).
	VkSemaphore waitSemaphores[] = { commander.imageAvailableSemaphores[frameIndex] };   // Which semaphores to wait on before command buffers execution begins.
	VkSemaphore signalSemaphores[] = { commander.renderFinishedSemaphores[frameIndex] };   // Which semaphores to signal once the command buffers have finished execution.
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };   // In which stages of the pipeline to wait the semaphore. VK_PIPELINE_STAGE_ ... TOP_OF_PIPE_BIT (ensures that the render passes don't begin until the image is available), COLOR_ATTACHMENT_OUTPUT_BIT (makes the render pass wait for this stage).

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;					// Semaphores upon which to wait before the CB/s begin execution.
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;				// Semaphores to be signaled once the CB/s have completed execution.
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commander.commandBuffers[frameIndex][imageIndex];   // Command buffers to submit for execution (here, the one that binds the swap chain image we just acquired as color attachment).

	//vkResetFences(e.c.device, 1, &framesInFlight[currentFrame]);	// Reset the fence to the unsignaled state.

	{
		const std::lock_guard<std::mutex> lock(commander.mutQueue);
		if (vkQueueSubmit(c.graphicsQueue, 1, &submitInfo, commander.framesInFlight[frameIndex]) != VK_SUCCESS)	// Submit the command buffer to the graphics queue. An array of VkSubmitInfo structs can be taken as argument when workload is much larger, for efficiency.
			throw std::runtime_error("Failed to submit draw command buffer!");
	}

#if defined(DEBUG_REND_PROFILER)
	PRINT("vkQueueSubmit: ", profiler.updateTime() * 1000.f);
#endif

	// Note:
	// Subpass dependencies: Subpasses in a render pass automatically take care of image layout transitions. These transitions are controlled by subpass dependencies (specify memory and execution dependencies between subpasses).
	// There are two built-in dependencies that take care of the transition at the start and at the end of the render pass, but the former does not occur at the right time. It assumes that the transition occurs at the start of the pipeline, but we haven't acquired the image yet at that point. Two ways to deal with this problem:
	//		- waitStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT (ensures that the render passes don't begin until the image is available).
	//		- waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT (makes the render pass wait for this stage).

	// 6. Presentation: Submit the result back to the swap chain to have it eventually show up on the screen.
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	VkSwapchainKHR swapChains[] = { swapChain.swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;			// Optional

	{
		const std::lock_guard<std::mutex> lock(commander.mutQueue);
		result = vkQueuePresentKHR(c.presentQueue, &presentInfo);		// Submit request to present an image to the swap chain. Our triangle may look a bit different because the shader interpolates in linear color space and then converts to sRGB color space.
		renderedFramesCount++;
	}

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || c.io.framebufferResized)
	{
		std::cout << "Out-of-date/Suboptimal KHR or window resized" << std::endl;
		c.io.framebufferResized = false;
		recreateSwapChain();
	}
	else if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to present swap chain image!");

	//vkQueueWaitIdle(e.presentQueue);   // Make the whole graphics pipeline to be used only one frame at a time (instead of using this, we use multiple semaphores for processing frames concurrently).

#if defined(DEBUG_REND_PROFILER)
	PRINT("vkQueuePresentKHR: ", profiler.updateTime() * 1000.f);
#endif
}

void Renderer::renderLoop()
{
#ifdef DEBUG_RENDERER
	std::cout << typeid(*this).name() << "::" << __func__ << " begin" << std::endl;
#endif

	commander.updateCommandBuffers(models, rp, swapChain.numImages(), commander.getNextFrame());
	worker.start();

	timer.startTimer();
	profiler.startTimer();

	while (!c.io.getWindowShouldClose())
	{
#ifdef DEBUG_RENDERLOOP
		std::cout << "Render loop 1/2 ----------" << std::endl;
#endif

		c.io.pollEvents();	// Check for events (processes only those events that have already been received and then returns immediately)
		drawFrame();

		if (c.io.isKeyPressed(GLFW_KEY_ESCAPE))
			c.io.setWindowShouldClose(true);

#ifdef DEBUG_RENDERLOOP
		std::cout << "Render loop 2/2 ----------" << std::endl;
#endif
	}

	worker.stop();

	vkDeviceWaitIdle(c.device);	// Waits for the logical device to finish operations. Needed for cleaning up once drawing and presentation operations (drawFrame) have finished. Use vkQueueWaitIdle for waiting for operations in a specific command queue to be finished.

	cleanup();

#ifdef DEBUG_RENDERER
	std::cout << typeid(*this).name() << "::" << __func__ << " end" << std::endl;
#endif
}


void Renderer::cleanup()
{
#ifdef DEBUG_RENDERER
	std::cout << typeid(*this).name() << "::" << __func__ << " (1/2)" << std::endl;
#endif

	c.queueWaitIdle(c.graphicsQueue, &commander.mutQueue);

	models.data.clear();   // lock_guard (worker.mutModels) not necessary before this because worker stopped the loading thread.

	if (globalBinding_vs.totalBytes) globalBinding_vs.destroyBinding();
	if (globalBinding_fs.totalBytes) globalBinding_fs.destroyBinding();

	commander.freeCommandBuffers();
	commander.destroySynchronizers();
	commander.destroyCommandPool();

	rp->destroyRenderPipeline();

	swapChain.destroy();

	c.destroy();

#ifdef DEBUG_RENDERER
	std::cout << typeid(*this).name() << "::" << __func__ << " (2/2)" << std::endl;
#endif
}

key64 Renderer::newModel(ModelDataInfo& modelInfo)
{
#ifdef DEBUG_RENDERER
	std::cout << typeid(*this).name() << "::" << __func__ << ": " << modelInfo.name << std::endl;
#endif
	
	if (modelInfo.renderPassIndex < models.keys.size() && modelInfo.subpassIndex < models.keys[modelInfo.renderPassIndex].size())
	{
		std::pair<std::unordered_map<key64, ModelData>::iterator, bool> result =
			models.data.emplace(std::make_pair(models.getNewKey(), ModelData(this, modelInfo)));   // Save model object into model list
		
		worker.newTask(result.first->first, LoadingWorker::construct);   // Schedule task: Construct model

		return result.first->first;
	}

	std::cout << "The renderpass/subpass specified for this model (" << modelInfo.name << ": " << modelInfo.renderPassIndex << '/' << modelInfo.subpassIndex << ") doesn't fit the render pipeline" << std::endl;
	return 0;
}

void Renderer::deleteModel(key64 key)	// <<< splice an element only knowing the iterator (no need to check lists)?
{
#ifdef DEBUG_RENDERER
	std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
#endif

	worker.newTask(key, LoadingWorker::delet);
}

ModelData* Renderer::getModel(key64 key)
{
	if (models.data.find(key) != models.data.end())
		return &models.data[key];
	else
		return nullptr;
}

void Renderer::setInstances(key64 key, size_t numberOfRenders)
{
#ifdef DEBUG_RENDERER
	std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
#endif

	const std::lock_guard<std::mutex> lock(worker.mutModels);

	if (models.data.find(key) != models.data.end())
		if (models.data[key].setActiveInstancesCount(numberOfRenders))
			commander.updateCommandBuffer = true;		// We flag commandBuffer for update assuming that our model is in list "model"
}

void Renderer::setInstances(std::vector<key64>& keys, size_t numberOfRenders)
{
#ifdef DEBUG_RENDERER
	std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
#endif

	const std::lock_guard<std::mutex> lock(worker.mutModels);

	for (key64 key : keys)
		if (models.data.find(key) != models.data.end())
			if (models.data[key].setActiveInstancesCount(numberOfRenders))
				commander.updateCommandBuffer = true;		// We flag commandBuffer for update assuming that our model is in list "model"
}

void Renderer::setMaxFPS(int maxFPS)
{
	if (maxFPS > 0)
		this->maxFPS = maxFPS;
	else
		this->maxFPS = 0;
}

void Renderer::updateUBOs(uint32_t imageIndex)
{
#ifdef DEBUG_RENDERLOOP
	std::cout << "Copy UBOs" << std::endl;
#endif

	void* data;
	size_t activeBytes;
	ModelData* model;

	// Global UBOs
	if (globalBinding_vs.totalBytes)
	{
		vkMapMemory(c.device, globalBinding_vs.bindingMemories[imageIndex], 0, globalBinding_vs.totalBytes, 0, &data);
		memcpy(data, globalBinding_vs.binding.data(), globalBinding_vs.totalBytes);
		vkUnmapMemory(c.device, globalBinding_vs.bindingMemories[imageIndex]);
	}

	if (globalBinding_fs.totalBytes)
	{
		vkMapMemory(c.device, globalBinding_fs.bindingMemories[imageIndex], 0, globalBinding_fs.totalBytes, 0, &data);
		memcpy(data, globalBinding_fs.binding.data(), globalBinding_fs.totalBytes);
		vkUnmapMemory(c.device, globalBinding_fs.bindingMemories[imageIndex]);
	}

	// Local UBOs
	const std::lock_guard<std::mutex> lock(worker.mutModels);

	models.distributeKeys();

	for (auto it = models.data.begin(); it != models.data.end(); it++)
		if (it->second.ready)
		{
			model = &it->second;

			activeBytes = model->vsUBOs.numActiveUbos * model->vsUBOs.uboSize;
			if (activeBytes)
			{
				vkMapMemory(c.device, model->vsUBOs.bindingMemories[imageIndex], 0, activeBytes, 0, &data);	// Get a pointer to some Vulkan/GPU memory of size X. vkMapMemory retrieves a host virtual address pointer (data) to a region of a mappable memory object (uniformBuffersMemory[]). We have to provide the logical device that owns the memory (e.device).
				memcpy(data, model->vsUBOs.binding.data(), activeBytes);											// Copy some data in that memory. Copies a number of bytes (sizeof(ubo)) from a source (ubo) to a destination (data).
				vkUnmapMemory(c.device, model->vsUBOs.bindingMemories[imageIndex]);							// "Get rid" of the pointer. Unmap a previously mapped memory object (uniformBuffersMemory[]).
			}

			activeBytes = model->fsUBOs.numActiveUbos * model->fsUBOs.uboSize;
			if (activeBytes)
			{
				vkMapMemory(c.device, model->fsUBOs.bindingMemories[imageIndex], 0, activeBytes, 0, &data);
				memcpy(data, model->fsUBOs.binding.data(), activeBytes);
				vkUnmapMemory(c.device, model->fsUBOs.bindingMemories[imageIndex]);
			}
		}
}

long double Renderer::getDeltaTime() const { return timer.getDeltaTime(); }

size_t Renderer::getFrameCount() { return renderedFramesCount; }

size_t Renderer::getFPS() { return static_cast<size_t>(std::round(1 / timer.getDeltaTime())); }

size_t Renderer::getModelsCount() { return models.data.size(); }

size_t Renderer::getCommandsCount() { return commander.commandsCount; }

size_t Renderer::loadedShaders() { return shaders.size(); }

size_t Renderer::loadedTextures() { return textures.size(); }

IOmanager& Renderer::getIO() { return c.io; }

int Renderer::getMaxMemoryAllocationCount() { return c.deviceData.maxMemoryAllocationCount; }

int Renderer::getMemAllocObjects() { return c.memAllocObjects; }

LoadingWorker::LoadingWorker(Renderer* renderer)
	: r(*renderer), stopThread(false) { }

LoadingWorker::~LoadingWorker()
{
#ifdef DEBUG_WORKER
	std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
#endif
}

void LoadingWorker::start()
{
	stopThread = false;
	cond.notify_one();
	thread_loadModels = std::thread(&LoadingWorker::thread_loadData, this, std::ref(r), std::ref(r.models), std::ref(r.commander));
}

void LoadingWorker::stop()
{
#ifdef DEBUG_RENDERER
	std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
#endif

	stopThread = true;
	cond.notify_one();

	if (thread_loadModels.joinable())
		thread_loadModels.join();
}

void LoadingWorker::newTask(key64 key, Task task)
{
	std::lock_guard lock(mutTasks);
	tasks.push(std::pair(key, task));
	cond.notify_one();   // Wake up the loading thread
}

void LoadingWorker::waitIdle()
{
	std::mutex mutWait;
	std::unique_lock lock(mutWait);
	cond.wait(lock, [this] { return tasks.empty(); });
}

size_t LoadingWorker::numTasks() { return tasks.size(); }

void LoadingWorker::extractModel(ModelsManager& models, key64 key)
{
	const std::lock_guard<std::mutex> lock(mutModels);

	models.data[key].ready = false;

	auto node = models.data.extract(key);   // auto = std::unordered_map<key64, ModelData>::node_type
	if (node.empty() == false)
		modelTP.insert(std::move(node));
}

void LoadingWorker::returnModel(ModelsManager& models, key64 key)
{
	const std::lock_guard<std::mutex> lock(mutModels);

	auto node = modelTP.extract(key);   // auto = std::unordered_map<key64, ModelData>::node_type
	if (node.empty() == false)
		models.data.insert(std::move(node));

	if (models.data[key].fullyConstructed)
		models.data[key].ready = true;
}

void LoadingWorker::thread_loadData(Renderer& renderer, ModelsManager& models, Commander& commander)
{
#ifdef DEBUG_WORKER
	std::cout << "- " << typeid(*this).name() << "::" << __func__ << " (begin)" << std::endl;
	std::cout << "- Loading thread ID: " << std::this_thread::get_id() << std::endl;
#endif

	key64 key;
	Task task;

	for(;;)
	{
#ifdef DEBUG_WORKER
		std::cout << "- New iteration -----" << std::endl;
#endif

		std::unique_lock lock(mutTasks);
		cond.wait(lock, [this] { return (!tasks.empty() || stopThread); });   // Wait for new tasks or a stop order.
		
		if (tasks.empty() && stopThread) return;   // Stop order executed here

		key = tasks.front().first;   // Get task info
		task = tasks.front().second;
		tasks.pop();

		lock.unlock();
		
		// Complete task
		switch (task)
		{
		case construct:
			models.data[key].fullConstruction(renderer);
			models.data[key].ready = true;
			commander.updateCommandBuffer = true;
			break;

		case delet:
			extractModel(models, key);
			modelTP.clear();
			commander.updateCommandBuffer = true;
			break;

		default:
			break;
		}
	}

#ifdef DEBUG_WORKER
	std::cout << "- " << typeid(*this).name() << "::" << __func__ << " (end)" << std::endl;
#endif
}

Help_RP_DS_PP::Help_RP_DS_PP() : lightingPass(0), postprocessingPass(0) { }

void Help_RP_DS_PP::createLightingPass(Renderer& ren, unsigned numLights, std::string vertShaderPath, std::string fragShaderPath, std::string fragToolsHeader)
{
	std::vector<float> v_quad;	// [4 * 5]
	std::vector<uint16_t> i_quad;
	getScreenQuad(v_quad, i_quad);	// <<< The parameter zValue doesn't represent height (otherwise, this value should serve for hiding one plane behind another).

	std::vector<ShaderLoader*> usedShaders{
		SL_fromFile::factory(vertShaderPath),
		SL_fromFile::factory(fragShaderPath, { SMod::changeHeader(fragToolsHeader) })
	};

	VertexType vertexType({ vaPos, vaUv });

	std::vector<TextureLoader*> usedTextures{ };

	ModelDataInfo modelInfo;
	modelInfo.name = "lightingPass";
	modelInfo.activeInstances = 1;
	modelInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	modelInfo.vertexType = vertexType;
	modelInfo.vertexesLoader = VL_fromBuffer::factory(v_quad.data(), vertexType.vertexSize, 4, i_quad, {});
	modelInfo.shadersInfo = usedShaders;
	modelInfo.texturesInfo = usedTextures;
	modelInfo.maxNumUbos_vs = 0;
	modelInfo.maxNumUbos_fs = 1;
	modelInfo.uboSize_vs = 0;
	modelInfo.uboSize_fs = sizes::vec4 + numLights * sizeof(Light);	// camPos,  n * LightPosDir (2*vec4),  n * LightProps (6*vec4);
	modelInfo.globalBinding_vs;
	modelInfo.globalBinding_fs;
	modelInfo.transparency = false;
	modelInfo.renderPassIndex = 1;
	modelInfo.subpassIndex = 0;

	lightingPass = ren.newModel(modelInfo);
}

void Help_RP_DS_PP::createPostprocessingPass(Renderer& ren, std::string vertShaderPath, std::string fragShaderPath)
{
	std::vector<float> v_quad;	// [4 * 5]
	std::vector<uint16_t> i_quad;
	getScreenQuad(v_quad, i_quad);	// <<< The parameter zValue doesn't represent heigth (otherwise, this value should serve for hiding one plane behind another).

	std::vector<ShaderLoader*> usedShaders{
		SL_fromFile::factory(vertShaderPath),
		SL_fromFile::factory(fragShaderPath)
	};

	VertexType vertexType({ vaPos, vaUv });

	std::vector<TextureLoader*> usedTextures{ };

	ModelDataInfo modelInfo;
	modelInfo.name = "postprocessingPass";
	modelInfo.activeInstances = 1;
	modelInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	modelInfo.vertexType = vertexType;
	modelInfo.vertexesLoader = VL_fromBuffer::factory(v_quad.data(), vertexType.vertexSize, 4, i_quad, {});
	modelInfo.shadersInfo = usedShaders;
	modelInfo.texturesInfo = usedTextures;
	modelInfo.maxNumUbos_vs = 0;
	modelInfo.maxNumUbos_fs = 0;
	modelInfo.uboSize_vs = 0;
	modelInfo.uboSize_fs = 0;
	modelInfo.globalBinding_vs;
	modelInfo.globalBinding_fs;
	modelInfo.transparency = false;
	modelInfo.renderPassIndex = 3;
	modelInfo.subpassIndex = 0;

	postprocessingPass = ren.newModel(modelInfo);
}

void Help_RP_DS_PP::updateLightingPass(Renderer& ren, glm::vec3& camPos, Light* lights, unsigned numLights)
{
	//if (models.data.find(lightingPass) == models.data.end()) return;
	ModelData* model = ren.getModel(lightingPass);
	if (!model) return;

	uint8_t* dest;

	//for (int i = 0; i < model->vsUBOs.numActiveUbos; i++)
	//{
	//	dest = model->vsUBOs.getUboPtr(i);
	//	//...
	//}

	for (uint32_t i = 0; i < model->fsUBOs.numActiveUbos; i++)
	{
		dest = model->fsUBOs.getUboPtr(i);
		memcpy(dest, &camPos, sizes::vec4);
		dest += sizes::vec4;
		memcpy(dest, lights, numLights * sizeof(Light));
	}
}

void Help_RP_DS_PP::updatePostprocessingPass(Renderer& ren)
{
	// No code necessary here
}
