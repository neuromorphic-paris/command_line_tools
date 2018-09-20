#include "../../third_party/sepia/source/sepia.hpp"
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>

#include <iostream> // @DEBUG

/// count determines the number of events in the given stream.
template <sepia::type event_stream_type>
std::size_t count(std::unique_ptr<std::istream> stream) {
    std::size_t events = 0;
    sepia::join_observable<event_stream_type>(std::move(stream), [&](sepia::event<event_stream_type>) { ++events; });
    return events;
}

/// read loads events from an Event Stream file.
static PyObject* read(PyObject* self, PyObject* args) {
    const char* filename_as_char_array;
    if (!PyArg_ParseTuple(args, "s", &filename_as_char_array)) {
        return nullptr;
    }
    auto events = PyDict_New();
    try {
        const std::string filename(filename_as_char_array);
        const auto header = sepia::read_header(sepia::filename_to_ifstream(filename));
        switch (header.event_stream_type) {
            case sepia::type::generic: {
                auto number_of_events =
                    static_cast<npy_intp>(count<sepia::type::generic>(sepia::filename_to_ifstream(filename)));
                auto t = PyArray_SimpleNew(1, &number_of_events, NPY_UINT64);
                auto bytes = PyArray_SimpleNew(1, &number_of_events, NPY_OBJECT);
                std::size_t index = 0;
                sepia::join_observable<sepia::type::generic>(
                    sepia::filename_to_ifstream(filename), [&](sepia::generic_event generic_event) {
                        *reinterpret_cast<uint64_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(t), index)) =
                            generic_event.t;
                        *reinterpret_cast<PyObject**>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(bytes), index)) =
                            PyBytes_FromStringAndSize(
                                reinterpret_cast<const char*>(generic_event.bytes.data()), generic_event.bytes.size());
                        ++index;
                    });
                PyDict_SetItem(events, PyUnicode_FromString("type"), PyUnicode_FromString("generic"));
                PyDict_SetItem(events, PyUnicode_FromString("t"), t);
                PyDict_SetItem(events, PyUnicode_FromString("bytes"), bytes);
                break;
            }
            case sepia::type::dvs: {
                auto number_of_events =
                    static_cast<npy_intp>(count<sepia::type::dvs>(sepia::filename_to_ifstream(filename)));
                auto t = PyArray_SimpleNew(1, &number_of_events, NPY_UINT64);
                auto x = PyArray_SimpleNew(1, &number_of_events, NPY_UINT16);
                auto y = PyArray_SimpleNew(1, &number_of_events, NPY_UINT16);
                auto is_increase = PyArray_SimpleNew(1, &number_of_events, NPY_BOOL);
                std::size_t index = 0;
                sepia::join_observable<sepia::type::dvs>(
                    sepia::filename_to_ifstream(filename), [&](sepia::dvs_event dvs_event) {
                        *reinterpret_cast<uint64_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(t), index)) =
                            dvs_event.t;
                        *reinterpret_cast<uint16_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(x), index)) =
                            dvs_event.x;
                        *reinterpret_cast<uint16_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(y), index)) =
                            dvs_event.y;
                        *reinterpret_cast<bool*>(PyArray_GETPTR1(
                            reinterpret_cast<PyArrayObject*>(is_increase), index)) = dvs_event.is_increase;
                        ++index;
                    });
                PyDict_SetItem(events, PyUnicode_FromString("type"), PyUnicode_FromString("dvs"));
                PyDict_SetItem(events, PyUnicode_FromString("width"), PyLong_FromUnsignedLong(header.width));
                PyDict_SetItem(events, PyUnicode_FromString("height"), PyLong_FromUnsignedLong(header.height));
                PyDict_SetItem(events, PyUnicode_FromString("t"), t);
                PyDict_SetItem(events, PyUnicode_FromString("x"), x);
                PyDict_SetItem(events, PyUnicode_FromString("y"), y);
                PyDict_SetItem(events, PyUnicode_FromString("is_increase"), is_increase);
                break;
            }
            case sepia::type::atis: {
                auto number_of_events =
                    static_cast<npy_intp>(count<sepia::type::atis>(sepia::filename_to_ifstream(filename)));
                auto t = PyArray_SimpleNew(1, &number_of_events, NPY_UINT64);
                auto x = PyArray_SimpleNew(1, &number_of_events, NPY_UINT16);
                auto y = PyArray_SimpleNew(1, &number_of_events, NPY_UINT16);
                auto is_threshold_crossing = PyArray_SimpleNew(1, &number_of_events, NPY_BOOL);
                auto polarity = PyArray_SimpleNew(1, &number_of_events, NPY_BOOL);
                std::size_t index = 0;
                sepia::join_observable<sepia::type::atis>(
                    sepia::filename_to_ifstream(filename), [&](sepia::atis_event atis_event) {
                        *reinterpret_cast<uint64_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(t), index)) =
                            atis_event.t;
                        *reinterpret_cast<uint16_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(x), index)) =
                            atis_event.x;
                        *reinterpret_cast<uint16_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(y), index)) =
                            atis_event.y;
                        *reinterpret_cast<bool*>(
                            PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(is_threshold_crossing), index)) =
                            atis_event.is_threshold_crossing;
                        *reinterpret_cast<bool*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(polarity), index)) =
                            atis_event.polarity;
                        ++index;
                    });
                PyDict_SetItem(events, PyUnicode_FromString("type"), PyUnicode_FromString("atis"));
                PyDict_SetItem(events, PyUnicode_FromString("width"), PyLong_FromUnsignedLong(header.width));
                PyDict_SetItem(events, PyUnicode_FromString("height"), PyLong_FromUnsignedLong(header.height));
                PyDict_SetItem(events, PyUnicode_FromString("t"), t);
                PyDict_SetItem(events, PyUnicode_FromString("x"), x);
                PyDict_SetItem(events, PyUnicode_FromString("y"), y);
                PyDict_SetItem(events, PyUnicode_FromString("is_threshold_crossing"), is_threshold_crossing);
                PyDict_SetItem(events, PyUnicode_FromString("polarity"), polarity);
                break;
            }
            case sepia::type::color: {
                auto number_of_events =
                    static_cast<npy_intp>(count<sepia::type::color>(sepia::filename_to_ifstream(filename)));
                auto t = PyArray_SimpleNew(1, &number_of_events, NPY_UINT64);
                auto x = PyArray_SimpleNew(1, &number_of_events, NPY_UINT16);
                auto y = PyArray_SimpleNew(1, &number_of_events, NPY_UINT16);
                auto r = PyArray_SimpleNew(1, &number_of_events, NPY_UINT8);
                auto g = PyArray_SimpleNew(1, &number_of_events, NPY_UINT8);
                auto b = PyArray_SimpleNew(1, &number_of_events, NPY_UINT8);
                std::size_t index = 0;
                sepia::join_observable<sepia::type::color>(
                    sepia::filename_to_ifstream(filename), [&](sepia::color_event color_event) {
                        *reinterpret_cast<uint64_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(t), index)) =
                            color_event.t;
                        *reinterpret_cast<uint16_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(x), index)) =
                            color_event.x;
                        *reinterpret_cast<uint16_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(y), index)) =
                            color_event.y;
                        *reinterpret_cast<uint8_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(r), index)) =
                            color_event.r;
                        *reinterpret_cast<uint8_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(g), index)) =
                            color_event.g;
                        *reinterpret_cast<uint8_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(b), index)) =
                            color_event.b;
                        ++index;
                    });
                PyDict_SetItem(events, PyUnicode_FromString("type"), PyUnicode_FromString("color"));
                PyDict_SetItem(events, PyUnicode_FromString("width"), PyLong_FromUnsignedLong(header.width));
                PyDict_SetItem(events, PyUnicode_FromString("height"), PyLong_FromUnsignedLong(header.height));
                PyDict_SetItem(events, PyUnicode_FromString("t"), t);
                PyDict_SetItem(events, PyUnicode_FromString("x"), x);
                PyDict_SetItem(events, PyUnicode_FromString("y"), y);
                PyDict_SetItem(events, PyUnicode_FromString("r"), r);
                PyDict_SetItem(events, PyUnicode_FromString("g"), g);
                PyDict_SetItem(events, PyUnicode_FromString("b"), b);
                break;
            }
            default:
                break;
        }
    } catch (const std::runtime_error& exception) {
        PyErr_SetString(PyExc_RuntimeError, exception.what());
        events = nullptr;
    }
    return events;
}

