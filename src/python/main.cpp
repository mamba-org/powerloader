#include <pybind11/iostream.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "downloader.hpp"

namespace py = pybind11;

PYBIND11_MODULE(pypowerloader, m)
{
    using namespace powerloader;

    m.def("hello_world", []() { std::cout << "Hello world!" << std::endl; });

    py::class_<fs::path>(m, "Path")
        .def(py::init<std::string>())
        .def("__str__", [](fs::path& self) -> std::string { return self.string(); })
        .def("__repr__", [](fs::path& self) -> std::string {
            return std::string("fs::path[") + std::string(self) + "]";
        });
    py::implicitly_convertible<std::string, fs::path>();

    py::class_<DownloadTarget, std::shared_ptr<DownloadTarget>>(m, "DownloadTarget")
        .def(py::init<const std::string&, const std::string&, const fs::path&>())
        .def_readonly("complete_url", &DownloadTarget::complete_url);

    py::class_<Mirror, std::shared_ptr<Mirror>>(m, "Mirror").def(py::init<const std::string&>());

    py::class_<Downloader>(m, "Downloader")
        .def(py::init<>())
        .def("download", &Downloader::download)
        .def("add", &Downloader::add);
}
