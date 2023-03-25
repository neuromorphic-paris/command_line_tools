#include "../third_party/pontella/source/pontella.hpp"
#include "../third_party/sepia/source/sepia.hpp"
#include "timecode.hpp"
#include <deque>

// plot parameters
constexpr uint16_t x_offset = 50;
constexpr uint16_t y_offset = 50;
constexpr uint16_t font_size = 20;
constexpr std::array<float, 2> curves_strokes{1.0f, 2.0f};
constexpr float font_baseline_ratio = 0.3f;
constexpr float font_width_ratio = 0.5f;
constexpr float font_exponent_ratio = 0.8f;
constexpr float font_exponent_baseline_ratio = 0.5f;

/// event_rate represents a number of events per second.
SEPIA_PACK(struct event_rate {
    uint64_t t;
    double value;
});

/// t_range extracts the first and last timestamp.
template <sepia::type event_stream_type>
std::pair<uint64_t, uint64_t> typed_t_range(std::unique_ptr<std::istream> stream) {
    std::pair<uint64_t, uint64_t> result{std::numeric_limits<uint64_t>::max(), 0};
    sepia::join_observable<event_stream_type>(std::move(stream), [&](sepia::event<event_stream_type> event) {
        if (result.first == std::numeric_limits<uint64_t>::max()) {
            result.first = event.t;
        }
        result.second = event.t;
    });
    if (result.first == std::numeric_limits<uint64_t>::max()) {
        result.first = 0;
    }
    ++result.second;
    return result;
}

std::pair<uint64_t, uint64_t> t_range(const sepia::header& header, std::unique_ptr<std::istream> stream) {
    switch (header.event_stream_type) {
        case sepia::type::generic: {
            return typed_t_range<sepia::type::generic>(std::move(stream));
        }
        case sepia::type::dvs: {
            return typed_t_range<sepia::type::dvs>(std::move(stream));
        }
        case sepia::type::atis: {
            return typed_t_range<sepia::type::atis>(std::move(stream));
        }
        case sepia::type::color: {
            return typed_t_range<sepia::type::color>(std::move(stream));
        }
    }
}

/// compute_event_rate calculates the event rate.
template <sepia::type event_stream_type, typename HandleEventRate>
void typed_compute_event_rate(
    std::unique_ptr<std::istream> stream,
    uint64_t begin_t,
    uint64_t end_t,
    uint64_t tau,
    HandleEventRate&& handle_event_rate) {
    const auto time_scale = 1e6 / static_cast<double>(tau);
    std::deque<uint64_t> ts;
    uint64_t previous_t = 0;
    auto first_t = std::numeric_limits<uint64_t>::max();
    sepia::join_observable<event_stream_type>(std::move(stream), [&](sepia::event<event_stream_type> event) {
        if (event.t < begin_t) {
            return;
        }
        if (event.t >= end_t) {
            throw sepia::end_of_file();
        }
        if (first_t == std::numeric_limits<uint64_t>::max()) {
            first_t = event.t;
        }
        const auto t = event.t - first_t;
        if (t > previous_t && !ts.empty()) {
            const auto back_t = ts.back() + 1;
            if (back_t < ts.front() + tau) {
                handle_event_rate({back_t + first_t, static_cast<double>(ts.size()) * time_scale});
            }
            while (!ts.empty()) {
                const auto front_t = ts.front();
                if (front_t + tau > t) {
                    break;
                } else {
                    handle_event_rate({ts.front() + tau + first_t, static_cast<double>(ts.size()) * time_scale});
                    while (!ts.empty() && ts.front() == front_t) {
                        ts.pop_front();
                    }
                }
            }
        }
        ts.push_back(t);
        previous_t = t;
    });
}

template <typename HandleEventRate>
void compute_event_rate(
    const sepia::header& header,
    std::unique_ptr<std::istream> stream,
    uint64_t begin_t,
    uint64_t end_t,
    uint64_t tau,
    HandleEventRate&& handle_event_rate) {
    switch (header.event_stream_type) {
        case sepia::type::generic: {
            typed_compute_event_rate<sepia::type::generic>(
                std::move(stream), begin_t, end_t, tau, std::forward<HandleEventRate>(handle_event_rate));
            break;
        }
        case sepia::type::dvs: {
            typed_compute_event_rate<sepia::type::dvs>(
                std::move(stream), begin_t, end_t, tau, std::forward<HandleEventRate>(handle_event_rate));
            break;
        }
        case sepia::type::atis: {
            typed_compute_event_rate<sepia::type::atis>(
                std::move(stream), begin_t, end_t, tau, std::forward<HandleEventRate>(handle_event_rate));
            break;
        }
        case sepia::type::color: {
            typed_compute_event_rate<sepia::type::color>(
                std::move(stream), begin_t, end_t, tau, std::forward<HandleEventRate>(handle_event_rate));
            break;
        }
    }
}

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
    std::string hex() {
        std::stringstream stream;
        stream << "#" << std::setw(2) << std::setfill('0') << std::hex << static_cast<int32_t>(r) << std::setw(2)
               << std::setfill('0') << std::hex << static_cast<int32_t>(g) << std::setw(2) << std::setfill('0')
               << std::hex << static_cast<int32_t>(b);
        const auto result = stream.str();
        return result;
    }
};

