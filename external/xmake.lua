
target("test_http")
    add_packages("ilias", "fmt")
    set_kind("binary")
    set_default(false)
    add_files("test.cpp")
target_end()