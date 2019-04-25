#include "../third_party/pontella/source/pontella.hpp"
#include "../third_party/sepia/source/sepia.hpp"
#include "../third_party/tarsier/source/convert.hpp"
#include "../third_party/tarsier/source/hash.hpp"
#include "../third_party/tarsier/source/replicate.hpp"
#include <iomanip>
#include <sstream>

/// type_to_string returns a text representation of the type enum.
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

/// hash_to_string converts a 128 bits hash to a hxadecimal representation.
std::string hash_to_string(std::pair<uint64_t, uint64_t> hash) {
    std::stringstream stream;
    stream << '"' << std::hex << std::get<1>(hash) << std::hex << std::setfill('0') << std::setw(16)
           << std::get<0>(hash) << '"';
    return stream.str();
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
                    std::string t_hash;
                    std::string bytes_hash;
                    {
                        auto hash = tarsier::make_hash<uint8_t>(
                            [&](std::pair<uint64_t, uint64_t> hash_value) { bytes_hash = hash_to_string(hash_value); });
                        sepia::join_observable<sepia::type::generic>(
                            sepia::filename_to_ifstream(command.arguments[0]),
                            tarsier::make_replicate<sepia::generic_event>(
                                [&](sepia::generic_event generic_event) {
                                    if (first) {
                                        first = false;
                                        begin_t = generic_event.t;
                                    }
                                    end_t = generic_event.t;
                                    ++events;
                                    for (const auto character : generic_event.bytes) {
                                        hash(character);
                                    }
                                },
                                tarsier::make_convert<sepia::generic_event>(
                                    [](sepia::generic_event generic_event) -> uint64_t { return generic_event.t; },
                                    tarsier::make_hash<uint64_t>([&](std::pair<uint64_t, uint64_t> hash_value) {
                                        t_hash = hash_to_string(hash_value);
                                    }))));
                    }
                    properties.emplace_back("begin_t", std::to_string(begin_t));
                    properties.emplace_back("end_t", std::to_string(end_t));
                    properties.emplace_back("events", std::to_string(events));
                    properties.emplace_back("t_hash", t_hash);
                    properties.emplace_back("bytes_hash", bytes_hash);
                    std::cout << properties_to_json(properties) << std::endl;
                    break;
                }
                case sepia::type::dvs: {
                    auto first = true;
                    uint64_t begin_t;
                    uint64_t end_t;
                    std::size_t events = 0;
                    std::size_t increase_events = 0;
                    std::string t_hash;
                    std::string x_hash;
                    std::string y_hash;
                    sepia::join_observable<sepia::type::dvs>(
                        sepia::filename_to_ifstream(command.arguments[0]),
                        tarsier::make_replicate<sepia::dvs_event>(
                            [&](sepia::dvs_event dvs_event) {
                                if (first) {
                                    first = false;
                                    begin_t = dvs_event.t;
                                }
                                end_t = dvs_event.t;
                                ++events;
                                if (dvs_event.is_increase) {
                                    ++increase_events;
                                }
                            },
                            tarsier::make_convert<sepia::dvs_event>(
                                [](sepia::dvs_event dvs_event) -> uint64_t { return dvs_event.t; },
                                tarsier::make_hash<uint64_t>([&](std::pair<uint64_t, uint64_t> hash_value) {
                                    t_hash = hash_to_string(hash_value);
                                })),
                            tarsier::make_convert<sepia::dvs_event>(
                                [](sepia::dvs_event dvs_event) -> uint16_t { return dvs_event.x; },
                                tarsier::make_hash<uint16_t>([&](std::pair<uint64_t, uint64_t> hash_value) {
                                    x_hash = hash_to_string(hash_value);
                                })),
                            tarsier::make_convert<sepia::dvs_event>(
                                [](sepia::dvs_event dvs_event) -> uint16_t { return dvs_event.y; },
                                tarsier::make_hash<uint16_t>([&](std::pair<uint64_t, uint64_t> hash_value) {
                                    y_hash = hash_to_string(hash_value);
                                }))));
                    properties.emplace_back("begin_t", std::to_string(begin_t));
                    properties.emplace_back("end_t", std::to_string(end_t));
                    properties.emplace_back("events", std::to_string(events));
                    properties.emplace_back("increase_events", std::to_string(increase_events));
                    properties.emplace_back("t_hash", t_hash);
                    properties.emplace_back("x_hash", x_hash);
                    properties.emplace_back("y_hash", y_hash);
                    std::cout << properties_to_json(properties) << std::endl;
                    break;
                }
                case sepia::type::atis: {
                    auto first = true;
                    uint64_t begin_t;
                    uint64_t end_t;
                    std::size_t events = 0;
                    std::size_t dvs_events = 0;
                    std::size_t increase_events = 0;
                    std::size_t second_events = 0;
                    std::string t_hash;
                    std::string x_hash;
                    std::string y_hash;
                    sepia::join_observable<sepia::type::atis>(
                        sepia::filename_to_ifstream(command.arguments[0]),
                        tarsier::make_replicate<sepia::atis_event>(
                            [&](sepia::atis_event atis_event) {
                                if (first) {
                                    first = false;
                                    begin_t = atis_event.t;
                                }
                                end_t = atis_event.t;
                                ++events;
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
                            },
                            tarsier::make_convert<sepia::atis_event>(
                                [](sepia::atis_event atis_event) -> uint64_t { return atis_event.t; },
                                tarsier::make_hash<uint64_t>([&](std::pair<uint64_t, uint64_t> hash_value) {
                                    t_hash = hash_to_string(hash_value);
                                })),
                            tarsier::make_convert<sepia::atis_event>(
                                [](sepia::atis_event atis_event) -> uint16_t { return atis_event.x; },
                                tarsier::make_hash<uint16_t>([&](std::pair<uint64_t, uint64_t> hash_value) {
                                    x_hash = hash_to_string(hash_value);
                                })),
                            tarsier::make_convert<sepia::atis_event>(
                                [](sepia::atis_event atis_event) -> uint16_t { return atis_event.y; },
                                tarsier::make_hash<uint16_t>([&](std::pair<uint64_t, uint64_t> hash_value) {
                                    y_hash = hash_to_string(hash_value);
                                }))));
                    properties.emplace_back("begin_t", std::to_string(begin_t));
                    properties.emplace_back("end_t", std::to_string(end_t));
                    properties.emplace_back("events", std::to_string(events));
                    properties.emplace_back("dvs_events", std::to_string(dvs_events));
                    properties.emplace_back("increase_events", std::to_string(increase_events));
                    properties.emplace_back("second_events", std::to_string(second_events));
                    properties.emplace_back("t_hash", t_hash);
                    properties.emplace_back("x_hash", x_hash);
                    properties.emplace_back("y_hash", y_hash);

                    std::cout << properties_to_json(properties) << std::endl;
                    break;
                }
                case sepia::type::color: {
                    auto first = true;
                    uint64_t begin_t;
                    uint64_t end_t;
                    std::size_t events = 0;
                    std::string t_hash;
                    std::string x_hash;
                    std::string y_hash;
                    std::string r_hash;
                    std::string g_hash;
                    std::string b_hash;
                    sepia::join_observable<sepia::type::color>(
                        sepia::filename_to_ifstream(command.arguments[0]),
                        tarsier::make_replicate<sepia::color_event>(
                            [&](sepia::color_event color_event) {
                                if (first) {
                                    first = false;
                                    begin_t = color_event.t;
                                }
                                end_t = color_event.t;
                                ++events;
                            },
                            tarsier::make_convert<sepia::color_event>(
                                [](sepia::color_event color_event) -> uint64_t { return color_event.t; },
                                tarsier::make_hash<uint64_t>([&](std::pair<uint64_t, uint64_t> hash_value) {
                                    t_hash = hash_to_string(hash_value);
                                })),
                            tarsier::make_convert<sepia::color_event>(
                                [](sepia::color_event color_event) -> uint16_t { return color_event.x; },
                                tarsier::make_hash<uint16_t>([&](std::pair<uint64_t, uint64_t> hash_value) {
                                    x_hash = hash_to_string(hash_value);
                                })),
                            tarsier::make_convert<sepia::color_event>(
                                [](sepia::color_event color_event) -> uint16_t { return color_event.y; },
                                tarsier::make_hash<uint16_t>([&](std::pair<uint64_t, uint64_t> hash_value) {
                                    y_hash = hash_to_string(hash_value);
                                })),
                            tarsier::make_convert<sepia::color_event>(
                                [](sepia::color_event color_event) -> uint8_t { return color_event.r; },
                                tarsier::make_hash<uint8_t>([&](std::pair<uint64_t, uint64_t> hash_value) {
                                    r_hash = hash_to_string(hash_value);
                                })),
                            tarsier::make_convert<sepia::color_event>(
                                [](sepia::color_event color_event) -> uint8_t { return color_event.g; },
                                tarsier::make_hash<uint8_t>([&](std::pair<uint64_t, uint64_t> hash_value) {
                                    g_hash = hash_to_string(hash_value);
                                })),
                            tarsier::make_convert<sepia::color_event>(
                                [](sepia::color_event color_event) -> uint8_t { return color_event.b; },
                                tarsier::make_hash<uint8_t>([&](std::pair<uint64_t, uint64_t> hash_value) {
                                    b_hash = hash_to_string(hash_value);
                                }))));
                    properties.emplace_back("begin_t", std::to_string(begin_t));
                    properties.emplace_back("end_t", std::to_string(end_t));
                    properties.emplace_back("events", std::to_string(events));
                    properties.emplace_back("t_hash", t_hash);
                    properties.emplace_back("x_hash", x_hash);
                    properties.emplace_back("y_hash", y_hash);
                    properties.emplace_back("r_hash", r_hash);
                    properties.emplace_back("g_hash", g_hash);
                    properties.emplace_back("b_hash", b_hash);
                    std::cout << properties_to_json(properties) << std::endl;
                    break;
                }
            }
        });
    return 0;
}
