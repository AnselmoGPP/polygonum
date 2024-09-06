
#include <iostream>
#include <stdexcept>
#include <cstdlib>				// EXIT_SUCCESS, EXIT_FAILURE
#include <array>
#include <cstdint>				// UINT32_MAX
#include <algorithm>			// std::min / std::max
#include <fstream>
#include <chrono>
#include <string>

#include "renderer.hpp"
#include "commons.hpp"
#include "toolkit.hpp"


// LoadingWorker ---------------------------------------------------------------------

LoadingWorker::LoadingWorker(int waitTime, std::vector<std::vector<std::list<ModelData>>>& models, std::list<ModelData>& modelsToLoad, std::list<ModelData>& modelsToDelete, std::list<Texture>& textures, std::list<Shader>& shaders, bool& updateCommandBuffer)
	: models(models), modelsToLoad(modelsToLoad), modelsToDelete(modelsToDelete), textures(textures), shaders(shaders), updateCommandBuffer(updateCommandBuffer), waitTime(waitTime), runThread(false) { }

LoadingWorker::~LoadingWorker() 
{ 
	#ifdef DEBUG_WORKER
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif
	//if (modelTP.size()) modelTP.clear();
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

bool LoadingWorker::isBeingProcessed(modelIter model)
{
	if (modelTP.size() && modelTP.begin() == model) return true;
	else return false;
}

void LoadingWorker::loadingThread()
{
	#ifdef DEBUG_WORKER
		std::cout << "- " << typeid(*this).name() << "::" << __func__ << " (begin)" << std::endl;
		std::cout << "- Loading thread ID: " << std::this_thread::get_id() << std::endl;
	#endif

	std::list<ModelData>::iterator mIter;
	std::list<Shader   >::iterator sIter, sIter2;
	std::list<Texture  >::iterator tIter, tIter2;
	bool modelsDeleted = false;
	size_t rp, sp;								//!< Render pass & subpass indices

	while (runThread)
	{
		// LOAD MODELS
		#ifdef DEBUG_WORKER
			std::cout << "- New iteration -----" << std::endl;
			std::cout << "   - Load models (" << modelsToLoad.size() << ')' << std::endl;
		#endif

		while (modelsToLoad.size())
		{
			// Move model from modelsToLoad to modelTP.
			{
				const std::lock_guard<std::mutex> lock(mutLoad);

				if (modelsToLoad.begin() != modelsToLoad.end())
				{
					#ifdef DEBUG_WORKER
						std::cout << "      - " << modelsToLoad.begin()->name << std::endl;
					#endif
					
					modelTP.splice(modelTP.cend(), modelsToLoad, modelsToLoad.begin());
				}
			}
			
			// Process modelTP (load data and upload to Vulkan) and move it to models.
			if (modelTP.size())
			{
				mIter = modelTP.begin();
				mIter->fullConstruction(shaders, textures, mutResources);
				mIter->fullyConstructed = true;
				mIter->inModels = true;
				rp = mIter->renderPassIndex;
				sp = mIter->subpassIndex;

				const std::lock_guard<std::mutex> lock(mutModels);

				models[rp][sp].splice(models[rp][sp].cend(), modelTP, mIter);
				updateCommandBuffer = true;
			}
		}
		
		// DELETE MODELS
		#ifdef DEBUG_WORKER
			std::cout << "   - Delete models (" << modelsToDelete.size() << ')' << std::endl;
		#endif

		while (modelsToDelete.size())
		{
			// Move model from modelsToDelete to modelTP.
			{
				const std::lock_guard<std::mutex> lock(mutDelete);
				
				if (modelsToDelete.begin() != modelsToDelete.end())
				{
					#ifdef DEBUG_WORKER
						std::cout << "      - " << modelsToDelete.begin()->name << std::endl;
					#endif

					modelTP.splice(modelTP.cend(), modelsToDelete, modelsToDelete.begin());
				}
			}
			
			// Process modelTP (delete model).
			if (modelTP.size())
			{
				modelTP.erase(modelTP.begin());
				modelsDeleted = true;
			}
		}
		
		// DELETE UNUSED RESOURCES <<< This should be done in main thread, not in loading thread because it could be redundant in an scenario of multiple loading threads.
		#ifdef DEBUG_WORKER
			std::cout << "   - Delete resources (" << shaders.size() << ", " << textures.size() << ')' << std::endl;
		#endif
		
		if (modelsDeleted)
		{
			const std::lock_guard<std::mutex> lock(mutResources);

			// Shaders
			#ifdef DEBUG_WORKER
				std::cout << "      - Delete shaders" << std::endl;
			#endif
			
			sIter = shaders.begin();
			while(sIter != shaders.end())
			{
				#ifdef DEBUG_WORKER
					if (!sIter->counter) std::cout << "         - " << sIter->id << std::endl;
				#endif
				
				sIter2 = sIter++;
				if (!sIter2->counter) shaders.erase(sIter2);
			}

			// Textures
			#ifdef DEBUG_WORKER
				std::cout << "      - Delete textures" << std::endl;
			#endif
			
			tIter = textures.begin();
			while (tIter != textures.end())
			{
				#ifdef DEBUG_WORKER
					if (!tIter->counter) std::cout << "         - " << tIter->id << std::endl;
				#endif
				
				tIter2 = tIter++;
				if (!tIter2->counter) textures.erase(tIter2);
			}

			modelsDeleted = false;
		}
		
		std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
	}

	#ifdef DEBUG_WORKER
		std::cout << "- " << typeid(*this).name() << "::" << __func__ << " (end)" << std::endl;
	#endif
}


// ShaderIncluder ---------------------------------------------------------------------

shaderc_include_result* ShaderIncluder::GetInclude(const char* sourceName, shaderc_include_type type, const char* destName, size_t includeDepth)
{
	auto container = new std::array<std::string, 2>;
	(*container)[0] = std::string(sourceName);
	readFile(sourceName, (*container)[1]);

	auto data = new shaderc_include_result;
	data->user_data = container;
	data->source_name = (*container)[0].data();
	data->source_name_length = (*container)[0].size();
	data->content = (*container)[1].data();
	data->content_length = (*container)[1].size();

	return data;
}

void ShaderIncluder::ReleaseInclude(shaderc_include_result* data)
{
	delete static_cast<std::array<std::string, 2>*>(data->user_data);
	delete data;
}


// Renderer ---------------------------------------------------------------------

Renderer::Renderer(void(*graphicsUpdate)(Renderer&), IOmanager& io, UBOinfo globalUBO_vs, UBOinfo globalUBO_fs)
	:
	e(io),
	io(io),
	updateCommandBuffer(false), 
	userUpdate(graphicsUpdate), 
	currentFrame(0), 
	commandsCount(0),
	frameCount(0),
	globalUBO_vs(&e, globalUBO_vs),
	globalUBO_fs(&e, globalUBO_fs),
	worker(500, models, modelsToLoad, modelsToDelete, textures, shaders, updateCommandBuffer)
{ 
	#ifdef DEBUG_RENDERER
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
		std::cout << "Main thread ID: " << std::this_thread::get_id() << std::endl;
		std::cout << "   Hardware concurrency: " << (unsigned int)std::thread::hardware_concurrency << std::endl;
	#endif

	// Prepare "models" list
	models.resize(e.rp->renderPasses.size());
	for (size_t rp = 0; rp < models.size(); rp++)
		models[rp].resize(e.rp->renderPasses[rp].subpasses.size());

	// Create UBOs
	if (this->globalUBO_vs.totalBytes) this->globalUBO_vs.createUBObuffers();
	if (this->globalUBO_fs.totalBytes) this->globalUBO_fs.createUBObuffers();
}

Renderer::~Renderer() 
{ 
	#ifdef DEBUG_RENDERER
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
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

		for (size_t rp = 0; rp < models.size(); rp++)		// for each RENDER PASS (color pass, post-processing...)
		{
			#ifdef DEBUG_COMMANDBUFFERS
				std::cout << "   Render pass " << rp << std::endl;
			#endif
			
			vkCmdBeginRenderPass(commandBuffers[i], &e.rp->renderPasses[rp].renderPassInfos[i], VK_SUBPASS_CONTENTS_INLINE);	// Start RENDER PASS. VK_SUBPASS_CONTENTS_INLINE (the render pass commands will be embedded in the primary command buffer itself and no secondary command buffers will be executed), VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS (the render pass commands will be executed from secondary command buffers).
			//vkCmdNextSubpass(commandBuffers[i], VK_SUBPASS_CONTENTS_INLINE);						// Start SUBPASS
			
			for (size_t sp = 0; sp < models[rp].size(); sp++)		// for each SUB-PASS
			{
				if(sp > 0) vkCmdNextSubpass(commandBuffers[i], VK_SUBPASS_CONTENTS_INLINE);			// Start SUBPASS
				//clearDepthBuffer(commandBuffers[i]);		// Already done in createRenderPass() (loadOp). Previously used for implementing layers (Painter's algorithm).

				for (modelIter it = models[rp][sp].begin(); it != models[rp][sp].end(); it++)		// for each MODEL
				{
					#ifdef DEBUG_COMMANDBUFFERS
						std::cout << "         Model: " << it->name << std::endl;
					#endif
					
					if (!it->activeInstances) continue;

					vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, it->graphicsPipeline);	// Second parameter: Specifies if the pipeline object is a graphics or compute pipeline.
					vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, &it->vert.vertexBuffer, offsets);

					if (it->vert.indexCount)		// has indices (it doesn't if data represents points)
						vkCmdBindIndexBuffer(commandBuffers[i], it->vert.indexBuffer, 0, VK_INDEX_TYPE_UINT16);

					if (it->descriptorSets.size())	// has descriptor set (UBOs, textures, input attachments)
						vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, it->pipelineLayout, 0, 1, &it->descriptorSets[i], 0, 0);
					
					if (it->vert.indexCount)		// has indices
						vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(it->vert.indexCount), it->activeInstances, 0, 0, 0);
					else
						vkCmdDraw(commandBuffers[i], it->vert.vertexCount, it->activeInstances, 0, 0);

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

	while (!io.windowShouldClose())
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
	
	// Wait for the frame to be finished (command buffer execution). If VK_TRUE, we wait for all fences; otherwise, we wait for any.
	vkWaitForFences(e.c.device, 1, &framesInFlight[currentFrame], VK_TRUE, UINT64_MAX);

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

	// Check if this image is being used. If used, wait. Then, mark it as used by this frame.
	if (imagesInFlight[imageIndex] != VK_NULL_HANDLE)									// Check if a previous frame is using this image (i.e. there is its fence to wait on)
		vkWaitForFences(e.c.device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
	imagesInFlight[imageIndex] = framesInFlight[currentFrame];							// Mark the image as now being in use by this frame

	updateStates(imageIndex);

	// Submit the command buffer
	VkSubmitInfo submitInfo{};
	VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };			// Which semaphores to wait on before execution begins.
	VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };		// Which semaphores to signal once the command buffers have finished execution.
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };	// In which stages of the pipeline to wait the semaphore. VK_PIPELINE_STAGE_ ... TOP_OF_PIPE_BIT (ensures that the render passes don't begin until the image is available), COLOR_ATTACHMENT_OUTPUT_BIT (makes the render pass wait for this stage).
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

	// Note:
	// Subpass dependencies: Subpasses in a render pass automatically take care of image layout transitions. These transitions are controlled by subpass dependencies (specify memory and execution dependencies between subpasses).
	// There are two built-in dependencies that take care of the transition at the start and at the end of the render pass, but the former does not occur at the right time. It assumes that the transition occurs at the start of the pipeline, but we haven't acquired the image yet at that point. Two ways to deal with this problem:
	//		- waitStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT (ensures that the render passes don't begin until the image is available).
	//		- waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT (makes the render pass wait for this stage).

	// Presentation (submit the result back to the swap chain to have it eventually show up on the screen).
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType				= VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount	= 1;
	presentInfo.pWaitSemaphores		= signalSemaphores;
	VkSwapchainKHR swapChains[]		= { e.swapChain.swapChain };
	presentInfo.swapchainCount		= 1;
	presentInfo.pSwapchains			= swapChains;
	presentInfo.pImageIndices		= &imageIndex;
	presentInfo.pResults			= nullptr;			// Optional

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

	for (uint32_t rp = 0; rp < models.size(); rp++)
		for (uint32_t sp = 0; sp < models[rp].size(); sp++)
			for (modelIter it = models[rp][sp].begin(); it != models[rp][sp].end(); it++)
				it->recreate_Pipeline_Descriptors();

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

		for (uint32_t rp = 0; rp < models.size(); rp++)
			for (uint32_t sp = 0; sp < models[rp].size(); sp++)
				for (modelIter it = models[rp][sp].begin(); it != models[rp][sp].end(); it++)
					it->cleanup_Pipeline_Descriptors();
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
	
	//for (size_t i = 0; i < models.size(); i++) 
	//	models[i].clear();
	models.clear();
	modelsToLoad.clear();
	modelsToDelete.clear();
	textures.clear();
	shaders.clear();

	if(globalUBO_vs.totalBytes)  globalUBO_vs.destroyUBOs();
	if (globalUBO_fs.totalBytes) globalUBO_fs.destroyUBOs();
	
	// Cleanup environment
	e.cleanup();
	
	#ifdef DEBUG_RENDERER
		std::cout << typeid(*this).name() << "::" << __func__ << " (2/2)" << std::endl;
	#endif
}

