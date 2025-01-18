#include <iostream>
#include <stdexcept>
#include <array>

#include "polygonum/renderer.hpp"
#include "polygonum/input.hpp"
#include "polygonum/toolkit.hpp"


// LoadingWorker ---------------------------------------------------------------------

LoadingWorker::LoadingWorker(int waitTime, Renderer& rend)
	: rend(rend), waitTime(waitTime), runThread(false) { }

LoadingWorker::~LoadingWorker() 
{ 
	#ifdef DEBUG_WORKER
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif
}

void LoadingWorker::start() 
{ 
	runThread = true;
	thread_loadModels = std::thread(&LoadingWorker::loadingThread, this);
}

void LoadingWorker::stop() 
{
	#ifdef DEBUG_RENDERER
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	runThread = false;
	if (thread_loadModels.joinable()) thread_loadModels.join();
}

void LoadingWorker::newTask(key64 key, Task task)
{
	tasks.push(std::pair(key, task));
}

void LoadingWorker::extractModel(key64 key)
{
	const std::lock_guard<std::mutex> lock(mutModels);
	
	rend.models[key].ready = false;

	auto node = rend.models.extract(key);   // auto = std::unordered_map<key64, ModelData>::node_type
	if (node.empty() == false)
		modelTP.insert(std::move(node));
}

void LoadingWorker::returnModel(key64 key)
{
	const std::lock_guard<std::mutex> lock(mutModels);

	auto node = modelTP.extract(key);   // auto = std::unordered_map<key64, ModelData>::node_type
	if (node.empty() == false)
		rend.models.insert(std::move(node));

	if(rend.models[key].fullyConstructed)
		rend.models[key].ready = true;
}

void LoadingWorker::loadingThread()
{
	#ifdef DEBUG_WORKER
		std::cout << "- " << typeid(*this).name() << "::" << __func__ << " (begin)" << std::endl;
		std::cout << "- Loading thread ID: " << std::this_thread::get_id() << std::endl;
	#endif

	key64 key;
	Task task;

	while (runThread)
	{
		#ifdef DEBUG_WORKER
			std::cout << "- New iteration -----" << std::endl;
		#endif

		if (tasks.size())
		{
			key = tasks.front().first;
			task = tasks.front().second;
			tasks.pop();

			switch (task)
			{
			case construct:
				//extractModel(key);
				//modelTP.begin()->second.fullConstruction(shaders, textures, mutResources);
				//returnModel(key);
				//updateCommandBuffer = true;
				rend.models[key].fullConstruction(rend);
				rend.models[key].ready = true;
				rend.updateCommandBuffer = true;
				break;

			case delet:
				extractModel(key);
				modelTP.clear();
				rend.updateCommandBuffer = true;
				break;

			default:
				break;
			}
		}
		else sleep(waitTime);
	}

	#ifdef DEBUG_WORKER
		std::cout << "- " << typeid(*this).name() << "::" << __func__ << " (end)" << std::endl;
	#endif
}


// Renderer ---------------------------------------------------------------------

Renderer::Renderer(void(*graphicsUpdate)(Renderer&), int width, int height, UBOinfo globalUBO_vs, UBOinfo globalUBO_fs)
	: io(width, height),
	e(io),
	updateCommandBuffer(false), 
	userUpdate(graphicsUpdate), 
	currentFrame(0), 
	commandsCount(0),
	frameCount(0),
	maxFPS(30),
	globalUBO_vs(&e, globalUBO_vs),
	globalUBO_fs(&e, globalUBO_fs),
	worker(500, *this),
	newKey(0)
{ 
	#ifdef DEBUG_RENDERER
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
		std::cout << "Main thread ID: " << std::this_thread::get_id() << std::endl;
		std::cout << "   Hardware concurrency: " << (unsigned int)std::thread::hardware_concurrency << std::endl;
	#endif

	// Resize list "keys"
	keys.resize(e.rp->renderPasses.size());
	for (size_t rp = 0; rp < keys.size(); rp++)
		keys[rp].resize(e.rp->renderPasses[rp].subpasses.size());

	// Create UBOs
	if (this->globalUBO_vs.totalBytes) this->globalUBO_vs.createUBO();
	if (this->globalUBO_fs.totalBytes) this->globalUBO_fs.createUBO();
}

Renderer::~Renderer() 
{ 
	#ifdef DEBUG_RENDERER
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif
}

