#include <pybind11/iostream.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "downloader.hpp"

namespace py = pybind11;

PYBIND11_MODULE(pypowerloader, m)
{
	using namespace powerloader;

	m.def("hello_world", []() {
		std::cout << "Hello world!" << std::endl;
	});
}