-- 查找 spdlog 库， 类似于 cmake 中的 find_package() 函数
add_requires("spdlog", {system = true})

set_toolchains("gcc")

-- global definations
-- add_defines("USE_SELF_DEFINED_CLEAR_COLOR")
add_defines("BUG_FIXES")
add_defines("EXPLICITLY_TRANSITIONNG_DEPTH_IMAGE")
add_defines("VERTEX_DEDUPLICATION")

-- debug log print
if is_mode("debug") then
    add_defines("LOG_DEBUG")
end

if is_mode("debug") then
    set_symbols("debug")
    set_optimize("none")
    add_defines("DEBUG")
    set_strip("none")
    add_cxflags("-g")
    add_ldflags("-g")
end

target("loading_models")
    set_kind("binary")
    add_files("tinyobj_demo.cpp")
    add_includedirs("./thrity_part")
    add_packages("spdlog::spdlog")

    add_links("glfw", "glad", "pthread", "vulkan")
