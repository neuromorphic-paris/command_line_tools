#include "../third_party/pontella/source/pontella.hpp"
#include "../third_party/sepia/source/sepia.hpp"
#include "../third_party/tarsier/source/replicate.hpp"
#include "../third_party/tarsier/source/stitch.hpp"
#include "timecode.hpp"
#include <iomanip>
#include <sstream>

/// exposure_measurement holds the parameters of an absolute luminance sample.
SEPIA_PACK(struct exposure_measurement {
    uint64_t delta_t;
    uint16_t x;
    uint16_t y;
});

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

class frame {
    public:
    frame(uint16_t width, uint16_t height) : _width(width), _height(height), _bytes(width * height * 3) {}
    frame(const frame&) = delete;
    frame(frame&& other) = default;
    frame& operator=(const frame&) = delete;
    frame& operator=(frame&& other) = delete;
    virtual ~frame() {}

    virtual void paste_ts_and_ons(
        uint16_t width,
        uint16_t height,
        const std::vector<std::pair<uint64_t, bool>>& ts_and_ons,
        uint16_t x_offset,
        uint16_t y_offset,
        style decay_style,
        uint64_t parameter,
        color on_color,
        color off_color,
        color idle_color,
        uint64_t frame_t) {
        for (uint16_t y = 0; y < height; ++y) {
            for (uint16_t x = 0; x < width; ++x) {
                const auto& t_and_on = ts_and_ons[x + y * width];
                float lambda;
                if (t_and_on.first == std::numeric_limits<uint64_t>::max()) {
                    lambda = 0.0f;
                } else {
                    switch (decay_style) {
                        case style::exponential:
                            lambda = std::exp(
                                -static_cast<float>(frame_t - 1 - t_and_on.first) / static_cast<float>(parameter));
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
                const auto index = (x + x_offset + (_height - 1 - (y + y_offset)) * _width) * 3;
                _bytes[index] = idle_color.mix_r(t_and_on.second ? on_color : off_color, lambda);
                _bytes[index + 1] = idle_color.mix_g(t_and_on.second ? on_color : off_color, lambda);
                _bytes[index + 2] = idle_color.mix_b(t_and_on.second ? on_color : off_color, lambda);
            }
        }
    }

    virtual void paste_delta_ts(
        uint16_t width,
        uint16_t height,
        const std::vector<uint64_t>& delta_ts,
        uint16_t x_offset,
        uint16_t y_offset,
        uint64_t white,
        bool white_auto,
        uint64_t black,
        bool black_auto,
        float discard_ratio,
        color atis_color) {
        auto minimum = 0.5f;
        auto maximum = 0.5f;
        if (white_auto || black_auto) {
            std::vector<uint64_t> sorted_delta_ts(delta_ts.size());
            const auto end =
                std::copy_if(delta_ts.begin(), delta_ts.end(), sorted_delta_ts.begin(), [](uint64_t delta_t) {
                    return delta_t < std::numeric_limits<uint64_t>::max() && delta_t > 0;
                });
            const auto size = static_cast<std::size_t>(std::distance(sorted_delta_ts.begin(), end));
            if (size > 0) {
                std::sort(sorted_delta_ts.begin(), end);
                auto black_candidate = sorted_delta_ts[static_cast<std::size_t>(size * (1.0f - discard_ratio))];
                auto white_candidate = sorted_delta_ts[static_cast<std::size_t>(size * discard_ratio + 0.5f)];
                if (black_candidate > white_candidate) {
                    minimum = 1.0f / static_cast<float>(black_candidate);
                    maximum = 1.0f / static_cast<float>(white_candidate);
                } else {
                    black_candidate = *std::prev(end);
                    white_candidate = sorted_delta_ts.front();
                    if (black_candidate > white_candidate) {
                        minimum = 1.0f / static_cast<float>(black_candidate);
                        maximum = 1.0f / static_cast<float>(white_candidate);
                    }
                }
            }
        } else {
            minimum = 1.0f / static_cast<float>(black);
            maximum = 1.0f / static_cast<float>(white);
        }
        auto slope = 0.0f;
        auto intercept = 0.5f;
        if (maximum > minimum) {
            slope = 1.0f / (maximum - minimum);
            intercept = -slope * minimum;
        }
        for (uint16_t y = 0; y < height; ++y) {
            for (uint16_t x = 0; x < width; ++x) {
                const auto index = (x + x_offset + (_height - 1 - (y + y_offset)) * _width) * 3;
                const auto delta_t = delta_ts[x + y * width];
                if (delta_t == std::numeric_limits<uint64_t>::max()) {
                    _bytes[index] = atis_color.r;
                    _bytes[index + 1] = atis_color.g;
                    _bytes[index + 2] = atis_color.b;
                } else {
                    uint8_t value = 0;
                    if (delta_t > 0) {
                        const auto luminance = 1.0f / static_cast<float>(delta_t);
                        if (luminance >= maximum) {
                            value = 255;
                        } else if (luminance > minimum) {
                            value = static_cast<uint8_t>((slope * luminance + intercept) * 255.0f);
                        }
                    }
                    _bytes[index] = value;
                    _bytes[index + 1] = value;
                    _bytes[index + 2] = value;
                }
            }
        }
    }

    virtual void write(const std::string& output_directory, uint8_t digits, uint64_t frame_index) const {
        if (output_directory.empty()) {
            std::cout.write(reinterpret_cast<const char*>(_bytes.data()), _bytes.size());
        } else {
            std::stringstream name;
            name << std::setfill('0') << std::setw(digits) << frame_index << ".ppm";
            const auto filename = sepia::join({output_directory, name.str()});
            std::ofstream output(filename);
            if (!output.good()) {
                throw sepia::unwritable_file(filename);
            }
            output << "P6\n" << _width << " " << _height << "\n255\n";
            output.write(reinterpret_cast<const char*>(_bytes.data()), _bytes.size());
        }
    }

    protected:
    const uint16_t _width;
    const uint16_t _height;
    std::vector<uint8_t> _bytes;
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
         "                                               defaults to #f4c20d",
         "    -b color, --offcolor color             sets the color for OFF events",
         "                                               color must be formatted as #hhhhhh,",
         "                                               where h is an hexadecimal digit",
         "                                               defaults to #1e88e5",
         "    -c color, --idlecolor color            sets the background color",
         "                                               color must be formatted as #hhhhhh,",
         "                                               where h is an hexadecimal digit",
         "                                               defaults to #292929",
         "    -d digits, --digits digits             sets the number of digits in output filenames",
         "                                               ignored if the output is not a directory",
         "                                               defaults to 6",
         "    -e ratio, --discard-ratio ratio        sets the ratio of pixels discarded for tone mapping",
         "                                               ignored if the stream type is not atis",
         "                                               used for white (resp. black) if --white (resp. --black) is "
         "not set",
         "                                               defaults to 0.01",
         "    -w duration, --white duration          sets the white integration duration for tone mapping (timecode)",
         "                                               defaults to automatic discard calculation",
         "    -x duration, --black duration          sets the black integration duration for tone mapping (timecode)",
         "                                               defaults to automatic discard calculation",
         "    -j color, --atiscolor color            sets the background color for ATIS exposure measurements",
         "                                               color must be formatted as #hhhhhh,",
         "                                               where h is an hexadecimal digit",
         "                                               defaults to #000000",
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
            {"discard-ratio", {"e"}},
            {"white", {"w"}},
            {"black", {"x"}},
            {"atiscolor", {"j"}},
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
            switch (header.event_stream_type) {
                case sepia::type::generic:
                    throw std::runtime_error("unsupported event stream type 'generic'");
                case sepia::type::dvs: {
                    std::vector<std::pair<uint64_t, bool>> ts_and_ons(
                        header.width * header.height, {std::numeric_limits<uint64_t>::max(), false});
                    uint64_t frame_index = 0;
                    auto first_t = std::numeric_limits<uint64_t>::max();
                    frame output_frame(header.width, header.height);
                    sepia::join_observable<sepia::type::dvs>(std::move(input), header, [&](sepia::dvs_event event) {
                        if (first_t == std::numeric_limits<uint64_t>::max()) {
                            first_t = event.t;
                        }
                        const auto frame_t = first_t + frame_index * frametime;
                        if (event.t >= frame_t) {
                            output_frame.paste_ts_and_ons(
                                header.width,
                                header.height,
                                ts_and_ons,
                                0,
                                0,
                                decay_style,
                                parameter,
                                on_color,
                                off_color,
                                idle_color,
                                frame_t);
                            output_frame.write(output_directory, digits, frame_index);
                            ++frame_index;
                        }
                        ts_and_ons[event.x + event.y * header.width].first = event.t;
                        ts_and_ons[event.x + event.y * header.width].second = event.is_increase;
                    });
                    break;
                }
                case sepia::type::atis: {
                    uint64_t white = 0;
                    auto white_auto = true;
                    {
                        const auto name_and_argument = command.options.find("white");
                        if (name_and_argument != command.options.end()) {
                            white = timecode(name_and_argument->second).value();
                            white_auto = false;
                            if (white == 0) {
                                throw std::runtime_error("white must be larger than 0");
                            }
                        }
                    }
                    uint64_t black = 0;
                    auto black_auto = true;
                    {
                        const auto name_and_argument = command.options.find("black");
                        if (name_and_argument != command.options.end()) {
                            black = timecode(name_and_argument->second).value();
                            black_auto = false;
                            if (black == 0) {
                                throw std::runtime_error("black must be larger than 0");
                            }
                            if (!white_auto && white > black) {
                                throw std::runtime_error("white must be smaller than or equal to black");
                            }
                        }
                    }
                    auto discard_ratio = 0.01f;
                    if (white_auto || black_auto) {
                        const auto name_and_argument = command.options.find("discard-ratio");
                        if (name_and_argument != command.options.end()) {
                            discard_ratio = std::stof(name_and_argument->second);
                            if (discard_ratio < 0.0f || discard_ratio > 1.0f) {
                                throw std::runtime_error("discard-ratio must be in he range [0, 1]");
                            }
                        }
                    }
                    color atis_color(0x00, 0x00, 0x00);
                    {
                        const auto name_and_argument = command.options.find("atiscolor");
                        if (name_and_argument != command.options.end()) {
                            atis_color = color(name_and_argument->second);
                        }
                    }
                    std::vector<std::pair<uint64_t, bool>> ts_and_ons(
                        header.width * header.height, {std::numeric_limits<uint64_t>::max(), false});
                    std::vector<uint64_t> delta_ts(header.width * header.height, std::numeric_limits<uint64_t>::max());
                    uint64_t frame_index = 0;
                    auto first_t = std::numeric_limits<uint64_t>::max();
                    frame output_frame(header.width * 2, header.height);
                    sepia::join_observable<sepia::type::atis>(
                        std::move(input),
                        header,
                        tarsier::make_replicate<sepia::atis_event>(
                            [&](sepia::atis_event event) {
                                if (first_t == std::numeric_limits<uint64_t>::max()) {
                                    first_t = event.t;
                                }
                                const auto frame_t = first_t + frame_index * frametime;
                                if (event.t >= frame_t) {
                                    output_frame.paste_ts_and_ons(
                                        header.width,
                                        header.height,
                                        ts_and_ons,
                                        0,
                                        0,
                                        decay_style,
                                        parameter,
                                        on_color,
                                        off_color,
                                        idle_color,
                                        frame_t);
                                    output_frame.paste_delta_ts(
                                        header.width,
                                        header.height,
                                        delta_ts,
                                        header.width,
                                        0,
                                        white,
                                        white_auto,
                                        black,
                                        black_auto,
                                        discard_ratio,
                                        atis_color);
                                    output_frame.write(output_directory, digits, frame_index);
                                    ++frame_index;
                                }
                            },
                            sepia::make_split<sepia::type::atis>(
                                [&](sepia::dvs_event event) {
                                    ts_and_ons[event.x + event.y * header.width].first = event.t;
                                    ts_and_ons[event.x + event.y * header.width].second = event.is_increase;
                                },
                                tarsier::make_stitch<sepia::threshold_crossing, exposure_measurement>(
                                    header.width,
                                    header.height,
                                    [](sepia::threshold_crossing threshold_crossing,
                                       uint64_t delta_t) -> exposure_measurement {
                                        return {delta_t, threshold_crossing.x, threshold_crossing.y};
                                    },
                                    [&](exposure_measurement event) {
                                        delta_ts[event.x + event.y * header.width] = event.delta_t;
                                    }))));
                    break;
                }
                case sepia::type::color:
                    throw std::runtime_error("unsupported event stream type 'color'");
            }
        });
}
