#include <stdexcept>
#include <iostream>
#include <utility>

#include "polygonum/input.hpp"


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
	
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);
}

void IOmanager::setCallbacks()
{
	glfwSetWindowUserPointer(window, this);   // Set this class as windowUserPointer (for making it accessible from callbacks)

	glfwSetFramebufferSizeCallback(window, framebufferResize_callback);   // Set callback (signals famebuffer resizing)
	glfwSetScrollCallback(window, mouseScroll_callback);   // Set callback (get mouse scrolling)
	glfwSetMouseButtonCallback(window, mouse_callback);   // Set callback (get mouse just performed an action: press or release)
	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, cursorPos_callback);
	glfwSetCursorEnterCallback(window, cursorEnter_callback);
}

void IOmanager::createWindowSurface(VkInstance instance, VkAllocationCallbacks* allocator, VkSurfaceKHR* surface)
{
	if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS)
		throw std::runtime_error("Failed to create window surface!");
}

std::array<int, 2> IOmanager::getResolution()
{
	std::array<int, 2> resolution; // width, height
	glfwGetFramebufferSize(window, &resolution[0], &resolution[1]);
	return resolution;
}

float IOmanager::getAspectRatio()
{
	auto [width, height] = getResolution();
	return static_cast<float>(width) / height;
}

void IOmanager::setWindowShouldClose(bool b) { return glfwSetWindowShouldClose(window, b); }

bool IOmanager::getWindowShouldClose() { return glfwWindowShouldClose(window); }

bool IOmanager::isWindowMinimized()
{
	return glfwGetWindowAttrib(window, GLFW_ICONIFIED);
}

void IOmanager::destroy()
{
	glfwDestroyWindow(window);		// GLFW window
	glfwTerminate();				// GLFW
}

bool IOmanager::isKeyPressed(int key) const
{
	return keyPressed.find(key) != keyPressed.end();
	//return glfwGetKey(window, key) == GLFW_PRESS;
}

bool IOmanager::isKeyReleased(int key) const
{
	return keyPressed.find(key) == keyPressed.end();
	//return glfwGetKey(window, key) == GLFW_RELEASE;
}

bool IOmanager::isMouseButtonPressed(int button) const
{
	return MBPressed.find(button) != MBPressed.end();
	//return glfwGetMouseButton(window, button) == GLFW_PRESS;
}

bool IOmanager::isMouseButtonReleased(int button) const
{
	return MBPressed.find(button) == MBPressed.end();
	//return glfwGetMouseButton(window, button) == GLFW_RELEASE;
}

std::array<double, 2> IOmanager::getCursorPos() const
{
	return cursorPos;
	// glfwGetCursorPos(window, xPos, yPos);
}

std::array<double, 2> IOmanager::getCursorDelta()
{
	return std::exchange(cursorDelta, {0.f, 0.f});
}

bool IOmanager::cursorEntersWindow() const
{
	return cursorEnteredWindow;
}

void IOmanager::setInputMode(int mode, int value) { glfwSetInputMode(window, mode, value); }

void IOmanager::pollEvents() { glfwPollEvents(); }

void IOmanager::waitEvents() { glfwWaitEvents(); }

float IOmanager::getYscrollOffset()
{
	return std::exchange(YscrollOffset, 0);
}

bool IOmanager::wasMouseButtonJustPressed(int button)
{
	if (MBJustPressed.find(button) == MBJustPressed.end())
		return false;

	MBJustPressed.erase(button);
	return true;
}

bool IOmanager::wasMouseButtonJustReleased(int button)
{
	if (MBJustReleased.find(button) == MBJustReleased.end())
		return false;

	MBJustReleased.erase(button);
	return true;
}

bool IOmanager::wasKeyJustPressed(int button)
{
	if (keyJustPressed.find(button) == keyJustPressed.end())
		return false;

	keyJustPressed.erase(button);
	return true;
}

bool IOmanager::wasKeyJustReleased(int button)
{
	if (keyJustReleased.find(button) == keyJustReleased.end())
		return false;

	keyJustReleased.erase(button);
	return true;
}

bool IOmanager::wasFramebufferJustResized()
{
	return std::exchange(framebufferResized, false);
}

void IOmanager::framebufferResize_callback(GLFWwindow* window, int width, int height)
{
	auto windowUserPointer = reinterpret_cast<IOmanager*>(glfwGetWindowUserPointer(window));
	windowUserPointer->framebufferResized = true;
}

void IOmanager::mouseScroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	auto windowUserPointer = reinterpret_cast<IOmanager*>(glfwGetWindowUserPointer(window));
	windowUserPointer->YscrollOffset = yoffset;
}

void IOmanager::mouse_callback(GLFWwindow* window, int button, int action, int mods)
{
	auto windowUserPointer = reinterpret_cast<IOmanager*>(glfwGetWindowUserPointer(window));

	switch (action)
	{
	case GLFW_PRESS:
		windowUserPointer->MBPressed.insert(button);
		windowUserPointer->MBJustPressed.insert(button);
		return;

	case GLFW_RELEASE:
		windowUserPointer->MBPressed.erase(button);
		windowUserPointer->MBJustReleased.insert(button);
		return;
	}		
}

void IOmanager::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	auto windowUserPointer = reinterpret_cast<IOmanager*>(glfwGetWindowUserPointer(window));

	switch (action)
	{
	case GLFW_PRESS:
		windowUserPointer->keyPressed.insert(key);
		windowUserPointer->keyJustPressed.insert(key);
		return;

	case GLFW_RELEASE:
		windowUserPointer->keyPressed.erase(key);
		windowUserPointer->keyJustReleased.insert(key);
		return;
	}
}

void IOmanager::cursorPos_callback(GLFWwindow* window, double xpos, double ypos)
{
	auto windowUserPointer = reinterpret_cast<IOmanager*>(glfwGetWindowUserPointer(window));

	windowUserPointer->cursorDelta[0] += xpos - windowUserPointer->cursorPos[0];
	windowUserPointer->cursorDelta[1] += ypos - windowUserPointer->cursorPos[1];

	windowUserPointer->cursorPos = { xpos, ypos };
}

void IOmanager::cursorEnter_callback(GLFWwindow* window, int entered)
{
	auto windowUserPointer = reinterpret_cast<IOmanager*>(glfwGetWindowUserPointer(window));

	windowUserPointer->cursorEnteredWindow = entered;
}