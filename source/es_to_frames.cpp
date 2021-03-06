#include "../third_party/pontella/source/pontella.hpp"
#include "../third_party/sepia/source/sepia.hpp"
#include "timecode.hpp"
#include <iomanip>
#include <sstream>

template <sepia::type type>
std::pair<uint64_t, uint64_t> t_range(std::unique_ptr<std::istream> stream) {
    uint64_t first_t = std::numeric_limits<uint64_t>::max();
    uint64_t last_t = 0;
    sepia::join_observable<type>(std::move(stream), [&](sepia::event<type> event) {
        if (first_t == std::numeric_limits<uint64_t>::max()) {
            first_t = event.t;
        }
        last_t = event.t;
    });
    return {first_t, last_t};
}

enum class style { exponential, linear, window };

struct color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    color(uint8_t default_r, uint8_t default_g, uint8_t default_b) : r(default_r), g(default_g), b(default_b) {}
    color(const std::string& hexadecimal_string) {
        if (hexadecimal_string.size() != 7 || hexadecimal_string.front() != '#'
            || std::any_of(std::next(hexadecimal_string.begin()), hexadecimal_string.end(), [](char character) {
                   return !std::isxdigit(character);
               })) {
            throw std::runtime_error("color must be formatted as #hhhhhh, where h is an hexadecimal digit");
        }
        r = static_cast<uint8_t>(std::stoul(hexadecimal_string.substr(1, 2), nullptr, 16));
        g = static_cast<uint8_t>(std::stoul(hexadecimal_string.substr(3, 2), nullptr, 16));
        b = static_cast<uint8_t>(std::stoul(hexadecimal_string.substr(5, 2), nullptr, 16));
    }
    uint8_t mix_r(color other, float lambda) {
        return static_cast<uint8_t>((1.0f - lambda) * r + lambda * other.r);
    }
    uint8_t mix_g(color other, float lambda) {
        return static_cast<uint8_t>((1.0f - lambda) * g + lambda * other.g);
    }
    uint8_t mix_b(color other, float lambda) {
        return static_cast<uint8_t>((1.0f - lambda) * b + lambda * other.b);
    }
};