/*
	-Wait for the frame's CB execution (inFlightFences)
	vkAcquireNextImageKHR (acquire swap chain image)
	-Wait for the swap chain image's CB execution (imagesInFlight/inFlightFences)
	Update CB (optional)
	-Wait for acquiring a swap chain image (imageAvailableSemaphores)
	vkQueueSubmit (execute CB)
	-Wait for CB execution (renderFinishedSemaphores)
	vkQueuePresentKHR (present image)

	waitFor(framesInFlight[currentFrame]);
	vkAcquireNextImageKHR(imageAvailableSemaphores[currentFrame], imageIndex);
	waitFor(imagesInFlight[imageIndex]);
	imagesInFlight[imageIndex] = framesInFlight[currentFrame];
	updateCB();
	vkResetFences(framesInFlight[currentFrame])
	vkQueueSubmit(renderFinishedSemaphores[currentFrame], framesInFlight[currentFrame]); // waitFor(imageAvailableSemaphores[currentFrame])
	vkQueuePresentKHR(); // waitFor(renderFinishedSemaphores[currentFrame]);
	currentFrame = nextFrame;

	1. Wait for command buffer execution
	2. Acquire image from swapchain
	3. Submit command buffer
	4. Present result to swapchain
*/
void Renderer::drawFrame()
{
#if defined(DEBUG_RENDERER) || defined(DEBUG_RENDERLOOP)
	std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
#endif

#if defined(DEBUG_REND_PROFILER)
	PRINT("> Begin drawFrame: ", profiler.updateTime() * 1000.f);
#endif

	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;	// By using the modulo operator (%), the frame index loops around after every MAX_FRAMES_IN_FLIGHT enqueued frames.

	// Wait for the frame to be finished (command buffer execution). If VK_TRUE, we wait for all fences; otherwise, we wait for any.
	vkWaitForFences(e.c.device, 1, &framesInFlight[currentFrame], VK_TRUE, UINT64_MAX);

#if defined(DEBUG_REND_PROFILER)
	PRINT("vkWaitForFences: ", profiler.updateTime() * 1000.f);
#endif

	// Acquire an image from the swap chain
	uint32_t imageIndex;		// Swap chain image index (0, 1, 2)
	VkResult result = vkAcquireNextImageKHR(e.c.device, e.swapChain.swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);		// Swap chain is an extension feature. imageIndex: index to the VkImage in our swapChainImages.
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

	// Check if this image is being used. If used, wait. Then, mark it as used by this frame.
	if (imagesInFlight[imageIndex] != VK_NULL_HANDLE)									// Check if a previous frame is using this image (i.e. there is its fence to wait on)
		vkWaitForFences(e.c.device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);

	imagesInFlight[imageIndex] = framesInFlight[currentFrame];							// Mark the image as now being in use by this frame

#if defined(DEBUG_REND_PROFILER)
	PRINT("vkWaitForFences: ", profiler.updateTime() * 1000.f);
	PRINT("updateStates:");
#endif

	updateStates(imageIndex);

#if defined(DEBUG_REND_PROFILER)
	PRINT("  Copy UBOs: ", profiler.updateTime() * 1000.f);
#endif

	// Update command buffer
	if (true)//if (updateCommandBuffer)
	{
		profiler.updateTime();
		vkWaitForFences(e.c.device, 1, &lastFence, VK_TRUE, UINT64_MAX);
		vkResetFences(e.c.device, 1, &framesInFlight[currentFrame]);	// Reset the fence to the unsignaled state.
		PRINT(profiler.updateTime() * 1000.f);

		const std::lock_guard<std::mutex> lock(e.mutCommandPool);		// vkQueueWaitIdle(e.c.graphicsQueue) was called before, in drawFrame()
		vkFreeCommandBuffers(e.c.device, e.commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());	// Any primary command buffer that is in the recording or executable state and has any element of pCommandBuffers recorded into it, becomes invalid.
		//vkResetCommandPool(e.c.device, e.commandPool, 0);
		//vkResetCommandBuffer(commandBuffers[currentFrame], 0);
		createCommandBuffers();
	}
	
#if defined(DEBUG_REND_PROFILER)
	PRINT("Update command buffer: ", profiler.updateTime() * 1000.f);
#endif

	// Submit the command buffer
	VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };			// Which semaphores to wait on before command buffers execution begins.
	VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };		// Which semaphores to signal once the command buffers have finished execution.
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };	// In which stages of the pipeline to wait the semaphore. VK_PIPELINE_STAGE_ ... TOP_OF_PIPE_BIT (ensures that the render passes don't begin until the image is available), COLOR_ATTACHMENT_OUTPUT_BIT (makes the render pass wait for this stage).

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;					// Semaphores upon which to wait before the CB/s begin execution.
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;				// Semaphores to be signaled once the CB/s have completed execution.
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];		// Command buffers to submit for execution (here, the one that binds the swap chain image we just acquired as color attachment).

	//vkResetFences(e.c.device, 1, &framesInFlight[currentFrame]);	// Reset the fence to the unsignaled state.

	{
		const std::lock_guard<std::mutex> lock(e.mutQueue);
		if (vkQueueSubmit(e.c.graphicsQueue, 1, &submitInfo, framesInFlight[currentFrame]) != VK_SUCCESS)	// Submit the command buffer to the graphics queue. An array of VkSubmitInfo structs can be taken as argument when workload is much larger, for efficiency.
			throw std::runtime_error("Failed to submit draw command buffer!");

		lastFence = framesInFlight[currentFrame];
	}

