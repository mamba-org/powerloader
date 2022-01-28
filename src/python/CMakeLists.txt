find_package(pybind11)
pybind11_add_module(pypowerloader main.cpp)
target_link_libraries(pypowerloader PRIVATE libpowerloader)
