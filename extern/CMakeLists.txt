# cd /home/username/dev/visual/_BUILD
# cmake -G"Unix Makefiles" ..
# cmake -G"Visual Studio 15 2017 Win64" ..
# cmake -G"Visual Studio 15 2017 Win32" ..

IF("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
MESSAGE(FATAL_ERROR "FATAL: In-source builds are not allowed. You should create a separate directory for build files.")
ENDIF()

set(CMAKE_USE_RELATIVE_PATHS ON)
set(CMAKE_SUPPRESS_REGENERATION ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

CMAKE_MINIMUM_REQUIRED (VERSION 3.12)
PROJECT (in_source_dependencies)

# Dependencies
MESSAGE(STATUS "[GLFW] ---------------")		# Start building GLFW (https://stackoverflow.com/questions/35744647/disabling-cmake-option-has-no-effect)
ADD_SUBDIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/glfw/glfw-3.3.2)
MESSAGE(STATUS "[Assimp] ---------------")
ADD_SUBDIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/assimp)

# Main project
#SET(PROJ_NAME "Vk_1")
#ADD_SUBDIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/projects/${PROJ_NAME})