#if defined(DEBUG_REND_PROFILER)
	PRINT("vkQueueSubmit: ", profiler.updateTime() * 1000.f);
#endif

	// Note:
	// Subpass dependencies: Subpasses in a render pass automatically take care of image layout transitions. These transitions are controlled by subpass dependencies (specify memory and execution dependencies between subpasses).
	// There are two built-in dependencies that take care of the transition at the start and at the end of the render pass, but the former does not occur at the right time. It assumes that the transition occurs at the start of the pipeline, but we haven't acquired the image yet at that point. Two ways to deal with this problem:
	//		- waitStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT (ensures that the render passes don't begin until the image is available).
	//		- waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT (makes the render pass wait for this stage).

	// Presentation (submit the result back to the swap chain to have it eventually show up on the screen).
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	VkSwapchainKHR swapChains[] = { e.swapChain.swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;			// Optional

	{
		const std::lock_guard<std::mutex> lock(e.mutQueue);
		result = vkQueuePresentKHR(e.c.presentQueue, &presentInfo);		// Submit request to present an image to the swap chain. Our triangle may look a bit different because the shader interpolates in linear color space and then converts to sRGB color space.
		frameCount++;
	}

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || io.framebufferResized)
	{
		std::cout << "Out-of-date/Suboptimal KHR or window resized" << std::endl;
		io.framebufferResized = false;
		recreateSwapChain();
	}
	else if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to present swap chain image!");

	//vkQueueWaitIdle(e.presentQueue);							// Make the whole graphics pipeline to be used only one frame at a time (instead of using this, we use multiple semaphores for processing frames concurrently).

#if defined(DEBUG_REND_PROFILER)
	PRINT("vkQueuePresentKHR: ", profiler.updateTime() * 1000.f);
#endif
}

void Renderer::drawFrame0()
{
#if defined(DEBUG_RENDERER) || defined(DEBUG_RENDERLOOP)
	std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
#endif

#if defined(DEBUG_REND_PROFILER)
	PRINT("> Begin drawFrame: ", profiler.updateTime() * 1000.f);
#endif

	// Wait for the frame to be finished (command buffer execution). If VK_TRUE, we wait for all fences; otherwise, we wait for any.
	vkWaitForFences(e.c.device, 1, &framesInFlight[currentFrame], VK_TRUE, UINT64_MAX);

#if defined(DEBUG_REND_PROFILER)
	PRINT("vkWaitForFences: ", profiler.updateTime() * 1000.f);
#endif

	// Acquire an image from the swap chain
	uint32_t imageIndex;		// Swap chain image index (0, 1, 2)
	VkResult result = vkAcquireNextImageKHR(e.c.device, e.swapChain.swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);		// Swap chain is an extension feature. imageIndex: index to the VkImage in our swapChainImages.
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

	// Check if this image is being used. If used, wait. Then, mark it as used by this frame.
	if (imagesInFlight[imageIndex] != VK_NULL_HANDLE)									// Check if a previous frame is using this image (i.e. there is its fence to wait on)
		vkWaitForFences(e.c.device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);

	imagesInFlight[imageIndex] = framesInFlight[currentFrame];							// Mark the image as now being in use by this frame

#if defined(DEBUG_REND_PROFILER)
	PRINT("vkWaitForFences: ", profiler.updateTime() * 1000.f);
	PRINT("updateStates:");
#endif

	updateStates(imageIndex);

#if defined(DEBUG_REND_PROFILER)
	PRINT("  Update command buffer: ", profiler.updateTime() * 1000.f);
#endif

	// Submit the command buffer
	VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };			// Which semaphores to wait on before command buffers execution begins.
	VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };		// Which semaphores to signal once the command buffers have finished execution.
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };	// In which stages of the pipeline to wait the semaphore. VK_PIPELINE_STAGE_ ... TOP_OF_PIPE_BIT (ensures that the render passes don't begin until the image is available), COLOR_ATTACHMENT_OUTPUT_BIT (makes the render pass wait for this stage).

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;					// Semaphores upon which to wait before the CB/s begin execution.
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;				// Semaphores to be signaled once the CB/s have completed execution.
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];		// Command buffers to submit for execution (here, the one that binds the swap chain image we just acquired as color attachment).

	vkResetFences(e.c.device, 1, &framesInFlight[currentFrame]);	// Reset the fence to the unsignaled state.

	{
		const std::lock_guard<std::mutex> lock(e.mutQueue);
		if (vkQueueSubmit(e.c.graphicsQueue, 1, &submitInfo, framesInFlight[currentFrame]) != VK_SUCCESS)	// Submit the command buffer to the graphics queue. An array of VkSubmitInfo structs can be taken as argument when workload is much larger, for efficiency.
			throw std::runtime_error("Failed to submit draw command buffer!");

		lastFence = framesInFlight[currentFrame];
	}

