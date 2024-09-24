#ifndef INPUT_HPP
#define INPUT_HPP

#include "vulkan/vulkan.h"			// From LunarG SDK. Can be used for off-screen rendering
//#define GLFW_INCLUDE_VULKAN		// Makes GLFW load the Vulkan header with it
#include "GLFW/glfw3.h"

/**
	@brief Input/Output manager for input (controls) and output (window) operations. 
	This holds input callbacks and serves as windowUserPointer (pointer accessible from callbacks). 
	Each window has a windowUserPointer, which can be used for any purpose, and GLFW will not modify 
	it throughout the life-time of the window.
*/
class IOmanager
{
	void initWindow(int width, int height);
	void setCallbacks();
	float YscrollOffset = 0;     //!< Set in a callback (windowUserPointer)

public:
	IOmanager(int width, int height);
	~IOmanager();

	GLFWwindow* window;				//!< Opaque window object.

	// Output (window)
	void createWindowSurface(VkInstance instance, VkAllocationCallbacks* allocator, VkSurfaceKHR* surface);
	void getFramebufferSize(int* width, int* height);
	float getAspectRatio();
	void setWindowShouldClose(bool b);
	bool getWindowShouldClose();
	void destroy();

	// Input (keys, mouse)
	int getKey(int key);
	int getMouseButton(int button);
	void getCursorPos(double* xpos, double* ypos);
	void setInputMode(int mode, int value);
	void pollEvents();							//!< Check for events (processes only those events that have already been received and then returns immediately)
	void waitEvents();

	// Callbacks
	float getYscrollOffset();			//!< Get YscrollOffset value and reset it (set to 0).
	bool framebufferResized = false;	//!< Many drivers/platforms trigger VK_ERROR_OUT_OF_DATE_KHR after window resize, but it's not guaranteed. This variable handles resizes explicitly.
	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);		//!< Callback for window resizing (called when window is resized).
	static void mouseScroll_callback(GLFWwindow* window, double xoffset, double yoffset);	//!< Callback for mouse scroll (called when mouse is scrolled).
};

#endif