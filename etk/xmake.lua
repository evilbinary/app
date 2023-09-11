target("etk.elf")
    set_type("cli")
    set_filename("etk")

    add_deps("etk","gui")

    add_files(
         'main.c',
        'etk_app.c',
        'sinclock.c',
        'mine.c',
        'terminal.c',
        -- # 'light.c',
        'power.c',
        'temhum.c',
        'status.c'
    ) 

    add_includedirs(
        "."
    )