target("infones")
set_type("cli")

add_deps("gui","image","jpeg","png","zlib")

add_files(
    'K6502.c',
    'InfoNES.c',
    'InfoNES_Mapper.c',
    'InfoNES_pAPU.c',
    'InfoNES_System_YiYiYa.c',
    'joypad_input.c'
) 

