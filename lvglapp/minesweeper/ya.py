target("minesweeper")
set_type("cli")

add_deps("lvgl")
add_files("main.c")

add_files("../app.c", "../app_adapter.c")

add_includedirs(
    "..",
    "include",
    "../include",
    "../../eggs/liblvgl",
    "../../eggs/liblvgl/port"
)