int main(int argc, char* argv[]) {
    return pontella::main(
        {"es_to_frames converts an Event Stream file to video frames",
         "    Frames use the P6 Netpbm format (https://en.wikipedia.org/wiki/Netpbm) if the output is a directory",
         "    Otherwise, the output consists in raw rgb24 frames",
         "Syntax: ./es_to_ppms [options]",
         "Available options:",
         "    -i file, --input file                  sets the path to the input .es file",
         "                                               defaults to standard input",
         "    -o directory, --output directory       sets the path to the output directory",
         "                                               defaults to standard output",
         "    -f frametime, --frametime frametime    sets the time between two frames (timecode)",
         "                                               defaults to 00:00:00.02",
         "    -s style, --style style                selects the decay function",
         "                                               one of exponential (default), linear, window",
         "    -p parameter, --parameter parameter    sets the function parameter (timecode)",
         "                                               defaults to 00:00:00.10",
         "                                               if style is `exponential`, the decay is set to parameter",
         "                                               if style is `linear`, the decay is set to parameter * 2",
         "                                               if style is `window`, the time window is set to parameter",
         "    -a color, --oncolor color              sets the color for ON events",
         "                                               color must be formatted as #hhhhhh,",
         "                                               where h is an hexadecimal digit",
         "                                               defaults to #ffffff",
         "    -b color, --offcolor color             sets the color for OFF events",
         "                                               color must be formatted as #hhhhhh,",
         "                                               where h is an hexadecimal digit",
         "                                               defaults to #000000",
         "    -c color, --idlecolor color            sets the background color",
         "                                               color must be formatted as #hhhhhh,",
         "                                               where h is an hexadecimal digit",
         "                                               defaults to #808080",
         "    -d digits, --digits digits             sets the number of digits in output filenames",
         "                                               ignored if the output is not a directory",
         "                                               defaults to 6",
         "    -h, --help                 shows this help message"},
        argc,
        argv,
        0,
        {
            {"input", {"i"}},
            {"output", {"o"}},
            {"frametime", {"f"}},
            {"style", {"s"}},
            {"parameter", {"p"}},
            {"oncolor", {"a"}},
            {"offcolor", {"b"}},
            {"idlecolor", {"c"}},
            {"digits", {"d"}},
        },
        {},
        [](pontella::command command) {
            uint64_t frametime = 20000;
            {
                const auto name_and_argument = command.options.find("frametime");
                if (name_and_argument != command.options.end()) {
                    frametime = timecode(name_and_argument->second).value();
                    if (frametime == 0) {
                        throw std::runtime_error("the frametime must be larger than 0");
                    }
                }
            }
            color on_color(0xff, 0xff, 0xff);
            {
                const auto name_and_argument = command.options.find("oncolor");
                if (name_and_argument != command.options.end()) {
                    on_color = color(name_and_argument->second);
                }
            }
            color off_color(0x00, 0x00, 0x00);
            {
                const auto name_and_argument = command.options.find("offcolor");
                if (name_and_argument != command.options.end()) {
                    off_color = color(name_and_argument->second);
                }
            }
            color idle_color(0x80, 0x80, 0x80);
            {
                const auto name_and_argument = command.options.find("idlecolor");
                if (name_and_argument != command.options.end()) {
                    idle_color = color(name_and_argument->second);
                }
            }
            auto decay_style = style::exponential;
            {
                const auto name_and_argument = command.options.find("style");
                if (name_and_argument != command.options.end()) {
                    if (name_and_argument->second == "linear") {
                        decay_style = style::linear;
                    } else if (name_and_argument->second == "window") {
                        decay_style = style::window;
                    } else if (name_and_argument->second != "exponential") {
                        throw std::runtime_error("style must be one of {exponential, linear, window}");
                    }
                }
            }
            uint64_t parameter = 100000;
            {
                const auto name_and_argument = command.options.find("parameter");
                if (name_and_argument != command.options.end()) {
                    parameter = timecode(name_and_argument->second).value();
                    if (parameter == 0) {
                        throw std::runtime_error("the parameter must be larger than 0");
                    }
                }
            }
            std::string output_directory;
            {
                const auto name_and_argument = command.options.find("output");
                if (name_and_argument != command.options.end()) {
                    output_directory = name_and_argument->second;
                }
            }
            std::unique_ptr<std::istream> input;
            {
                const auto name_and_argument = command.options.find("input");
                if (name_and_argument == command.options.end()) {
                    input = sepia::make_unique<std::istream>(std::cin.rdbuf());
                } else {
                    input = sepia::filename_to_ifstream(name_and_argument->second);
                }
            }
            const auto header = sepia::read_header(*input);
            uint8_t digits = 6;
            {
                const auto name_and_argument = command.options.find("digits");
                if (name_and_argument != command.options.end()) {
                    const auto digits_candidate = std::stoull(name_and_argument->second);
                    if (digits_candidate == 0 || digits_candidate > 255) {
                        throw std::runtime_error("digits must be in the range [1, 255]");
                    }
                    digits = static_cast<uint8_t>(digits_candidate);
                }
            }
            std::vector<std::pair<uint64_t, bool>> ts_and_ons(
                header.width * header.height, {std::numeric_limits<uint64_t>::max(), false});
            uint64_t frame_index = 0;
            auto first_t = std::numeric_limits<uint64_t>::max();
            auto generate_ppms = [&](sepia::dvs_event event) {
                if (first_t == std::numeric_limits<uint64_t>::max()) {
                    first_t = event.t;
                }
                const auto frame_t = first_t + frame_index * frametime;
                if (event.t >= frame_t) {
                    std::vector<uint8_t> frame(header.width * header.height * 3);
                    for (uint16_t y = 0; y < header.height; ++y) {
                        for (uint16_t x = 0; x < header.width; ++x) {
                            const auto& t_and_on = ts_and_ons[x + y * header.width];
                            float lambda;
                            if (t_and_on.first == std::numeric_limits<uint64_t>::max()) {
                                lambda = 0.0f;
                            } else {
                                switch (decay_style) {
                                    case style::exponential:
                                        lambda = std::exp(
                                            -static_cast<float>(frame_t - 1 - t_and_on.first)
                                            / static_cast<float>(parameter));
                                        break;
                                    case style::linear:
                                        lambda = t_and_on.first + 2 * parameter > frame_t - 1 ?
                                                     static_cast<float>(t_and_on.first + 2 * parameter - (frame_t - 1))
                                                         / static_cast<float>(2 * parameter) :
                                                     0.0f;
                                        break;
                                    case style::window:
                                        lambda = t_and_on.first + parameter > frame_t - 1 ? 1.0f : 0.0f;
                                        break;
                                }
                            }
                            frame[(x + (header.height - 1 - y) * header.width) * 3] =
                                idle_color.mix_r(t_and_on.second ? on_color : off_color, lambda);
                            frame[(x + (header.height - 1 - y) * header.width) * 3 + 1] =
                                idle_color.mix_g(t_and_on.second ? on_color : off_color, lambda);
                            frame[(x + (header.height - 1 - y) * header.width) * 3 + 2] =
                                idle_color.mix_b(t_and_on.second ? on_color : off_color, lambda);
                        }
                    }
                    if (output_directory.empty()) {
                        std::cout.write(reinterpret_cast<const char*>(frame.data()), frame.size());
                    } else {
                        std::stringstream name;
                        name << std::setfill('0') << std::setw(digits) << frame_index << ".ppm";
                        const auto filename = sepia::join({output_directory, name.str()});
                        std::ofstream output(filename);
                        if (!output.good()) {
                            throw sepia::unwritable_file(filename);
                        }
                        output << "P6\n" << header.width << " " << header.height << "\n255\n";
                        output.write(reinterpret_cast<const char*>(frame.data()), frame.size());
                    }
                    ++frame_index;
                }
                ts_and_ons[event.x + event.y * header.width].first = event.t;
                ts_and_ons[event.x + event.y * header.width].second = event.is_increase;
            };
            switch (header.event_stream_type) {
                case sepia::type::generic:
                    throw std::runtime_error("unsupported event stream type 'generic'");
                case sepia::type::dvs:
                    sepia::join_observable<sepia::type::dvs>(std::move(input), header, generate_ppms);
                    break;
                case sepia::type::atis:
                    sepia::join_observable<sepia::type::atis>(
                        std::move(input),
                        header,
                        sepia::make_split<sepia::type::atis>(generate_ppms, [](sepia::threshold_crossing) {}));
                    break;
                case sepia::type::color:
                    throw std::runtime_error("unsupported event stream type 'color'");
            }
        });
}
