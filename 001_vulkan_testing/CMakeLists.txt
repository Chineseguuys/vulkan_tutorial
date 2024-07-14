cmake_minimum_required(VERSION 3.16.3)

project(vulkan_testing)

set (CMAKE_EXPORT_COMPILE_COMMANDS ON)
set (CMAKE_VERBOSE_MAKEFILE ON)
set (CMAKE_BUILD_TYPE "Debug")
#set (CMAKE_BUILD_TYPE "Release")
set (CMAKE_CXX_STANDARD 20)
set (USE_OPENMP_BACKEND ON)

find_package(spdlog REQUIRED)

add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCE_DIR}/main.cpp)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        /user/local/include
)

# 未知的原因导致这个目录没有默认在编译器的搜索路径里面
target_link_directories(${PROJECT_NAME}
    PUBLIC
        /usr/local/lib/
)

target_link_libraries(${PROJECT_NAME}
    #GLEW
    glfw
    glad
    GL 
    #X11
    pthread
    Xrandr
    Xi
    vulkan
    spdlog::spdlog
    ${CMAKE_DL_LIBS} # Name of library containing ``dlopen`` and ``dlclose``.
)
