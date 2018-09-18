#include "../third_party/pontella/source/pontella.hpp"
#include "../third_party/sepia/source/sepia.hpp"

/// type_to_string returns a text representation og the type enum.
std::string type_to_string(sepia::type type) {
    switch (type) {
        case sepia::type::generic:
            return "generic";
        case sepia::type::dvs:
            return "dvs";
        case sepia::type::atis:
            return "atis";
        case sepia::type::color:
            return "color";
    }
    return "unknown";
}

/// properties_to_json converts a list of properties to pretty-printed JSON.
std::string properties_to_json(const std::vector<std::pair<std::string, std::string>>& properties) {
    std::string json("{\n");
    for (std::size_t index = 0; index < properties.size(); ++index) {
        json += std::string("    \"") + properties[index].first + "\": " + properties[index].second
                + (index < properties.size() - 1 ? "," : "") + "\n";
    }
    json += "}";
    return json;
}

int main(int argc, char* argv[]) {
    return pontella::main(
        {
            "statistics retrieves the event stream's properties and outputs them in JSON format.",
            "Syntax: ./statistics [options] /path/to/input.es",
            "Available options:",
            "    -h, --help    shows this help message",
        },
        argc,
        argv,
        1,
        {},
        {},
        [](pontella::command command) {
            const auto header = sepia::read_header(sepia::filename_to_ifstream(command.arguments[0]));
            std::vector<std::pair<std::string, std::string>> properties{
                {"version",
                 std::string("\"") + std::to_string(static_cast<uint32_t>(std::get<0>(header.version))) + "."
                     + std::to_string(static_cast<uint32_t>(std::get<1>(header.version))) + "."
                     + std::to_string(static_cast<uint32_t>(std::get<2>(header.version))) + "\""},
                {"type", std::string("\"") + type_to_string(header.event_stream_type) + "\""},
            };
            if (header.event_stream_type != sepia::type::generic) {
                properties.emplace_back("width", std::to_string(header.width));
                properties.emplace_back("height", std::to_string(header.height));
            }
            switch (header.event_stream_type) {
                case sepia::type::generic: {
                    auto first = true;
                    uint64_t begin_t;
                    uint64_t end_t;
                    std::size_t events = 0;
                    uint64_t size_sum = 0;
                    sepia::join_observable<sepia::type::generic>(
                        sepia::filename_to_ifstream(command.arguments[0]), [&](sepia::generic_event generic_event) {
                            if (first) {
                                first = false;
                                begin_t = generic_event.t;
                            }
                            end_t = generic_event.t;
                            ++events;
                            size_sum += generic_event.bytes.size();
                        });
                    properties.emplace_back("begin_t", std::to_string(begin_t));
                    properties.emplace_back("end_t", std::to_string(end_t));
                    properties.emplace_back("events", std::to_string(events));
                    properties.emplace_back("size_sum", std::to_string(size_sum));
                    std::cout << properties_to_json(properties) << std::endl;
                    break;
                }
                case sepia::type::dvs: {
                    auto first = true;
                    uint64_t begin_t;
                    uint64_t end_t;
                    std::size_t events = 0;
                    uint64_t x_sum = 0;
                    uint64_t y_sum = 0;
                    std::size_t increase_events = 0;
                    sepia::join_observable<sepia::type::dvs>(
                        sepia::filename_to_ifstream(command.arguments[0]), [&](sepia::dvs_event dvs_event) {
                            if (first) {
                                first = false;
                                begin_t = dvs_event.t;
                            }
                            end_t = dvs_event.t;
                            ++events;
                            x_sum += dvs_event.x;
                            y_sum += dvs_event.y;
                            if (dvs_event.is_increase) {
                                ++increase_events;
                            }
                        });
                    properties.emplace_back("begin_t", std::to_string(begin_t));
                    properties.emplace_back("end_t", std::to_string(end_t));
                    properties.emplace_back("events", std::to_string(events));
                    properties.emplace_back("x_sum", std::to_string(x_sum));
                    properties.emplace_back("y_sum", std::to_string(y_sum));
                    properties.emplace_back("increase_events", std::to_string(increase_events));
                    std::cout << properties_to_json(properties) << std::endl;
                    break;
                }
                case sepia::type::atis: {
                    auto first = true;
                    uint64_t begin_t;
                    uint64_t end_t;
                    std::size_t events = 0;
                    uint64_t x_sum = 0;
                    uint64_t y_sum = 0;
                    std::size_t dvs_events = 0;
                    std::size_t increase_events = 0;
                    std::size_t second_events = 0;
                    sepia::join_observable<sepia::type::atis>(
                        sepia::filename_to_ifstream(command.arguments[0]), [&](sepia::atis_event atis_event) {
                            if (first) {
                                first = false;
                                begin_t = atis_event.t;
                            }
                            end_t = atis_event.t;
                            ++events;
                            x_sum += atis_event.x;
                            y_sum += atis_event.y;
                            if (!atis_event.is_threshold_crossing) {
                                ++dvs_events;
                            }
                            if (atis_event.polarity) {
                                if (atis_event.is_threshold_crossing) {
                                    ++second_events;
                                } else {
                                    ++increase_events;
                                }
                            }
                        });
                    properties.emplace_back("begin_t", std::to_string(begin_t));
                    properties.emplace_back("end_t", std::to_string(end_t));
                    properties.emplace_back("events", std::to_string(events));
                    properties.emplace_back("x_sum", std::to_string(x_sum));
                    properties.emplace_back("y_sum", std::to_string(y_sum));
                    properties.emplace_back("dvs_events", std::to_string(dvs_events));
                    properties.emplace_back("increase_events", std::to_string(increase_events));
                    properties.emplace_back("second_events", std::to_string(second_events));
                    std::cout << properties_to_json(properties) << std::endl;
                    break;
                }
                case sepia::type::color: {
                    auto first = true;
                    uint64_t begin_t;
                    uint64_t end_t;
                    std::size_t events = 0;
                    uint64_t x_sum = 0;
                    uint64_t y_sum = 0;
                    uint64_t r_sum = 0;
                    uint64_t g_sum = 0;
                    uint64_t b_sum = 0;
                    sepia::join_observable<sepia::type::color>(
                        sepia::filename_to_ifstream(command.arguments[0]), [&](sepia::color_event color_event) {
                            if (first) {
                                first = false;
                                begin_t = color_event.t;
                            }
                            end_t = color_event.t;
                            ++events;
                            x_sum += color_event.x;
                            y_sum += color_event.y;
                            r_sum += color_event.r;
                            g_sum += color_event.g;
                            b_sum += color_event.b;
                        });
                    properties.emplace_back("begin_t", std::to_string(begin_t));
                    properties.emplace_back("end_t", std::to_string(end_t));
                    properties.emplace_back("events", std::to_string(events));
                    properties.emplace_back("x_sum", std::to_string(x_sum));
                    properties.emplace_back("y_sum", std::to_string(y_sum));
                    properties.emplace_back("r_sum", std::to_string(r_sum));
                    properties.emplace_back("g_sum", std::to_string(g_sum));
                    properties.emplace_back("b_sum", std::to_string(r_sum));
                    std::cout << properties_to_json(properties) << std::endl;
                    break;
                }
            }
        });
    return 0;
}