modelIter Renderer::newModel(ModelDataInfo& modelInfo)
{
	#ifdef DEBUG_RENDERER
		std::cout << typeid(*this).name() << "::" << __func__ << ": " << modelInfo.name << std::endl;
	#endif

	if (modelInfo.renderPassIndex < models.size() && modelInfo.subpassIndex < models[modelInfo.renderPassIndex].size())
	{
		const std::lock_guard<std::mutex> lock(worker.mutLoad);

		return modelsToLoad.emplace(modelsToLoad.cend(), e, modelInfo);
	}
	
	std::cout << "The renderpass/subpass specified for this model (" << modelInfo.name << ": " << modelInfo.renderPassIndex << '/' << modelInfo.subpassIndex << ") doesn't fit the render pipeline" << std::endl;
	return modelsToLoad.end();
}

void Renderer::deleteModel(modelIter model)	// <<< splice an element only knowing the iterator (no need to check lists)?
{
	#ifdef DEBUG_RENDERER
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif
	
	// If the model was not found on modelsToDelete, look in modelsToLoad
	// Problem: What if the model is being loaded, or was loaded after the delete order?
	// Solution: Wait for any model to delete in ModelsToLoad to be loaded. Then, delete. (this doesn't solve second problem).

	//size_t rpi = model->renderPassIndex;	// Removed: Crashes when the modelIter points to a model that no longer exists (cleared during cleanup())
	
	while (true)	// If model is being processed, continue in loop until it has been loaded and delete it.
	{
		{
			// Be the only thread touching the lists of models
			const std::lock_guard<std::mutex> lock_1(worker.mutModels);
			const std::lock_guard<std::mutex> lock_3(worker.mutLoad);
			const std::lock_guard<std::mutex> lock_2(worker.mutDelete);
			
			// Look in Renderer::models
			for(unsigned rp = 0; rp < models.size(); rp++)
				for (unsigned sp = 0; sp < models[rp].size(); sp++)
					for (auto it = models[rp][sp].begin(); it != models[rp][sp].end(); it++)
						if (&*it == &*model)	// https://stackoverflow.com/questions/6302706/check-whether-iterator-belongs-to-a-list
						{
							model->inModels = false;
							modelsToDelete.splice(modelsToDelete.cend(), models[rp][sp], model);
							updateCommandBuffer = true;
							return;
						}
			
			// Look in Renderer::modelsToLoad
			for (auto it = modelsToLoad.begin(); it != modelsToLoad.end(); it++)
				if (&*it == &*model)
				{
					PRINT("load");
					model->inModels = false;
					modelsToDelete.splice(modelsToDelete.cend(), modelsToLoad, model);
					return;
				}

			// If model is not being processed, exit loop
			if (!worker.isBeingProcessed(model)) break;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(3));
	}

	std::cout << "Model to delete not found" << std::endl;
}

