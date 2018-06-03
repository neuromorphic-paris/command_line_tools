#include "../third_party/sepia/source/sepia.hpp"
#include "mex.hpp"
#include "mexAdapter.hpp"

/// count determines the number of events in the given stream.
template <sepia::type event_stream_type>
std::size_t count(std::unique_ptr<std::istream> stream) {
    std::size_t events = 0;
    sepia::join_observable<event_stream_type>(std::move(stream), [&](sepia::event<event_stream_type>) { ++events; });
    return events;
}

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
        if (inputs.size() != 1) {
            error("a single input argument is expected");
        }
        if (inputs[0].getType() != matlab::data::ArrayType::CHAR) {
            error("the first input argument must be a char array");
        }
        if (outputs.size() != 2) {
            error("two output arguments are expected");
        }
        const matlab::data::CharArray filename_as_char_array = inputs[0];
        const auto filename = filename_as_char_array.toAscii();
        const auto header = sepia::read_header(sepia::filename_to_ifstream(filename));
        switch (header.event_stream_type) {
            case sepia::type::generic: {
                {
                    auto matlab_header = factory.createStructArray({1}, {"type"});
                    matlab_header[0]["type"] = factory.createCharArray("generic");
                    outputs[0] = std::move(matlab_header);
                }
                const auto events = count<sepia::type::generic>(sepia::filename_to_ifstream(filename));
                auto t = factory.createArray<uint64_t>({events, 1});
                auto bytes = factory.createArray<matlab::data::MATLABString>({events, 1});
                std::size_t index = 0;
                sepia::join_observable<sepia::type::generic>(
                    sepia::filename_to_ifstream(filename), [&](sepia::generic_event generic_event) {
                        t[index] = generic_event.t;
                        bytes[index] = std::string(generic_event.bytes.begin(), generic_event.bytes.end());
                        ++index;
                    });
                auto matlab_events = factory.createStructArray({1}, {"t", "bytes"});
                matlab_events[0]["t"] = std::move(t);
                matlab_events[0]["bytes"] = std::move(bytes);
                outputs[1] = std::move(matlab_events);
                break;
            }
            case sepia::type::dvs: {
                {
                    auto matlab_header = factory.createStructArray({1}, {"type", "width", "height"});
                    matlab_header[0]["type"] = factory.createCharArray("dvs");
                    matlab_header[0]["width"] = factory.createArray<uint16_t>({1, 1}, {header.width});
                    matlab_header[0]["height"] = factory.createArray<uint16_t>({1, 1}, {header.height});
                    outputs[0] = std::move(matlab_header);
                }
                const auto events = count<sepia::type::dvs>(sepia::filename_to_ifstream(filename));
                auto t = factory.createArray<uint64_t>({events, 1});
                auto x = factory.createArray<uint16_t>({events, 1});
                auto y = factory.createArray<uint16_t>({events, 1});
                auto is_increase = factory.createArray<bool>({events, 1});
                std::size_t index = 0;
                sepia::join_observable<sepia::type::dvs>(
                    sepia::filename_to_ifstream(filename), [&](sepia::dvs_event dvs_event) {
                        t[index] = dvs_event.t;
                        x[index] = dvs_event.x;
                        y[index] = dvs_event.y;
                        is_increase[index] = dvs_event.is_increase;
                        ++index;
                    });
                auto matlab_events = factory.createStructArray({1}, {"t", "x", "y", "is_increase"});
                matlab_events[0]["t"] = std::move(t);
                matlab_events[0]["x"] = std::move(x);
                matlab_events[0]["y"] = std::move(y);
                matlab_events[0]["is_increase"] = std::move(is_increase);
                outputs[1] = std::move(matlab_events);
                break;
            }
            case sepia::type::atis: {
                {
                    auto matlab_header = factory.createStructArray({1}, {"type", "width", "height"});
                    matlab_header[0]["type"] = factory.createCharArray("atis");
                    matlab_header[0]["width"] = factory.createArray<uint16_t>({1, 1}, {header.width});
                    matlab_header[0]["height"] = factory.createArray<uint16_t>({1, 1}, {header.height});
                    outputs[0] = std::move(matlab_header);
                }
                const auto events = count<sepia::type::atis>(sepia::filename_to_ifstream(filename));
                auto t = factory.createArray<uint64_t>({events, 1});
                auto x = factory.createArray<uint16_t>({events, 1});
                auto y = factory.createArray<uint16_t>({events, 1});
                auto is_threshold_crossing = factory.createArray<bool>({events, 1});
                auto polarity = factory.createArray<bool>({events, 1});
                std::size_t index = 0;
                sepia::join_observable<sepia::type::atis>(
                    sepia::filename_to_ifstream(filename), [&](sepia::atis_event atis_event) {
                        t[index] = atis_event.t;
                        x[index] = atis_event.x;
                        y[index] = atis_event.y;
                        is_threshold_crossing[index] = atis_event.is_threshold_crossing;
                        polarity[index] = atis_event.polarity;
                        ++index;
                    });
                auto matlab_events =
                    factory.createStructArray({1}, {"t", "x", "y", "is_threshold_crossing", "polarity"});
                matlab_events[0]["t"] = std::move(t);
                matlab_events[0]["x"] = std::move(x);
                matlab_events[0]["y"] = std::move(y);
                matlab_events[0]["is_threshold_crossing"] = std::move(is_threshold_crossing);
                matlab_events[0]["polarity"] = std::move(polarity);
                outputs[1] = std::move(matlab_events);
                break;
            }
            case sepia::type::color: {
                {
                    auto matlab_header = factory.createStructArray({1}, {"type", "width", "height"});
                    matlab_header[0]["type"] = factory.createCharArray("color");
                    matlab_header[0]["width"] = factory.createArray<uint16_t>({1, 1}, {header.width});
                    matlab_header[0]["height"] = factory.createArray<uint16_t>({1, 1}, {header.height});
                    outputs[0] = std::move(matlab_header);
                }
                const auto events = count<sepia::type::color>(sepia::filename_to_ifstream(filename));
                auto t = factory.createArray<uint64_t>({events, 1});
                auto x = factory.createArray<uint16_t>({events, 1});
                auto y = factory.createArray<uint16_t>({events, 1});
                auto r = factory.createArray<uint8_t>({events, 1});
                auto g = factory.createArray<uint8_t>({events, 1});
                auto b = factory.createArray<uint8_t>({events, 1});
                std::size_t index = 0;
                sepia::join_observable<sepia::type::color>(
                    sepia::filename_to_ifstream(filename), [&](sepia::color_event color_event) {
                        t[index] = color_event.t;
                        x[index] = color_event.x;
                        y[index] = color_event.y;
                        r[index] = color_event.r;
                        g[index] = color_event.g;
                        b[index] = color_event.b;
                        ++index;
                    });
                auto matlab_events = factory.createStructArray({1}, {"t", "x", "y", "r", "g", "b"});
                matlab_events[0]["t"] = std::move(t);
                matlab_events[0]["x"] = std::move(x);
                matlab_events[0]["y"] = std::move(y);
                matlab_events[0]["r"] = std::move(r);
                matlab_events[0]["g"] = std::move(g);
                matlab_events[0]["b"] = std::move(b);
                outputs[1] = std::move(matlab_events);
                break;
            }
        }
    }
};