/// get_array loads a PyArray from a dict.
static PyArrayObject* get_array(PyObject* dict, const std::string& key, int32_t type, const std::string& type_name) {
    auto object = PyDict_GetItem(dict, PyUnicode_FromString(key.c_str()));
    if (!object) {
        throw std::runtime_error(std::string("the dict must have a '") + key + "' key");
    }
    if (!PyArray_Check(object) || PyArray_TYPE(reinterpret_cast<PyArrayObject*>(object)) != type) {
        throw std::runtime_error(std::string("'") + key + "' must be " + type_name + " numpy array");
    }
    auto array = reinterpret_cast<PyArrayObject*>(object);
    if (PyArray_NDIM(array) != 1) {
        throw std::runtime_error(std::string("'") + key + "' must have a single dimension");
    }
    return array;
}

/// width_and_height loads the width and height keys from a dict.
static std::pair<uint16_t, uint16_t> get_width_and_height(PyObject* dict) {
    std::pair<uint16_t, uint16_t> width_and_height;
    auto width = PyDict_GetItem(dict, PyUnicode_FromString("width"));
    if (!width) {
        throw std::runtime_error("the dict must have a 'width' key");
    }
    if (!PyLong_Check(width)) {
        throw std::runtime_error("'width' must be a long integer");
    }
    const auto width_as_long = PyLong_AsLong(width);
    width_and_height.first = static_cast<uint16_t>(width_as_long);
    if (width_and_height.first != width_as_long) {
        throw std::runtime_error("'width' must be in the range [0, 65535]");
    }
    auto height = PyDict_GetItem(dict, PyUnicode_FromString("height"));
    if (!height) {
        throw std::runtime_error("the dict must have a 'height' key");
    }
    if (!PyLong_Check(height)) {
        throw std::runtime_error("'height' must be a long integer");
    }
    const auto height_as_long = PyLong_AsLong(height);
    width_and_height.second = static_cast<uint16_t>(height_as_long);
    if (width_and_height.second != height_as_long) {
        throw std::runtime_error("'height' must be in the range [0, 65535]");
    }
    return width_and_height;
}