void Renderer::setInstances(modelIter model, size_t numberOfRenders)
{
	#ifdef DEBUG_RENDERER
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif
	
	// CHECK FOR EXISTENCE: std::find() or check delete modelIter (677)
	//if(exists(model))
	if(model->setActiveInstancesCount(numberOfRenders))
		updateCommandBuffer = true;		// We flag commandBuffer for update assuming that our model is in list "model"
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
	
	timer.computeDeltaTime();

	userUpdate(*this);		// Update model matrices and other things (user defined)

	// - MOVE MODELS

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
				models[rp][sp].splice(models[rp][sp].end(), models[rp][sp], lastModelsToDraw[0]);
			}
			lastModelsToDraw.erase(lastModelsToDraw.begin());

			// <<< updateCommandBuffer = true;
		}
	}

	// - COPY DATA FROM UBOS TO GPU MEMORY

	// Copy the data in the uniform buffer object to the current uniform buffer
	// <<< Using a UBO this way is not the most efficient way to pass frequently changing values to the shader. Push constants are more efficient for passing a small buffer of data to shaders.

	#ifdef DEBUG_RENDERLOOP
		std::cout << "Copy UBOs" << std::endl;
	#endif
	
	const std::lock_guard<std::mutex> lock(worker.mutModels);

	void* data;

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

	size_t activeBytes;

	for (rp = 0; rp < models.size(); rp++)
		for (sp = 0; sp < models[rp].size(); sp++)
			for (modelIter it = models[rp][sp].begin(); it != models[rp][sp].end(); it++)
			{
				activeBytes = it->vsUBO.numActiveDescriptors * it->vsUBO.descriptorSize;
				if (activeBytes)
				{
					vkMapMemory(e.c.device, it->vsUBO.uboMemories[currentImage], 0, activeBytes, 0, &data);	// Get a pointer to some Vulkan/GPU memory of size X. vkMapMemory retrieves a host virtual address pointer (data) to a region of a mappable memory object (uniformBuffersMemory[]). We have to provide the logical device that owns the memory (e.device).
					memcpy(data, it->vsUBO.ubo.data(), activeBytes);										// Copy some data in that memory. Copies a number of bytes (sizeof(ubo)) from a source (ubo) to a destination (data).
					vkUnmapMemory(e.c.device, it->vsUBO.uboMemories[currentImage]);							// "Get rid" of the pointer. Unmap a previously mapped memory object (uniformBuffersMemory[]).
				}

				activeBytes = it->fsUBO.numActiveDescriptors * it->fsUBO.descriptorSize;
				if (activeBytes)
				{
					vkMapMemory(e.c.device, it->fsUBO.uboMemories[currentImage], 0, activeBytes, 0, &data);
					memcpy(data, it->fsUBO.ubo.data(), activeBytes);
					vkUnmapMemory(e.c.device, it->fsUBO.uboMemories[currentImage]);
				}
			}

	// - UPDATE COMMAND BUFFER
	#ifdef DEBUG_RENDERLOOP
		std::cout << "Update command buffer" << std::endl;
	#endif

	if (updateCommandBuffer)
	{
		{
			//const std::lock_guard<std::mutex> lock(e.mutQueue);
			//vkQueueWaitIdle(e.c.graphicsQueue);
		}
		vkWaitForFences(e.c.device, 1, &lastFence, VK_TRUE, UINT64_MAX);
		
		const std::lock_guard<std::mutex> lock(e.mutCommandPool);		// vkQueueWaitIdle(e.c.graphicsQueue) was called before, in drawFrame()
		vkFreeCommandBuffers(e.c.device, e.commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());	// Any primary command buffer that is in the recording or executable state and has any element of pCommandBuffers recorded into it, becomes invalid.
		createCommandBuffers();
	}
}


