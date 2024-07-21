-- 查找 spdlog 库， 类似于 cmake 中的 find_package() 函数
add_requires("spdlog", {system = true})

target("window_surface")
    set_kind("binary")
    add_files("main.cpp")

    add_packages("spdlog::spdlog")

    add_links("glfw", "glad", "pthread", "vulkan")
