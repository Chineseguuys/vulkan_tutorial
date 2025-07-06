-- 查找 spdlog 库， 类似于 cmake 中的 find_package() 函数
add_requires("spdlog", {system = true})

-- global definations
--add_defines("USE_SELF_DEFINED_CLEAR_COLOR")

-- debug log print
add_defines("LOG_DEBUG")
--add_defines("USING_GLFW")
--add_defines("USING_SDL2")
add_defines("USING_XCB")

if is_mode("debug") then
    set_symbols("debug")
    set_optimize("none")
    add_defines("DEBUG")
    set_strip("none")
    add_cxflags("-g")
    add_ldflags("-g")
end

target("rendering")
    set_kind("binary")
    add_files("main.cpp")

    add_packages("spdlog::spdlog")
    add_packages("SDL2")

    add_links("glfw", "glad", "pthread", "vulkan", "SDL2")
    add_links("xcb")