void Renderer::createLightingPass(unsigned numLights, std::string vertShaderPath, std::string fragShaderPath, std::string fragToolsHeader)
{
	std::vector<float> v_quad;	// [4 * 5]
	std::vector<uint16_t> i_quad;
	getScreenQuad(v_quad, i_quad, 1.f, 0.f);	// <<< The parameter zValue doesn't represent height (otherwise, this value should serve for hiding one plane behind another).

	VerticesLoader vertexData(vt_32.vertexSize, v_quad.data(), 4, i_quad);

	std::vector<ShaderLoader> usedShaders{ 
		ShaderLoader(vertShaderPath),
		ShaderLoader(fragShaderPath, std::vector<ShaderModifier>{ {sm_changeHeader, {fragToolsHeader}} })
	};

	std::vector<TextureLoader> usedTextures{ };

	ModelDataInfo modelInfo;
	modelInfo.name = "lightingPass";
	modelInfo.activeInstances = 1;
	modelInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	modelInfo.vertexType = vt_32;
	modelInfo.verticesLoader = &vertexData;
	modelInfo.shadersInfo = &usedShaders;
	modelInfo.texturesInfo = &usedTextures;
	modelInfo.maxDescriptorsCount_vs = 0;
	modelInfo.maxDescriptorsCount_fs = 1;
	modelInfo.UBOsize_vs = 0;
	modelInfo.UBOsize_fs = size.vec4 + numLights * sizeof(Light);	// camPos,  n * LightPosDir (2*vec4),  n * LightProps (6*vec4);
	modelInfo.globalUBO_vs;
	modelInfo.globalUBO_fs;
	modelInfo.transparency = false;
	modelInfo.renderPassIndex = 1;
	modelInfo.subpassIndex = 0;

	lightingPass = newModel(modelInfo);
}

