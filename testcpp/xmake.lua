target("test-cpp")
    set_type("cli")
    add_deps("cxx","cxxabi")

    add_files("test-cpp.cpp") 