target("lua.elf")
    set_type("cli")
    set_filename("lua")

    add_deps("lua")

    add_files("lua.c")