/// write stores events to an Event Stream file.
static PyObject* write(PyObject* self, PyObject* args) {
    PyObject* events;
    const char* filename_as_char_array;
    if (!PyArg_ParseTuple(args, "O!s", &PyDict_Type, &events, &filename_as_char_array)) {
        return nullptr;
    }
    try {
        auto type = PyDict_GetItem(events, PyUnicode_FromString("type"));
        if (!type) {
            throw std::runtime_error("the dict must have a 'type' key");
        }
        if (!PyUnicode_Check(type)) {
            throw std::runtime_error("'type' must be a unicode string");
        }
        auto type_as_python_string = PyUnicode_AsEncodedString(type, "utf-8", "strict");
        const std::string type_as_string(PyBytes_AsString(type_as_python_string));
        Py_DECREF(type_as_python_string);
        if (type_as_string == "generic") {
            auto t = get_array(events, "t", NPY_UINT64, "a uint64");
            auto bytes = get_array(events, "bytes", NPY_OBJECT, "an object");
            if (PyArray_SIZE(t) != PyArray_SIZE(bytes)) {
                throw std::runtime_error("'t' and 'bytes' must have the same size");
            }
            sepia::write<sepia::type::generic> write(sepia::filename_to_ofstream(std::string(filename_as_char_array)));
            for (std::size_t index = 0; index < static_cast<std::size_t>(PyArray_SIZE(t)); ++index) {
                auto payload =
                    *reinterpret_cast<PyObject**>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(bytes), index));
                if (!PyBytes_Check(payload)) {
                    throw std::runtime_error("'bytes' elements must be bytes strings");
                }
                std::string bytes_as_string(PyBytes_AsString(payload));
                write({*reinterpret_cast<uint64_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(t), index)),
                       std::vector<uint8_t>(bytes_as_string.begin(), bytes_as_string.end())});
            }
        } else if (type_as_string == "dvs") {
            const auto width_and_height = get_width_and_height(events);
            auto t = get_array(events, "t", NPY_UINT64, "a uint64");
            auto x = get_array(events, "x", NPY_UINT16, "a uint16");
            auto y = get_array(events, "y", NPY_UINT16, "a uint16");
            auto is_increase = get_array(events, "is_increase", NPY_BOOL, "a bool");
            if (PyArray_SIZE(t) != PyArray_SIZE(x) || PyArray_SIZE(t) != PyArray_SIZE(y)
                || PyArray_SIZE(t) != PyArray_SIZE(is_increase)) {
                throw std::runtime_error("'t', 'x', 'y' and 'is_increase' must have the same size");
            }
            sepia::write<sepia::type::dvs> write(
                sepia::filename_to_ofstream(std::string(filename_as_char_array)),
                width_and_height.first,
                width_and_height.second);
            for (std::size_t index = 0; index < static_cast<std::size_t>(PyArray_SIZE(t)); ++index) {
                write(
                    {*reinterpret_cast<uint64_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(t), index)),
                     *reinterpret_cast<uint16_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(x), index)),
                     *reinterpret_cast<uint16_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(y), index)),
                     *reinterpret_cast<bool*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(is_increase), index))});
            }
        } else if (type_as_string == "atis") {
            const auto width_and_height = get_width_and_height(events);
            auto t = get_array(events, "t", NPY_UINT64, "a uint64");
            auto x = get_array(events, "x", NPY_UINT16, "a uint16");
            auto y = get_array(events, "y", NPY_UINT16, "a uint16");
            auto is_threshold_crossing = get_array(events, "is_threshold_crossing", NPY_BOOL, "a bool");
            auto polarity = get_array(events, "polarity", NPY_BOOL, "a bool");
            if (PyArray_SIZE(t) != PyArray_SIZE(x) || PyArray_SIZE(t) != PyArray_SIZE(y)
                || PyArray_SIZE(t) != PyArray_SIZE(is_threshold_crossing)
                || PyArray_SIZE(t) != PyArray_SIZE(polarity)) {
                throw std::runtime_error(
                    "'t', 'x', 'y', 'is_threshold_crossing' and 'polarity' must have the same size");
            }
            sepia::write<sepia::type::atis> write(
                sepia::filename_to_ofstream(std::string(filename_as_char_array)),
                width_and_height.first,
                width_and_height.second);
            for (std::size_t index = 0; index < static_cast<std::size_t>(PyArray_SIZE(t)); ++index) {
                write({*reinterpret_cast<uint64_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(t), index)),
                       *reinterpret_cast<uint16_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(x), index)),
                       *reinterpret_cast<uint16_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(y), index)),
                       *reinterpret_cast<bool*>(
                           PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(is_threshold_crossing), index)),
                       *reinterpret_cast<bool*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(polarity), index))});
            }
        } else if (type_as_string == "color") {
            const auto width_and_height = get_width_and_height(events);
            auto t = get_array(events, "t", NPY_UINT64, "a uint64");
            auto x = get_array(events, "x", NPY_UINT16, "a uint16");
            auto y = get_array(events, "y", NPY_UINT16, "a uint16");
            auto r = get_array(events, "r", NPY_UINT8, "a uint8");
            auto g = get_array(events, "g", NPY_UINT8, "a uint8");
            auto b = get_array(events, "b", NPY_UINT8, "a uint8");
            if (PyArray_SIZE(t) != PyArray_SIZE(x) || PyArray_SIZE(t) != PyArray_SIZE(y)
                || PyArray_SIZE(t) != PyArray_SIZE(r) || PyArray_SIZE(t) != PyArray_SIZE(g)
                || PyArray_SIZE(t) != PyArray_SIZE(b)) {
                throw std::runtime_error("'t', 'x', 'y', 'r', 'g' and 'b' must have the same size");
            }
            sepia::write<sepia::type::color> write(
                sepia::filename_to_ofstream(std::string(filename_as_char_array)),
                width_and_height.first,
                width_and_height.second);
            for (std::size_t index = 0; index < static_cast<std::size_t>(PyArray_SIZE(t)); ++index) {
                write({*reinterpret_cast<uint64_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(t), index)),
                       *reinterpret_cast<uint16_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(x), index)),
                       *reinterpret_cast<uint16_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(y), index)),
                       *reinterpret_cast<uint8_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(r), index)),
                       *reinterpret_cast<uint8_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(g), index)),
                       *reinterpret_cast<uint8_t*>(PyArray_GETPTR1(reinterpret_cast<PyArrayObject*>(b), index))});
            }
        } else {
            throw std::runtime_error("'type' must be one of {'generic', 'dvs', 'atis', 'color'}");
        }
    } catch (const std::runtime_error& exception) {
        PyErr_SetString(PyExc_RuntimeError, exception.what());
        PyObject* result = nullptr;
        return result;
    }
    Py_RETURN_NONE;
}

static PyMethodDef eventstream_methods[] = {
    {"read", (PyCFunction)read, METH_VARARGS, "read events from an Event Stream file"},
    {"write", (PyCFunction)write, METH_VARARGS, "write events to an Event Stream file"},
    {nullptr, nullptr, 0, nullptr}};

#if PY_MAJOR_VERSION >= 3
#define PyMODINIT_FUNC_RETURN return module
static struct PyModuleDef eventstream_definition = {PyModuleDef_HEAD_INIT,
                                                    "eventstream",
                                                    "eventstream reads and writes Event Stream files",
                                                    -1,
                                                    eventstream_methods};
PyMODINIT_FUNC PyInit_eventstream() {
    auto module = PyModule_Create(&eventstream_definition);
#else
#define PyMODINIT_FUNC_RETURN return
PyMODINIT_FUNC initeventstream() {
    Py_InitModule("eventstream", eventstream_methods);
#endif
    import_array();
    PyMODINIT_FUNC_RETURN;
}