int main(int argc, char* argv[]) {
    return pontella::main(
        {"event_rate plots the number of events per second (slidding time window).",
         "Syntax: ./event_rate [options] /path/to/input.es /path/to/output.svg",
         "Available options:",
         "    -b timestamp, --begin timestamp         ignores events before this timestamp (timecode)",
         "                                                defaults to 00:00:00",
         "    -e timestamp, --end timestamp           ignores events after this timestamp (timecode)",
         "                                                defaults to the end of the recording",
         "    -l tau, --long tau                      sets the long (foreground curve) time window (timecode)",
         "                                                defaults to 00:00:01.000",
         "    -s tau, --short tau                     sets the short (background curve) time window (timecode)",
         "                                                defaults to 00:00:00.010",
         "    -x size, --width size                   sets the output width in pixels",
         "                                                defaults to 1280",
         "    -y size, --height size                  sets the output height in pixels",
         "                                                defaults to 720",
         "    -a color, --longcolor color             sets the color of the long time window curve",
         "                                                color must be formatted as #hhhhhh,",
         "                                                where h is an hexadecimal digit",
         "                                                defaults to #4285F4",
         "    -c color, --shortcolor color            sets the color of the short time window curve",
         "                                                color must be formatted as #hhhhhh,",
         "                                                where h is an hexadecimal digit",
         "                                                defaults to #C4D7F5",
         "    -d color, --axiscolor color             sets the axis and labels colors",
         "                                                color must be formatted as #hhhhhh,",
         "                                                where h is an hexadecimal digit",
         "                                                defaults to #000000",
         "    -f color, --maingridcolor color         sets the main logarithmic grid color",
         "                                                color must be formatted as #hhhhhh,",
         "                                                where h is an hexadecimal digit",
         "                                                defaults to #555555",
         "    -g color, --secondarygridcolor color    sets the secondary logarithmic grid color",
         "                                                color must be formatted as #hhhhhh,",
         "                                                where h is an hexadecimal digit",
         "                                                defaults to #DDDDDD",
         "    -h, --help                              shows this help message"},
        argc,
        argv,
        2,
        {
            {"begin", {"b"}},
            {"end", {"e"}},
            {"long", {"l"}},
            {"short", {"s"}},
            {"width", {"x"}},
            {"height", {"y"}},
            {"longcolor", {"a"}},
            {"shortcolor", {"c"}},
            {"axiscolor", {"d"}},
            {"maingridcolor", {"f"}},
            {"secondarygridcolor", {"g"}},
        },
        {},
        [](pontella::command command) {
            uint64_t begin_t = 0;
            {
                const auto name_and_argument = command.options.find("begin");
                if (name_and_argument != command.options.end()) {
                    begin_t = timecode(name_and_argument->second).value();
                }
            }
            auto end_t = std::numeric_limits<uint64_t>::max();
            {
                const auto name_and_argument = command.options.find("end");
                if (name_and_argument != command.options.end()) {
                    end_t = timecode(name_and_argument->second).value();
                    if (end_t <= begin_t) {
                        throw std::runtime_error("end must be strictly larger than begin");
                    }
                }
            }
            uint64_t long_tau = 1000000;
            {
                const auto name_and_argument = command.options.find("long");
                if (name_and_argument != command.options.end()) {
                    long_tau = timecode(name_and_argument->second).value();
                    if (long_tau == 0) {
                        throw std::runtime_error("long must be larger than 0");
                    }
                }
            }
            uint64_t short_tau = 10000;
            {
                const auto name_and_argument = command.options.find("short");
                if (name_and_argument != command.options.end()) {
                    short_tau = timecode(name_and_argument->second).value();
                    if (short_tau == 0) {
                        throw std::runtime_error("short must be larger than 0");
                    }
                }
            }
            std::size_t width = 1280;
            {
                const auto name_and_argument = command.options.find("width");
                if (name_and_argument != command.options.end()) {
                    width = std::stoull(name_and_argument->second);
                }
            }
            std::size_t height = 720;
            {
                const auto name_and_argument = command.options.find("height");
                if (name_and_argument != command.options.end()) {
                    height = std::stoull(name_and_argument->second);
                }
            }
            color long_color(0x42, 0x85, 0xF4);
            {
                const auto name_and_argument = command.options.find("longcolor");
                if (name_and_argument != command.options.end()) {
                    long_color = color(name_and_argument->second);
                }
            }
            color short_color(0xC4, 0xD7, 0xF5);
            {
                const auto name_and_argument = command.options.find("shortcolor");
                if (name_and_argument != command.options.end()) {
                    short_color = color(name_and_argument->second);
                }
            }
            color axis_color(0x00, 0x00, 0x00);
            {
                const auto name_and_argument = command.options.find("axiscolor");
                if (name_and_argument != command.options.end()) {
                    axis_color = color(name_and_argument->second);
                }
            }
            color main_grid_color(0x55, 0x55, 0x55);
            {
                const auto name_and_argument = command.options.find("maingridcolor");
                if (name_and_argument != command.options.end()) {
                    main_grid_color = color(name_and_argument->second);
                }
            }
            color secondary_grid_color(0xDD, 0xDD, 0xDD);
            {
                const auto name_and_argument = command.options.find("secondarygridcolor");
                if (name_and_argument != command.options.end()) {
                    secondary_grid_color = color(name_and_argument->second);
                }
            }
            const auto header = sepia::read_header(sepia::filename_to_ifstream(command.arguments[0]));
            auto output = sepia::filename_to_ofstream(command.arguments[1]);
            std::pair<uint64_t, uint64_t> first_and_last_t{begin_t, end_t};
            if (command.options.find("begin") == command.options.end()
                || command.options.find("end") == command.options.end()) {
                const auto recording_first_and_last_t =
                    t_range(header, sepia::filename_to_ifstream(command.arguments[0]));
                if (command.options.find("begin") == command.options.end()) {
                    first_and_last_t.first = recording_first_and_last_t.first;
                }
                if (command.options.find("end") == command.options.end()) {
                    first_and_last_t.second = recording_first_and_last_t.second;
                }
            }
            if (first_and_last_t.second <= first_and_last_t.first) {
                throw std::runtime_error("begin must be smaller than than the end of the recording");
            }
            double minimum = std::numeric_limits<double>::infinity();
            double maximum = -std::numeric_limits<double>::infinity();
            std::array<uint64_t, 2> type_to_tau = {long_tau, short_tau};
            std::array<std::vector<std::pair<double, double>>, 2> type_to_minmaxes;
            for (const std::size_t index : {0, 1}) {
                const auto tau = type_to_tau[index];
                type_to_minmaxes[index].resize(
                    width - 1 - x_offset,
                    {std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()});
                const auto pixel_window = static_cast<uint64_t>(std::round(
                    static_cast<double>(first_and_last_t.second - first_and_last_t.first) / (width - 1 - x_offset)));
                compute_event_rate(
                    header,
                    sepia::filename_to_ifstream(command.arguments[0]),
                    begin_t,
                    end_t,
                    tau,
                    [&](event_rate event) {
                        if (event.value > 0.0) {
                            minimum = std::min(minimum, event.value);
                        }
                        maximum = std::max(maximum, event.value);
                        const auto pixel_index = std::min(
                            static_cast<std::size_t>((event.t - first_and_last_t.first) / pixel_window),
                            width - 1 - x_offset - 1);
                        type_to_minmaxes[index][pixel_index].first =
                            std::min(type_to_minmaxes[index][pixel_index].first, event.value);
                        type_to_minmaxes[index][pixel_index].second =
                            std::max(type_to_minmaxes[index][pixel_index].second, event.value);
                    });
            }
            if (minimum == std::numeric_limits<double>::infinity()) {
                minimum = 1.0;
            }
            if (maximum <= minimum) {
                maximum = minimum * 10.0;
            }
            const auto log_minimum = static_cast<uint32_t>(std::floor(std::log10(minimum)));
            const auto log_maximum = static_cast<uint32_t>(std::ceil(std::log10(maximum)));
            auto value_to_y = [=](double value) -> double {
                return (static_cast<double>(font_size)
                        - (static_cast<double>(height) - 1.0 - static_cast<double>(y_offset)))
                           / (log_maximum - log_minimum) * (std::log10(value) - log_maximum)
                       + font_size;
            };
            for (auto& minmaxes : type_to_minmaxes) {
                for (std::size_t pixel_index = 0; pixel_index < minmaxes.size(); ++pixel_index) {
                    auto& minmax = minmaxes[pixel_index];
                    if (minmax.first == std::numeric_limits<double>::infinity()
                        || minmax.second == -std::numeric_limits<double>::infinity()) {
                        if (pixel_index == 0) {
                            minmax.first = minimum;
                            minmax.second = minimum;
                        } else {
                            minmax.first = minmaxes[pixel_index - 1].first;
                            minmax.second = minmaxes[pixel_index - 1].second;
                        }
                    } else if (minmax.first == 0) {
                        minmax.first = minimum;
                    }
                    if (minmax.second < minmax.first) {
                        minmax.second = maximum;
                    }
                }
            }
            *output << "<svg version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << width << " "
                    << height << "\">\n";
            for (uint32_t index = log_minimum + 1; index < log_maximum + 1; ++index) {
                *output << "<path fill=\"#00000000\" stroke=\"" << main_grid_color.hex()
                        << "\" stroke-width=\"1\" d=\"M" << x_offset << "," << value_to_y(std::pow(10, index)) << " L"
                        << (width - 1) << "," << value_to_y(std::pow(10, index)) << "\" />\n";
            }
            for (uint32_t index = log_minimum; index < log_maximum + 1; ++index) {
                const auto digits = std::to_string(index).size();
                *output << "<text text-anchor=\"end\" x=\""
                        << x_offset - std::round((digits * font_exponent_ratio + 1) * font_size * font_width_ratio)
                        << "\" y=\"" << (value_to_y(std::pow(10, index)) + font_size * font_baseline_ratio)
                        << "\" font-size=\"" << font_size << "px\" fill=\"" << axis_color.hex() << "\">10</text>\n";
                *output << "<text text-anchor=\"end\" x=\"" << x_offset - std::round(font_size * font_width_ratio)
                        << "\" y=\""
                        << (value_to_y(std::pow(10, index))
                            + font_size * (font_baseline_ratio - font_exponent_baseline_ratio))
                        << "\" font-size=\"" << std::round(font_size * font_exponent_ratio) << "px\" fill=\""
                        << axis_color.hex() << "\">" << index << "</text>\n";
            }
            *output << "<text text-anchor=\"begin\" x=\"" << x_offset << "\" y=\""
                    << (height - 1 - y_offset + font_size * (1 + font_baseline_ratio)) << "\" font-size=\"" << font_size
                    << "px\" fill=\"" << axis_color.hex() << "\">"
                    << timecode(first_and_last_t.first).to_timecode_string() << "</text>\n";
            *output << "<text text-anchor=\"end\" x=\"" << (width - 1) << "\" y=\""
                    << (height - 1 - y_offset + font_size * (1 + font_baseline_ratio)) << "\" font-size=\"" << font_size
                    << "px\" fill=\"" << axis_color.hex() << "\">"
                    << timecode(first_and_last_t.second).to_timecode_string() << "</text>\n";
            for (uint32_t index = log_minimum; index < log_maximum; ++index) {
                for (uint32_t multiplier = 2; multiplier < 10; ++multiplier) {
                    *output << "<path fill=\"#00000000\" stroke=\"" << secondary_grid_color.hex()
                            << "\"  stroke-width=\"1\" d=\"M" << x_offset << ","
                            << value_to_y(std::pow(10, index) * multiplier) << " L" << (width - 1) << ","
                            << value_to_y(std::pow(10, index) * multiplier) << "\" />\n";
                }
            }
            std::array<std::string, 2> curves_colors{long_color.hex(), short_color.hex()};
            for (const std::size_t index : {1, 0}) {
                *output << "<path fill=\"" << curves_colors[index] << "\" stroke=\"" << curves_colors[index]
                        << "\" stroke-width=\"" << curves_strokes[index] << "\" d=\"";
                for (std::size_t pixel_index = 0; pixel_index < type_to_minmaxes[index].size(); ++pixel_index) {
                    *output << (pixel_index == 0 ? "M" : "L") << (x_offset + pixel_index) << ","
                            << value_to_y(type_to_minmaxes[index][pixel_index].first) << " ";
                }
                for (std::size_t pixel_index = 0; pixel_index < type_to_minmaxes[index].size(); ++pixel_index) {
                    *output << "L" << (x_offset + type_to_minmaxes[index].size() - 1 - pixel_index) << ","
                            << value_to_y(
                                   type_to_minmaxes[index][type_to_minmaxes[index].size() - 1 - pixel_index].second)
                            << " ";
                }
                *output << "Z\" />\n";
            }
            *output << "<path fill=\"#00000000\" stroke=\"" << axis_color.hex() << "\" stroke-width=\"2\" d=\"M"
                    << x_offset << ",0 L" << x_offset << "," << (height - y_offset) << " L" << (width - 1) << ","
                    << (height - 1 - y_offset) << "\" />\n";
            *output << "</svg>";
        });
}