#if defined(DEBUG_REND_PROFILER)
	PRINT("vkQueueSubmit: ", profiler.updateTime() * 1000.f);
#endif

	// Note:
	// Subpass dependencies: Subpasses in a render pass automatically take care of image layout transitions. These transitions are controlled by subpass dependencies (specify memory and execution dependencies between subpasses).
	// There are two built-in dependencies that take care of the transition at the start and at the end of the render pass, but the former does not occur at the right time. It assumes that the transition occurs at the start of the pipeline, but we haven't acquired the image yet at that point. Two ways to deal with this problem:
	//		- waitStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT (ensures that the render passes don't begin until the image is available).
	//		- waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT (makes the render pass wait for this stage).

	// Presentation (submit the result back to the swap chain to have it eventually show up on the screen).
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	VkSwapchainKHR swapChains[] = { e.swapChain.swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;			// Optional

	{
		const std::lock_guard<std::mutex> lock(e.mutQueue);
		result = vkQueuePresentKHR(e.c.presentQueue, &presentInfo);		// Submit request to present an image to the swap chain. Our triangle may look a bit different because the shader interpolates in linear color space and then converts to sRGB color space.
		frameCount++;
	}

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || io.framebufferResized)
	{
		std::cout << "Out-of-date/Suboptimal KHR or window resized" << std::endl;
		io.framebufferResized = false;
		recreateSwapChain();
	}
	else if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to present swap chain image!");

	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;	// By using the modulo operator (%), the frame index loops around after every MAX_FRAMES_IN_FLIGHT enqueued frames.

	//vkQueueWaitIdle(e.presentQueue);							// Make the whole graphics pipeline to be used only one frame at a time (instead of using this, we use multiple semaphores for processing frames concurrently).

#if defined(DEBUG_REND_PROFILER)
	PRINT("vkQueuePresentKHR: ", profiler.updateTime() * 1000.f);
#endif
}

// (24)
void Renderer::createCommandBuffers()
{
	#if defined(DEBUG_RENDERER) || defined(DEBUG_COMMANDBUFFERS)
		std::cout << typeid(*this).name() << "::" << __func__ << " BEGIN" << std::endl;
	#endif
	
	commandsCount = 0;
	VkDeviceSize offsets[] = { 0 };

	// Commmand buffer allocation
	commandBuffers.resize(e.swapChain.images.size());

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = e.commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;		// VK_COMMAND_BUFFER_LEVEL_ ... PRIMARY (can be submitted to a queue for execution, but cannot be called from other command buffers), SECONDARY (cannot be submitted directly, but can be called from primary command buffers - useful for reusing common operations from primary command buffers).
	allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();		// Number of buffers to allocate.
	
	//const std::lock_guard<std::mutex> lock(e.mutCommandPool);	// already called before calling createCommandBuffers() 

	if (vkAllocateCommandBuffers(e.c.device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate command buffers!");

	// Start command buffer recording (one per swapChainImage)
	ModelData* model;

	for (size_t i = 0; i < commandBuffers.size(); i++)		// for each SWAPCHAIN IMAGE
	{
		#ifdef DEBUG_COMMANDBUFFERS
			std::cout << "Command buffer " << i << std::endl;
		#endif

		// Start command buffer recording
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;			// [Optional] VK_COMMAND_BUFFER_USAGE_ ... ONE_TIME_SUBMIT_BIT (the command buffer will be rerecorded right after executing it once), RENDER_PASS_CONTINUE_BIT (secondary command buffer that will be entirely within a single render pass), SIMULTANEOUS_USE_BIT (the command buffer can be resubmitted while it is also already pending execution).
		beginInfo.pInheritanceInfo = nullptr;		// [Optional] Only relevant for secondary command buffers. It specifies which state to inherit from the calling primary command buffers.

		if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS)		// If a command buffer was already recorded once, this call resets it. It's not possible to append commands to a buffer at a later time.
			throw std::runtime_error("Failed to begin recording command buffer!");

		for (size_t rp = 0; rp < keys.size(); rp++)		// for each RENDER PASS (color pass, post-processing...)
		{
			#ifdef DEBUG_COMMANDBUFFERS
				std::cout << "   Render pass " << rp << std::endl;
			#endif
			
			vkCmdBeginRenderPass(commandBuffers[i], &e.rp->renderPasses[rp].renderPassInfos[i], VK_SUBPASS_CONTENTS_INLINE);	// Start RENDER PASS. VK_SUBPASS_CONTENTS_INLINE (the render pass commands will be embedded in the primary command buffer itself and no secondary command buffers will be executed), VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS (the render pass commands will be executed from secondary command buffers).
			//vkCmdNextSubpass(commandBuffers[i], VK_SUBPASS_CONTENTS_INLINE);						// Start SUBPASS
			
			for (size_t sp = 0; sp < keys[rp].size(); sp++)		// for each SUB-PASS
			{
				if(sp > 0) vkCmdNextSubpass(commandBuffers[i], VK_SUBPASS_CONTENTS_INLINE);			// Start SUBPASS
				//clearDepthBuffer(commandBuffers[i]);		// Already done in createRenderPass() (loadOp). Previously used for implementing layers (Painter's algorithm).

				for (key64 key : keys[rp][sp])		// for each MODEL
				{
					#ifdef DEBUG_COMMANDBUFFERS
						std::cout << "         Model: " << it->name << std::endl;
					#endif
					
					model = &models[key];
					if (model->getActiveInstancesCount() == 0) continue;

					vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, model->graphicsPipeline);	// Second parameter: Specifies if the pipeline object is a graphics or compute pipeline.
					vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, &model->vert.vertexBuffer, offsets);

					if (model->vert.indexCount)		// has indices (it doesn't if data represents points)
						vkCmdBindIndexBuffer(commandBuffers[i], model->vert.indexBuffer, 0, VK_INDEX_TYPE_UINT16);

					if (model->descriptorSets.size())	// has descriptor set (UBOs, textures, input attachments)
						vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, model->pipelineLayout, 0, 1, &model->descriptorSets[i], 0, 0);
					
					if (model->vert.indexCount)		// has indices
						vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(model->vert.indexCount), model->getActiveInstancesCount(), 0, 0, 0);
					else
						vkCmdDraw(commandBuffers[i], model->vert.vertexCount, model->getActiveInstancesCount(), 0, 0);

					commandsCount++;
				}
			}

			vkCmdEndRenderPass(commandBuffers[i]);
		}
		
		if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS)
			throw std::runtime_error("Failed to record command buffer!");
	}

	updateCommandBuffer = false;

	#if defined(DEBUG_RENDERER) || defined(DEBUG_COMMANDBUFFERS)
		std::cout << typeid(*this).name() << "::" << __func__ << " END" << std::endl;
	#endif
}

