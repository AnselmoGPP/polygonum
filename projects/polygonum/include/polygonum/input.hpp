#ifndef INPUT_HPP
#define INPUT_HPP

#include "polygonum/commons.hpp"

#include <array>

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

	// - Variables set in a callback (via windowUserPointer)

	bool framebufferResized = false;   //!< Many drivers/platforms trigger VK_ERROR_OUT_OF_DATE_KHR after window resize, but it's not guaranteed. This variable handles resizes explicitly.

	std::unordered_set<int> MBPressed, MBJustPressed, MBJustReleased;
	std::unordered_set<int> keyPressed, keyJustPressed, keyJustReleased;
	float YscrollOffset = 0;
	std::array<double, 2> cursorPos = { 0.f, 0.f };   //!< Pixel coordinates. Origin: top-left.
	std::array<double, 2> cursorDelta = { 0.f, 0.f };
	bool cursorEnteredWindow = false;   //!< True (cursor crossed in), False (cursor crossed out)

public:
	IOmanager(int width, int height);
	~IOmanager();

	GLFWwindow* window;				//!< Opaque window object.

	// - Output (window)

	void createWindowSurface(VkInstance instance, VkAllocationCallbacks* allocator, VkSurfaceKHR* surface);
	void setWindowShouldClose(bool b);
	bool getWindowShouldClose();
	bool isWindowMinimized();
	void pollEvents();   //!< Check for events (processes only those events that have already been received and then returns immediately)
	void waitEvents();
	void destroy();

	bool wasFramebufferJustResized(); //!< Resets state <- FOR RENDERER USE ONLY

	void setInputMode(int mode, int value);
	std::array<double, 2> getCursorPos() const; // Get pixel coordinates (origin: top-left)
	std::array<double, 2> getCursorDelta(); // Resets state
	bool cursorEntersWindow() const;

	std::array<int, 2> getResolution(); // Window resolution (pixels)
	float getAspectRatio();

	// - Input (keys, mouse)

	bool isKeyPressed(int key) const;
	bool isKeyReleased(int key) const;
	bool wasKeyJustPressed(int button);   // Resets state
	bool wasKeyJustReleased(int button);   // Resets state

	bool isMouseButtonPressed(int button) const;
	bool isMouseButtonReleased(int button) const;
	bool wasMouseButtonJustPressed(int button);   // Resets state
	bool wasMouseButtonJustReleased(int button);   // Resets state

	float getYscrollOffset();   //!< Get YscrollOffset value and reset it (set to 0).

	// - Callbacks. Called during polling, once per event occurred. Accesses IOmanager from the callbacks via a windowUserPointer.

	static void framebufferResize_callback(GLFWwindow* window, int width, int height);   //!< Callback for window resizing (called when window is resized).
	static void mouseScroll_callback(GLFWwindow* window, double xoffset, double yoffset);   //!< Callback for mouse scroll (called when mouse is scrolled).
	static void mouse_callback(GLFWwindow* window, int button, int action, int mods);   // For mouse buttons
	static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);   //!< For keys
	static void cursorPos_callback(GLFWwindow* window, double xpos, double ypos);   //!< For cursor position
	static void cursorEnter_callback(GLFWwindow* window, int entered);   //!< Check if cursor enters the window
};

#endif