-- 查找 spdlog 库， 类似于 cmake 中的 find_package() 函数
add_requires("spdlog", {system = true})

if is_mode("debug") then
    set_symbols("debug")
    set_optimize("none")
    add_defines("DEBUG")
    set_strip("none")
    add_cxflags("-g")
    add_ldflags("-g")
end

target("image_views")
    set_kind("binary")
    add_files("main.cpp")

    add_packages("spdlog::spdlog")

    add_links("glfw", "glad", "pthread", "vulkan")

    set_rundir("$(projectdir)/build")  -- 项目根目录下的 bin 文件夹