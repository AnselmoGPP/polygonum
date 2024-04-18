#include <stdexcept>

#include "glm/gtc/type_ptr.hpp"

#include "input.hpp"

#include <iostream>
IOmanager::IOmanager(int width, int height)
{
	initWindow(width, height);
	setCallbacks();
}

IOmanager::~IOmanager()
{ 
	#ifdef DEBUG_IMPORT
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif
}

void IOmanager::initWindow(int width, int height)
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);	// Tell GLFW not to create an OpenGL context
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);		// Enable resizable window (default)

	window = glfwCreateWindow((int)width, (int)height, "Grapho", nullptr, nullptr);
	//glfwSetWindowUserPointer(window, this);								// Input class has been set as windowUserPointer
	//glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);	// This callback has been set in Input

	glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);
}

void IOmanager::setCallbacks()
{
	glfwSetWindowUserPointer(window, this);								// Set this class as windowUserPointer (for making it accessible from callbacks)
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);	// Set callback (signals famebuffer resizing)
	glfwSetScrollCallback(window, mouseScroll_callback);				// Set callback (get mouse scrolling)
}

void IOmanager::createWindowSurface(VkInstance instance, VkAllocationCallbacks* allocator, VkSurfaceKHR* surface)
{
	if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS)
		throw std::runtime_error("Failed to create window surface!");
}

void IOmanager::getFramebufferSize(int* width, int* height) { glfwGetFramebufferSize(window, width, height); }

float IOmanager::getAspectRatio()
{
	int width, height;
	getFramebufferSize(&width, &height);
	return (float)width / height;
}

void IOmanager::setWindowShouldClose(bool b) { return glfwSetWindowShouldClose(window, b); }

bool IOmanager::windowShouldClose() { return glfwWindowShouldClose(window); }

void IOmanager::destroy()
{
	glfwDestroyWindow(window);		// GLFW window
	glfwTerminate();				// GLFW
}

int IOmanager::getKey(int key) { return glfwGetKey(window, key); }

int IOmanager::getMouseButton(int button) { return glfwGetMouseButton(window, button); }

void IOmanager::getCursorPos(double* xpos, double* ypos) { glfwGetCursorPos(window, xpos, ypos); }

void IOmanager::setInputMode(int mode, int value) { glfwSetInputMode(window, mode, value); }

void IOmanager::pollEvents() { glfwPollEvents(); }

void IOmanager::waitEvents() { glfwWaitEvents(); }

float IOmanager::getYscrollOffset()
{
	float offset = YscrollOffset;
	YscrollOffset = 0;
	return offset;
}

void IOmanager::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	auto windowUserPointer = reinterpret_cast<IOmanager*>(glfwGetWindowUserPointer(window));
	windowUserPointer->framebufferResized = true;
}

void IOmanager::mouseScroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	auto windowUserPointer = reinterpret_cast<IOmanager*>(glfwGetWindowUserPointer(window));
	windowUserPointer->YscrollOffset = yoffset;
}
