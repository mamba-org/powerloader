#include "powerloader/context.hpp"
#include <pybind11/iostream.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include <powerloader/downloader.hpp>

namespace py = pybind11;

PYBIND11_MODULE(pypowerloader, m)
{
    using namespace powerloader;

    m.def("hello_world", []() { std::cout << "Hello world!" << std::endl; });

    py::class_<fs::path>(m, "Path")
        .def(py::init<std::string>())
        .def("__str__", [](fs::path& self) -> std::string { return self.string(); })
        .def("__repr__",
             [](fs::path& self) -> std::string
             { return std::string("fs::path[") + self.u8string() + "]"; });
    py::implicitly_convertible<std::string, fs::path>();

    py::class_<DownloadTarget, std::shared_ptr<DownloadTarget>>(m, "DownloadTarget")
        .def(py::init<const std::string&, const std::string&, const fs::path&>())
        .def_property_readonly("complete_url", &DownloadTarget::complete_url)
        .def_property("progress_callback",
                      &DownloadTarget::progress_callback,
                      &DownloadTarget::set_progress_callback);

    py::class_<Mirror, std::shared_ptr<Mirror>>(m, "Mirror")
        .def(py::init<const Context&, const std::string&>());

    py::class_<mirror_map_type>(m, "MirrorMap")
        .def(py::init<>())
        .def("get_mirrors", &mirror_map_type::get_mirrors)
        .def("add_unique_mirror", &mirror_map_type::get_mirrors)
        .def("as_dict", [](const mirror_map_type& value) { return value.as_map(); });

    py::class_<Context, std::unique_ptr<Context>>(m, "Context")
        .def(py::init([] { return std::make_unique<Context>(); }))
        .def_readwrite("verbosity", &Context::verbosity)
        .def_readwrite("mirror_map", &Context::mirror_map);

    py::class_<Downloader>(m, "Downloader")
        .def(py::init<const Context&>())
        .def("download", &Downloader::download)
        .def("add", &Downloader::add);
}
