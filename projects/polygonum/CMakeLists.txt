CMAKE_MINIMUM_REQUIRED(VERSION 3.12)

if( UNIX )
	#set(CMAKE_CXX_COMPILER "/usr/bin/c++")
	#set(CMAKE_C_COMPILER "gcc-9")
	#set(CMAKE_CXX_COMPILER "/usr/bin/g++-9")
endif()

PROJECT(polygonum
	VERSION 1.0
	DESCRIPTION "Low level Vulkan renderer"
	LANGUAGES CXX
	)

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_GENERATOR_PLATFORM x64)
FIND_PACKAGE(Vulkan REQUIRED)

MESSAGE(STATUS "Project: " ${PROJECT_NAME})

ADD_DEFINITIONS(
	-std=c++17
	#-O2		# Optimization incompatible with Debug mode ('/O2' and '/RTC1' are incompatible)
	#-O3
)
#ADD_COMPILE_DEFINITIONS( IMGUI_IMPL_OPENGL_LOADER_GLEW=1 )
#ADD_COMPILE_DEFINITIONS( IMGUI_IMPL_OPENGL_LOADER_GLAD=1 )

ADD_LIBRARY(${PROJECT_NAME} STATIC
	src/main.cpp

	src/environment.cpp
	src/renderer.cpp
	src/models.cpp
	src/vertex.cpp
	src/importer.cpp
	src/ubo.cpp
	src/input.cpp
	src/timer.cpp
	src/toolkit.cpp
	src/physics.cpp
	src/commons.cpp
	src/ecs.cpp

	include/polygonum/environment.hpp
	include/polygonum/renderer.hpp
	include/polygonum/models.hpp
	include/polygonum/vertex.hpp
	include/polygonum/importer.hpp
	include/polygonum/ubo.hpp
	include/polygonum/input.hpp
	include/polygonum/timer.hpp
	include/polygonum/toolkit.hpp
	include/polygonum/physics.hpp
	include/polygonum/commons.hpp
	include/polygonum/ecs.hpp
)

TARGET_INCLUDE_DIRECTORIES( ${PROJECT_NAME} PUBLIC
	include
	C:/VulkanSDK/1.3.280.0/Include
	../../extern/glfw/glfw-3.3.2/include
	../../extern/stb
	../../extern/assimp/include
	../../_BUILD/extern/assimp/include/
)








