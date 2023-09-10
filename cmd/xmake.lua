target("cat")
    set_type("cli")

    add_files("cat.c") 

target("date")
    set_type("cli")
    add_files("date.c") 

target("echo")
    set_type("cli")
    add_files("echo.c") 

target("hexdump")
    set_type("cli")
    add_files("hexdump.c") 


target("kill")
    set_type("cli")
    add_files("kill.c") 

target("ls")
    set_type("cli")
    add_files("ls.c") 

target("shell")
    set_type("cli")
    add_files("shell.c") 


target("touch")
    set_type("cli")
    add_files("touch.c") 