target("gui.elf")
    set_type("cli")
    set_filename("gui")

    add_deps("gui","image","jpeg","png","zlib")

    add_files("main.c") 

