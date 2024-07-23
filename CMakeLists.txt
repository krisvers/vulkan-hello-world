cmake_minimum_required(VERSION 3.10)
project(vulkan_hello_world)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(vulkan_hello_world ${SOURCES})

target_link_libraries(vulkan_hello_world Vulkan::Vulkan)
include_directories(${Vulkan_INCLUDE_DIRS} "include")

target_link_libraries(vulkan_hello_world glfw)
include_directories(${GLFW_INCLUDE_DIRS})

if (LINUX)
	target_link_libraries(vulkan_hello_world X11)
elseif (APPLE)
	target_link_libraries(vulkan_hello_world "-framework Cocoa")
	target_link_libraries(vulkan_hello_world "-framework QuartzCore")
endif()


set_property(TARGET vulkan_hello_world PROPERTY CXX_STANDARD 17)