void Renderer::updateLightingPass(glm::vec3& camPos, Light* lights, unsigned numLights)
{
	uint8_t* dest;

	//for (int i = 0; i < lightingPass->vsUBO.numActiveDescriptors; i++)
	//{
	//	dest = lightingPass->vsUBO.getDescriptorPtr(i);
	//	//...
	//}

	for (int i = 0; i < lightingPass->fsUBO.numActiveDescriptors; i++)
	{
		dest = lightingPass->fsUBO.getDescriptorPtr(i);
		memcpy(dest, &camPos, size.vec4);
		dest += size.vec4;
		memcpy(dest, lights, numLights * sizeof(Light));
	}
}

void Renderer::createPostprocessingPass(std::string vertShaderPath, std::string fragShaderPath)
{
	std::vector<float> v_quad;	// [4 * 5]
	std::vector<uint16_t> i_quad;
	getScreenQuad(v_quad, i_quad, 1.f, 0.f);	// <<< The parameter zValue doesn't represent heigth (otherwise, this value should serve for hiding one plane behind another).

	VerticesLoader vertexData(vt_32.vertexSize, v_quad.data(), 4, i_quad);

	std::vector<ShaderLoader> usedShaders{ vertShaderPath, fragShaderPath };
	
	std::vector<TextureLoader> usedTextures{ };

	ModelDataInfo modelInfo;
	modelInfo.name = "postprocessingPass";
	modelInfo.activeInstances = 1;
	modelInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	modelInfo.vertexType = vt_32;
	modelInfo.verticesLoader = &vertexData;
	modelInfo.shadersInfo = &usedShaders;
	modelInfo.texturesInfo = &usedTextures;
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
	
}

TimerSet& Renderer::getTimer() { return timer; }

size_t Renderer::getRendersCount(modelIter model) { return model->activeInstances; }

size_t Renderer::getFrameCount() { return frameCount; }

size_t Renderer::getModelsCount()
{
	size_t count = 0;

	for (auto& renderPass : models)
		for (auto& subPass : renderPass)
			count += subPass.size();

	return count;
}

size_t Renderer::getCommandsCount() { return commandsCount; }

size_t Renderer::loadedModels() { return models[0].size() + models[1].size(); }

size_t Renderer::loadedShaders() { return shaders.size(); }

size_t Renderer::loadedTextures() { return textures.size(); }

IOmanager& Renderer::getIO() { return io; }

int Renderer::getMaxMemoryAllocationCount() { return e.c.deviceData.maxMemoryAllocationCount; }

int Renderer::getMemAllocObjects() { return e.c.memAllocObjects; }
