CMAKE_MINIMUM_REQUIRED(VERSION 3.12)

if( UNIX )
	#set(CMAKE_CXX_COMPILER "/usr/bin/c++")
	#set(CMAKE_C_COMPILER "gcc-9")
	#set(CMAKE_CXX_COMPILER "/usr/bin/g++-9")
endif()

PROJECT(example_1
	VERSION 1.0
	DESCRIPTION "Basic triangle"
	LANGUAGES CXX
	)

set(appConfig "Debug")
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

ADD_EXECUTABLE( ${PROJECT_NAME}
	src/main.cpp
)

TARGET_SOURCES(${PROJECT_NAME} PRIVATE
	#../../extern/imgui/imgui-1.72b/imgui.cpp
	#../../extern/imgui/imgui-1.72b/imgui_demo.cpp
	#../../extern/imgui/imgui-1.72b/imgui_draw.cpp
	#../../extern/imgui/imgui-1.72b/imgui_widgets.cpp
	#../../extern/imgui/imgui-1.72b/examples/imgui_impl_glfw.cpp
	#../../extern/imgui/imgui-1.72b/examples/imgui_impl_opengl3.cpp
)

TARGET_INCLUDE_DIRECTORIES( ${PROJECT_NAME} PUBLIC
	include
	C:/VulkanSDK/1.3.280.0/Include
	../Polygonum/include
	../../extern/glfw/glfw-3.3.2/include
	../../extern/assimp/include
	../../_BUILD/extern/assimp/include/
	#../../extern/stb
)

if( UNIX )
	#TARGET_INCLUDE_DIRECTORIES( ${PROJECT_NAME} PUBLIC /usr/include/c++/9 )

	TARGET_LINK_LIBRARIES( ${PROJECT_NAME} 
		${PROJECT_SOURCE_DIR}/../../_BUILD/extern/glfw/glfw-3.3.2/src/libglfw3.a
		${PROJECT_SOURCE_DIR}/../../_BUILD/extern/glm/glm-0.9.9.5/glm/libglm_static.a
		C:/VulkanSDK/1.3.275.0/Lib/shaderc_combined.lib
		-lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi #-lglfw 
	)
elseif( WIN32 )
	if(appConfig STREQUAL "Debug")
		TARGET_LINK_LIBRARIES( ${PROJECT_NAME} 
			#Vulkan::Vulkan
			C:/VulkanSDK/1.3.280.0/Lib/vulkan-1.lib
			C:/VulkanSDK/1.3.280.0/Lib/shaderc_combinedd.lib

			${PROJECT_SOURCE_DIR}/../../_BUILD/projects/Polygonum/${appConfig}/polygonum.lib
			${PROJECT_SOURCE_DIR}/../../_BUILD/extern/glfw/glfw-3.3.2/src/${appConfig}/glfw3.lib
			${PROJECT_SOURCE_DIR}/../../_BUILD/extern/assimp/lib/${appConfig}/assimp-vc143-mtd.lib
			${PROJECT_SOURCE_DIR}/../../_BUILD/extern/assimp/contrib/zlib/${appConfig}/zlibstaticd.lib
		)
	elseif( appConfig STREQUAL "Release" )
		TARGET_LINK_LIBRARIES( ${PROJECT_NAME} 
			#Vulkan::Vulkan
			C:/VulkanSDK/1.3.280.0/Lib/vulkan-1.lib
			C:/VulkanSDK/1.3.280.0/Lib/shaderc_combined.lib

			${PROJECT_SOURCE_DIR}/../../_BUILD/projects/Renderer/${appConfig}/polygonum.lib
			${PROJECT_SOURCE_DIR}/../../_BUILD/extern/glfw/glfw-3.3.2/src/${appConfig}/glfw3.lib
			${PROJECT_SOURCE_DIR}/../../_BUILD/extern/assimp/lib/${appConfig}/assimp-vc143-mt.lib
			${PROJECT_SOURCE_DIR}/../../_BUILD/extern/assimp/contrib/zlib/${appConfig}/zlibstatic.lib
		)
	endif()
endif()











