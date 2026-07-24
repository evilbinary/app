arch_type=get_arch_type()
if arch_type in['armv7-a','x86']:

    target("scheme")
    set_type("cli")
    set_filename("scheme")

    add_deps("chez")

    add_files('main.c')