// (25)
void Renderer::createSyncObjects()
{
	#ifdef DEBUG_RENDERER
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	framesInFlight.resize(MAX_FRAMES_IN_FLIGHT);
	imagesInFlight.resize(e.swapChain.images.size(), VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;						// Reset to signaled state (CB finished execution)

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(e.c.device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(e.c.device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(e.c.device, &fenceInfo, nullptr, &framesInFlight[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create synchronization objects for a frame!");
		}
	}

	lastFence = framesInFlight[currentFrame];
}

void Renderer::renderLoop()
{
	#ifdef DEBUG_RENDERER
		std::cout << typeid(*this).name() << "::" << __func__ << " begin" << std::endl;
	#endif

	createCommandBuffers();
	createSyncObjects();
	worker.start();

	timer.startTimer();
	profiler.startTimer();

	while (!io.getWindowShouldClose())
	{
		#ifdef DEBUG_RENDERLOOP
			std::cout << "Render loop 1/2 ----------" << std::endl;
		#endif
		
		io.pollEvents();	// Check for events (processes only those events that have already been received and then returns immediately)
		drawFrame();

		if (io.getKey(GLFW_KEY_ESCAPE) == GLFW_PRESS)
			io.setWindowShouldClose(true);

		#ifdef DEBUG_RENDERLOOP
			std::cout << "Render loop 2/2 ----------" << std::endl;
		#endif
	}
	
	worker.stop();

	vkDeviceWaitIdle(e.c.device);	// Waits for the logical device to finish operations. Needed for cleaning up once drawing and presentation operations (drawFrame) have finished. Use vkQueueWaitIdle for waiting for operations in a specific command queue to be finished.

	cleanup();
	
	#ifdef DEBUG_RENDERER
		std::cout << typeid(*this).name() << "::" << __func__ << " end" << std::endl;
	#endif
}

void Renderer::recreateSwapChain()
{
	#ifdef DEBUG_RENDERER
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif
	
	// Get window size
	int width = 0, height = 0;
	//io.getFramebufferSize(&width, &height);
	while (width == 0 || height == 0) 
	{
		io.getFramebufferSize(&width, &height);
		io.waitEvents();
	}
	std::cout << "New window size: " << width << ", " << height << std::endl;

	vkDeviceWaitIdle(e.c.device);			// We shouldn't touch resources that may be in use.

	// Cleanup swapChain:
	cleanupSwapChain();

	// Recreate swapChain:
	//    - Environment
	e.recreate_RenderPipeline_SwapChain();

	//    - Each model
	const std::lock_guard<std::mutex> lock(worker.mutModels);


	for (auto it = models.begin(); it != models.end(); it++)
				it->second.recreate_Pipeline_Descriptors();

	//    - Renderer
	const std::lock_guard<std::mutex> lock2(e.mutCommandPool);
	createCommandBuffers();				// Command buffers directly depend on the swap chain images.
	imagesInFlight.resize(e.swapChain.images.size(), VK_NULL_HANDLE);
}

void Renderer::cleanupSwapChain()
{
	#ifdef DEBUG_RENDERER
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif
	
	{
		const std::lock_guard<std::mutex> lock(e.mutQueue);
		vkQueueWaitIdle(e.c.graphicsQueue);
	}
	{
		const std::lock_guard<std::mutex> lock(e.mutCommandPool);
		vkFreeCommandBuffers(e.c.device, e.commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
	}

	// Models
	{
		const std::lock_guard<std::mutex> lock(worker.mutModels);

		for(auto it = models.begin(); it != models.end(); it++)
			it->second.cleanup_Pipeline_Descriptors();
	}

	// Environment
	e.cleanup_RenderPipeline_SwapChain();
}

void Renderer::clearDepthBuffer(VkCommandBuffer commandBuffer)
{
	VkClearAttachment attachmentToClear;
	attachmentToClear.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	attachmentToClear.clearValue.depthStencil = { 1.0f, 0 };

	VkClearRect rectangleToClear;
	rectangleToClear.rect.offset = { 0, 0 };
	rectangleToClear.rect.extent = e.swapChain.extent;
	rectangleToClear.baseArrayLayer = 0;
	rectangleToClear.layerCount = 1;

	vkCmdClearAttachments(commandBuffer, 1, &attachmentToClear, 1, &rectangleToClear);
}

void Renderer::distributeKeys()
{
	for (auto& rp : keys)
		for (auto& sp : rp)
			sp.clear();

	for (auto it = models.begin(); it != models.end(); it++)
		if(it->second.getActiveInstancesCount() && it->second.ready)
			keys[it->second.renderPassIndex][it->second.subpassIndex].push_back(it->first);
}

key64 Renderer::getNewKey()
{
	do
	{
		if (models.find(++newKey) == models.end())
			return newKey;
	} while (true);
}

void Renderer::cleanup()
{
	#ifdef DEBUG_RENDERER
		std::cout << typeid(*this).name() << "::" << __func__ << " (1/2)" << std::endl;
	#endif
	
	// Cleanup renderer
	//cleanupSwapChain();
	
	// Renderer
	{
		const std::lock_guard<std::mutex> lock(e.mutQueue);
		vkQueueWaitIdle(e.c.graphicsQueue);
	}
	{
		const std::lock_guard<std::mutex> lock(e.mutCommandPool);
		vkFreeCommandBuffers(e.c.device, e.commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());	// Free Command buffers
	}
	
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {							// Semaphores (render & image available) & fences (in flight)
		vkDestroySemaphore(e.c.device, renderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(e.c.device, imageAvailableSemaphores[i], nullptr);
		vkDestroyFence(e.c.device, framesInFlight[i], nullptr);
	}
	
	// Cleanup models, textures and shaders
	// const std::lock_guard<std::mutex> lock(worker.mutModels);	// Not necessary (worker stopped loading thread)
	
	models.clear();

	if(globalUBO_vs.totalBytes)  globalUBO_vs.destroyUBO();
	if (globalUBO_fs.totalBytes) globalUBO_fs.destroyUBO();
	
	// Cleanup environment
	e.cleanup();
	
	#ifdef DEBUG_RENDERER
		std::cout << typeid(*this).name() << "::" << __func__ << " (2/2)" << std::endl;
	#endif
}

key64 Renderer::newModel(ModelDataInfo& modelInfo)
{
	#ifdef DEBUG_RENDERER
		std::cout << typeid(*this).name() << "::" << __func__ << ": " << modelInfo.name << std::endl;
	#endif

	if (modelInfo.renderPassIndex < keys.size() && modelInfo.subpassIndex < keys[modelInfo.renderPassIndex].size())
	{
		std::pair<std::unordered_map<key64, ModelData>::iterator, bool> result = 
			models.emplace(std::make_pair(getNewKey(), ModelData(&e, modelInfo)));   // Save model object into model list
		
		worker.newTask(result.first->first, construct);   // Schedule task: Construct model

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
	
	worker.newTask(key, delet);
}

ModelData* Renderer::getModel(key64 key)
{
	if (models.find(key) != models.end())
		return &models[key];
	else
		return nullptr;
}

void Renderer::setInstances(key64 key, size_t numberOfRenders)
{
	#ifdef DEBUG_RENDERER
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif
	
	const std::lock_guard<std::mutex> lock(worker.mutModels);

	if(models.find(key) != models.end())
		if(models[key].setActiveInstancesCount(numberOfRenders))
			updateCommandBuffer = true;		// We flag commandBuffer for update assuming that our model is in list "model"
}

void Renderer::setInstances(std::vector<key64>& keys, size_t numberOfRenders)
{
	#ifdef DEBUG_RENDERER
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	const std::lock_guard<std::mutex> lock(worker.mutModels);

	for(key64 key : keys)
		if (models.find(key) != models.end())
			if (models[key].setActiveInstancesCount(numberOfRenders))
				updateCommandBuffer = true;		// We flag commandBuffer for update assuming that our model is in list "model"
}

void Renderer::setMaxFPS(int maxFPS)
{
	if (maxFPS > 0)
		this->maxFPS = maxFPS;
	else
		this->maxFPS = 0;
}

void Renderer::updateStates(uint32_t currentImage)
{
	#if defined(DEBUG_RENDERER) || defined(DEBUG_RENDERLOOP)
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif

	// - USER UPDATES

	#ifdef DEBUG_RENDERLOOP
		std::cout << "userUpdate()" << std::endl;
	#endif
	
	timer.updateTime();
	waitForFPS(timer, maxFPS);
	timer.reUpdateTime();

#if defined(DEBUG_REND_PROFILER)
	PRINT("  waitForFPS: ", profiler.updateTime() * 1000.f);
#endif

	userUpdate(*this);		// Update model matrices and other things (user defined)

#if defined(DEBUG_REND_PROFILER)
	PRINT("  userUpdate: ", profiler.updateTime() * 1000.f);
#endif

	// - MOVE MODELS
	/*
	#ifdef DEBUG_RENDERLOOP
		std::cout << "Move models" << std::endl;
	#endif
	
	uint32_t rp, sp;

	{
		const std::lock_guard<std::mutex> lock(worker.mutModels);
		
		while (lastModelsToDraw.size())		// Move the modelIter to last position in models and update 
		{
			if (lastModelsToDraw[0]->inModels)
			{
				rp = lastModelsToDraw[0]->renderPassIndex;
				sp = lastModelsToDraw[0]->subpassIndex;
				models.splice(models[rp][sp].end(), models[rp][sp], lastModelsToDraw[0]);
			}
			lastModelsToDraw.erase(lastModelsToDraw.begin());

			// <<< updateCommandBuffer = true;
		}
	}
	*/

	// - COPY DATA FROM UBOS TO GPU MEMORY

	// Copy the data in the uniform buffer object to the current uniform buffer
	// <<< Using a UBO this way is not the most efficient way to pass frequently changing values to the shader. Push constants are more efficient for passing a small buffer of data to shaders.

	#ifdef DEBUG_RENDERLOOP
		std::cout << "Copy UBOs" << std::endl;
	#endif
	
	const std::lock_guard<std::mutex> lock(worker.mutModels);

	distributeKeys();

	void* data;
	size_t activeBytes;
	ModelData* model;

	// Global UBOs
	if (globalUBO_vs.totalBytes)
	{
		vkMapMemory(e.c.device, globalUBO_vs.uboMemories[currentImage], 0, globalUBO_vs.totalBytes, 0, &data);
		memcpy(data, globalUBO_vs.ubo.data(), globalUBO_vs.totalBytes);
		vkUnmapMemory(e.c.device, globalUBO_vs.uboMemories[currentImage]);
	}

	if (globalUBO_fs.totalBytes)
	{
		vkMapMemory(e.c.device, globalUBO_fs.uboMemories[currentImage], 0, globalUBO_fs.totalBytes, 0, &data);
		memcpy(data, globalUBO_fs.ubo.data(), globalUBO_fs.totalBytes);
		vkUnmapMemory(e.c.device, globalUBO_fs.uboMemories[currentImage]);
	}

	// Local UBOs
	for(auto it = models.begin(); it != models.end(); it++)
		if(it->second.ready)
		{
			model = &it->second;

			activeBytes = model->vsUBO.numActiveSubUbos * model->vsUBO.subUboSize;
			if (activeBytes)
			{
				vkMapMemory(e.c.device, model->vsUBO.uboMemories[currentImage], 0, activeBytes, 0, &data);	// Get a pointer to some Vulkan/GPU memory of size X. vkMapMemory retrieves a host virtual address pointer (data) to a region of a mappable memory object (uniformBuffersMemory[]). We have to provide the logical device that owns the memory (e.device).
				memcpy(data, model->vsUBO.ubo.data(), activeBytes);											// Copy some data in that memory. Copies a number of bytes (sizeof(ubo)) from a source (ubo) to a destination (data).
				vkUnmapMemory(e.c.device, model->vsUBO.uboMemories[currentImage]);							// "Get rid" of the pointer. Unmap a previously mapped memory object (uniformBuffersMemory[]).
			}

			activeBytes = model->fsUBO.numActiveSubUbos * model->fsUBO.subUboSize;
			if (activeBytes)
			{
				vkMapMemory(e.c.device, model->fsUBO.uboMemories[currentImage], 0, activeBytes, 0, &data);
				memcpy(data, model->fsUBO.ubo.data(), activeBytes);
				vkUnmapMemory(e.c.device, model->fsUBO.uboMemories[currentImage]);
			}
		}
}

void Renderer::createLightingPass(unsigned numLights, std::string vertShaderPath, std::string fragShaderPath, std::string fragToolsHeader)
{
	std::vector<float> v_quad;	// [4 * 5]
	std::vector<uint16_t> i_quad;
	getScreenQuad(v_quad, i_quad, 1.f, 0.f);	// <<< The parameter zValue doesn't represent height (otherwise, this value should serve for hiding one plane behind another).

	std::vector<ShaderLoader*> usedShaders{ 
		SL_fromFile::factory(vertShaderPath),
		SL_fromFile::factory(fragShaderPath, { SMod::changeHeader(fragToolsHeader) })
	};

	std::vector<TextureLoader*> usedTextures{ };

	ModelDataInfo modelInfo;
	modelInfo.name = "lightingPass";
	modelInfo.activeInstances = 1;
	modelInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	modelInfo.vertexType = vt_32;
	modelInfo.vertexesLoader = VL_fromBuffer::factory(v_quad.data(), vt_32.vertexSize, 4, i_quad, {});
	modelInfo.shadersInfo = usedShaders;
	modelInfo.texturesInfo = usedTextures;
	modelInfo.maxDescriptorsCount_vs = 0;
	modelInfo.maxDescriptorsCount_fs = 1;
	modelInfo.UBOsize_vs = 0;
	modelInfo.UBOsize_fs = sizes::vec4 + numLights * sizeof(Light);	// camPos,  n * LightPosDir (2*vec4),  n * LightProps (6*vec4);
	modelInfo.globalUBO_vs;
	modelInfo.globalUBO_fs;
	modelInfo.transparency = false;
	modelInfo.renderPassIndex = 1;
	modelInfo.subpassIndex = 0;

	lightingPass = newModel(modelInfo);
}

void Renderer::updateLightingPass(glm::vec3& camPos, Light* lights, unsigned numLights)
{
	if (models.find(lightingPass) == models.end()) return;

	uint8_t* dest;

	//for (int i = 0; i < lightingPass->vsUBO.numActiveDescriptors; i++)
	//{
	//	dest = lightingPass->vsUBO.getDescriptorPtr(i);
	//	//...
	//}

	for (int i = 0; i < models[lightingPass].fsUBO.numActiveSubUbos; i++)
	{
		dest = models[lightingPass].fsUBO.getSubUboPtr(i);
		memcpy(dest, &camPos, sizes::vec4);
		dest += sizes::vec4;
		memcpy(dest, lights, numLights * sizeof(Light));
	}
}

void Renderer::createPostprocessingPass(std::string vertShaderPath, std::string fragShaderPath)
{
	std::vector<float> v_quad;	// [4 * 5]
	std::vector<uint16_t> i_quad;
	getScreenQuad(v_quad, i_quad, 1.f, 0.f);	// <<< The parameter zValue doesn't represent heigth (otherwise, this value should serve for hiding one plane behind another).

	std::vector<ShaderLoader*> usedShaders{
		SL_fromFile::factory(vertShaderPath),
		SL_fromFile::factory(fragShaderPath)
	};
	
	std::vector<TextureLoader*> usedTextures{ };

	ModelDataInfo modelInfo;
	modelInfo.name = "postprocessingPass";
	modelInfo.activeInstances = 1;
	modelInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	modelInfo.vertexType = vt_32;
	modelInfo.vertexesLoader = VL_fromBuffer::factory(v_quad.data(), vt_32.vertexSize, 4, i_quad, {});
	modelInfo.shadersInfo = usedShaders;
	modelInfo.texturesInfo = usedTextures;
	modelInfo.maxDescriptorsCount_vs = 0;
	modelInfo.maxDescriptorsCount_fs = 0;
	modelInfo.UBOsize_vs = 0;
	modelInfo.UBOsize_fs = 0;
	modelInfo.globalUBO_vs;
	modelInfo.globalUBO_fs;
	modelInfo.transparency = false;
	modelInfo.renderPassIndex = 3;
	modelInfo.subpassIndex = 0;

	postprocessingPass = newModel(modelInfo);
}

void Renderer::updatePostprocessingPass()
{
	// No code necessary here
}

Timer& Renderer::getTimer() { return timer; }

size_t Renderer::getRendersCount(key64 key)
{
	if (models.find(key) != models.end())
		return models[key].getActiveInstancesCount();
	else
		return 0;
}

size_t Renderer::getFrameCount() { return frameCount; }

size_t Renderer::getFPS() { return std::round(1 / timer.getDeltaTime()); }

size_t Renderer::getModelsCount() { return models.size(); }

size_t Renderer::getCommandsCount() { return commandsCount; }

size_t Renderer::loadedShaders() { return shaders.size(); }

size_t Renderer::loadedTextures() { return textures.size(); }

IOmanager& Renderer::getIO() { return io; }

int Renderer::getMaxMemoryAllocationCount() { return e.c.deviceData.maxMemoryAllocationCount; }

int Renderer::getMemAllocObjects() { return e.c.memAllocObjects; }
