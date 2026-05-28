cmake_minimum_required(VERSION 3.21)
project(VkP2 VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG vulkan-sdk-1.3.296.0
    SOURCE_SUBDIR _not_built
)
FetchContent_MakeAvailable(volk)

FetchContent_Declare(VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.3.0
    SOURCE_SUBDIR _not_built
)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

find_package(Vulkan REQUIRED)

file(GLOB_RECURSE VkP2_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.hpp"
    "include/*.hpp"
    "include/*.h"
)

add_library(VkP2 STATIC
    ${VkP2_SOURCES}
    "${volk_SOURCE_DIR}/volk.c"
)
add_library(VkP2::VkP2 ALIAS VkP2)

target_include_directories(VkP2 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    "$ENV{VULKAN_SDK}/Include"
    "${volk_SOURCE_DIR}"
    "${vulkanmemoryallocator_SOURCE_DIR}/include"
)
target_include_directories(VkP2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include/vkp2
)

target_link_directories(VkP2 PUBLIC
    "$ENV{VULKAN_SDK}/Lib"
)

target_link_libraries(VkP2 PUBLIC
    slang
)

target_compile_definitions(VkP2 PUBLIC VK_NO_PROTOTYPES=1)

if(MSVC)
    target_compile_options(VkP2 PRIVATE /W4)
else()
    target_compile_options(VkP2 PRIVATE -Wall -Wextra)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${VkP2_SOURCES})
