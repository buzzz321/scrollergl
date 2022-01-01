add_rules("mode.debug", "mode.release")

add_requires("glfw")
target("scrollgl")
    set_kind("binary")
    add_files("*.cc")
    add_files("*.c")
    add_packages("glfw")
    if is_mode("debug") then
        add_defines("DEBUG")
    end
