#include "../third_party/sepia/source/sepia.hpp"
#include "mex.hpp"
#include "mexAdapter.hpp"

/// key is used during the validation of the event struct.
struct key {
    matlab::data::ArrayType type;
    std::string type_name;
    bool found;
    std::size_t size;
};

class MexFunction : public matlab::mex::Function {
    public:
    void operator()(matlab::mex::ArgumentList outputs, matlab::mex::ArgumentList inputs) {
        auto engine = getEngine();
        matlab::data::ArrayFactory factory;
        auto error = [&](const std::string& what) {
            engine->feval(
                matlab::engine::convertUTF8StringToUTF16String("error"),
                0,
                std::vector<matlab::data::Array>({factory.createScalar(what)}));
        };
        if (inputs.size() != 3) {
            error("three input arguments are expected");
        }
        if (inputs[0].getType() != matlab::data::ArrayType::CHAR) {
            error("the first input argument must be a char array");
        }
        const matlab::data::CharArray filename_as_char_array = inputs[0];
        const auto filename = filename_as_char_array.toAscii();
        if (inputs[1].getType() != matlab::data::ArrayType::STRUCT) {
            error("the second input argument must be a struct");
        }
        const matlab::data::StructArray matlab_header = inputs[1];
        sepia::header header;
        auto has_type = false;
        for (const auto& field_name : matlab_header.getFieldNames()) {
            if (std::string(field_name) == "type") {
                if (matlab_header[0]["type"].getType() != matlab::data::ArrayType::CHAR) {
                    error("the header type must be a char array");
                }
                const matlab::data::CharArray type_as_char_array = matlab_header[0]["type"];
                const auto type = type_as_char_array.toAscii();
                if (type == "generic") {
                    header.event_stream_type = sepia::type::generic;
                } else if (type == "dvs") {
                    header.event_stream_type = sepia::type::dvs;
                } else if (type == "atis") {
                    header.event_stream_type = sepia::type::atis;
                } else if (type == "color") {
                    header.event_stream_type = sepia::type::color;
                } else {
                    error("the header type must be one of {'generic', 'dvs', 'atis', 'color'}");
                }
                has_type = true;
            } else if (std::string(field_name) == "width") {
                if (matlab_header[0]["width"].getType() != matlab::data::ArrayType::UINT16) {
                    error("the header width must be a uint16");
                }
                const matlab::data::TypedArray<uint16_t> type_as_uint16_array = matlab_header[0]["width"];
                header.width = type_as_uint16_array[0];
                if (header.width == 0) {
                    error("the header width cannot be 0");
                }
            } else if (std::string(field_name) == "height") {
                if (matlab_header[0]["height"].getType() != matlab::data::ArrayType::UINT16) {
                    error("the header height must be a uint16");
                }
                const matlab::data::TypedArray<uint16_t> type_as_uint16_array = matlab_header[0]["height"];
                header.height = type_as_uint16_array[0];
                if (header.height == 0) {
                    error("the header height cannot be 0");
                }
            }
        }
        if (!has_type) {
            error("the header must have a field 'type'");
        }
        if (header.event_stream_type != sepia::type::generic) {
            if (header.width == 0) {
                error("the header must have a field 'width'");
            }
            if (header.height == 0) {
                error("the header must have a field 'height'");
            }
        }
        if (inputs[2].getType() != matlab::data::ArrayType::STRUCT) {
            error("the third input argument must be a struct");
        }
        std::unordered_map<std::string, key> name_to_key{{"t", {matlab::data::ArrayType::UINT64, "uint64", false, 0}}};
        switch (header.event_stream_type) {
            case sepia::type::generic: {
                name_to_key.insert({"bytes", {matlab::data::ArrayType::MATLAB_STRING, "string", false, 0}});
                break;
            }
            case sepia::type::dvs: {
                name_to_key.insert({"x", {matlab::data::ArrayType::UINT16, "uint16_t", false, 0}});
                name_to_key.insert({"y", {matlab::data::ArrayType::UINT16, "uint16_t", false, 0}});
                name_to_key.insert({"is_increase", {matlab::data::ArrayType::LOGICAL, "logical", false, 0}});
                break;
            }
            case sepia::type::atis: {
                name_to_key.insert({"x", {matlab::data::ArrayType::UINT16, "uint16", false, 0}});
                name_to_key.insert({"y", {matlab::data::ArrayType::UINT16, "uint16", false, 0}});
                name_to_key.insert({"is_threshold_crossing", {matlab::data::ArrayType::LOGICAL, "logical", false, 0}});
                name_to_key.insert({"polarity", {matlab::data::ArrayType::LOGICAL, "logical", false, 0}});
                break;
            }
            case sepia::type::color: {
                name_to_key.insert({"x", {matlab::data::ArrayType::UINT16, "uint16", false, 0}});
                name_to_key.insert({"y", {matlab::data::ArrayType::UINT16, "uint16", false, 0}});
                name_to_key.insert({"r", {matlab::data::ArrayType::UINT8, "uint8", false, 0}});
                name_to_key.insert({"g", {matlab::data::ArrayType::UINT8, "uint8", false, 0}});
                name_to_key.insert({"b", {matlab::data::ArrayType::UINT8, "uint8", false, 0}});
                break;
            }
        }
        const matlab::data::StructArray matlab_events = inputs[2];
        for (const auto& field_name : matlab_events.getFieldNames()) {
            auto name_and_key = name_to_key.find(field_name);
            if (name_and_key != name_to_key.end()) {
                if (matlab_events[0][name_and_key->first].getType() != name_and_key->second.type) {
                    error(
                        std::string("the events ") + name_and_key->first + " must be a "
                        + name_and_key->second.type_name);
                }
                const auto dimensions = matlab_events[0][name_and_key->first].getDimensions();
                if (dimensions.size() != 2) {
                    error(std::string("the events ") + name_and_key->first + " must have two dimensions");
                }
                if (dimensions[1] != 1) {
                    error(
                        std::string("the events ") + name_and_key->first
                        + " must have a size one aongisde the second dimension");
                }
                name_and_key->second.size = dimensions[0];
                name_and_key->second.found = true;
            }
        }
        const auto size = name_to_key.begin()->second.size;
        for (const auto& name_and_key : name_to_key) {
            if (!name_and_key.second.found) {
                error(std::string("events must have a field '") + name_and_key.first + "'");
            }
            if (name_and_key.second.size != size) {
                error("events fields do not have the same dimensions");
            }
        }
        if (!outputs.empty()) {
            error("no output arguments are expected");
        }
        const matlab::data::TypedArray<uint64_t> t = matlab_events[0]["t"];
        switch (header.event_stream_type) {
            case sepia::type::generic: {
                const matlab::data::StringArray bytes = matlab_events[0]["bytes"];
                sepia::write<sepia::type::generic> write(sepia::filename_to_ofstream(filename));
                for (std::size_t index = 0; index < size; ++index) {
                    std::string bytes_as_string = bytes[index];
                    write({t[index], std::vector<uint8_t>(bytes_as_string.begin(), bytes_as_string.end())});
                }
                break;
            }
            case sepia::type::dvs: {
                const matlab::data::TypedArray<uint16_t> x = matlab_events[0]["x"];
                const matlab::data::TypedArray<uint16_t> y = matlab_events[0]["y"];
                const matlab::data::TypedArray<bool> is_increase = matlab_events[0]["is_increase"];
                sepia::write<sepia::type::dvs> write(
                    sepia::filename_to_ofstream(filename), header.width, header.height);
                for (std::size_t index = 0; index < size; ++index) {
                    write({t[index], x[index], y[index], is_increase[index]});
                }
                break;
            }
            case sepia::type::atis: {
                const matlab::data::TypedArray<uint16_t> x = matlab_events[0]["x"];
                const matlab::data::TypedArray<uint16_t> y = matlab_events[0]["y"];
                const matlab::data::TypedArray<bool> is_threshold_crossing = matlab_events[0]["is_threshold_crossing"];
                const matlab::data::TypedArray<bool> polarity = matlab_events[0]["polarity"];
                sepia::write<sepia::type::atis> write(
                    sepia::filename_to_ofstream(filename), header.width, header.height);
                for (std::size_t index = 0; index < size; ++index) {
                    write({t[index], x[index], y[index], is_threshold_crossing[index], polarity[index]});
                }
                break;
            }
            case sepia::type::color: {
                const matlab::data::TypedArray<uint16_t> x = matlab_events[0]["x"];
                const matlab::data::TypedArray<uint16_t> y = matlab_events[0]["y"];
                const matlab::data::TypedArray<uint8_t> r = matlab_events[0]["r"];
                const matlab::data::TypedArray<uint8_t> g = matlab_events[0]["g"];
                const matlab::data::TypedArray<uint8_t> b = matlab_events[0]["b"];
                sepia::write<sepia::type::color> write(
                    sepia::filename_to_ofstream(filename), header.width, header.height);
                for (std::size_t index = 0; index < size; ++index) {
                    write({t[index], x[index], y[index], r[index], g[index], b[index]});
                }
                break;
            }
        }
    }
};
