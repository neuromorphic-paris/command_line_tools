#include "../third_party/pontella/source/pontella.hpp"
#include "../third_party/sepia/source/sepia.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb_truetype.hpp"
#include "../third_party/tarsier/source/replicate.hpp"
#include "../third_party/tarsier/source/stitch.hpp"
#include "font.hpp"
#include "timecode.hpp"
#include <iomanip>
#include <sstream>
#include <tuple>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/// exposure_measurement holds the parameters of an absolute luminance sample.
SEPIA_PACK(struct exposure_measurement {
    uint64_t delta_t;
    uint16_t x;
    uint16_t y;
});

constexpr uint16_t font_left = 20;
constexpr uint16_t font_top = 20;
constexpr uint16_t font_size = 30;

enum class style { exponential, linear, window, cumulative, cumulative_shared };

struct state {
    std::vector<std::pair<uint64_t, bool>> ts_and_ons;
    std::vector<std::tuple<uint64_t, double, bool>> ts_and_activities_and_ons;
    std::vector<std::pair<uint64_t, double>> on_ts_and_activities;
    std::vector<std::pair<uint64_t, double>> off_ts_and_activities;
};

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

class frame {
    public:
    frame(uint16_t width, uint16_t height, uint16_t scale) :
        _width(width * scale), _height(height * scale), _scale(scale), _bytes((width * scale) * (height * scale) * 3) {}
    frame(const frame&) = delete;
    frame(frame&& other) = delete;
    frame& operator=(const frame&) = delete;
    frame& operator=(frame&& other) = delete;
    virtual ~frame() {}

    virtual void paste_state(
        uint16_t width,
        uint16_t height,
        const state& style_state,
        uint16_t x_offset,
        uint16_t y_offset,
        style decay_style,
        uint64_t tau,
        color on_color,
        color off_color,
        color idle_color,
        uint64_t frame_t,
        float cumulative_ratio,
        float lambda_maximum,
        bool lambda_maximum_auto) {
        if (decay_style == style::cumulative || decay_style == style::cumulative_shared) {
            std::vector<std::pair<float, bool>> lambdas_and_ons(width * height);
            for (std::size_t index = 0; index < width * height; ++index) {
                switch (decay_style) {
                    case style::cumulative: {
                        const auto on_lambda =
                            style_state.on_ts_and_activities[index].second
                            * std::exp(
                                -static_cast<float>(frame_t - 1 - style_state.on_ts_and_activities[index].first)
                                / static_cast<float>(tau));
                        const auto off_lambda =
                            style_state.off_ts_and_activities[index].second
                            * std::exp(
                                -static_cast<float>(frame_t - 1 - style_state.off_ts_and_activities[index].first)
                                / static_cast<float>(tau));
                        if (off_lambda > on_lambda) {
                            lambdas_and_ons[index].first = off_lambda;
                            lambdas_and_ons[index].second = false;
                        } else {
                            lambdas_and_ons[index].first = on_lambda;
                            lambdas_and_ons[index].second = true;
                        }
                        break;
                    }
                    case style::cumulative_shared: {
                        lambdas_and_ons[index].first =
                            std::get<1>(style_state.ts_and_activities_and_ons[index])
                            * std::exp(
                                -static_cast<float>(
                                    frame_t - 1 - std::get<0>(style_state.ts_and_activities_and_ons[index]))
                                / static_cast<float>(tau));
                        lambdas_and_ons[index].second = std::get<2>(style_state.ts_and_activities_and_ons[index]);
                        break;
                    }
                    default:
                        break;
                }
            }
            if (lambda_maximum_auto) {
                std::vector<float> sorted_lambdas(lambdas_and_ons.size());
                std::transform(
                    lambdas_and_ons.begin(),
                    lambdas_and_ons.end(),
                    sorted_lambdas.begin(),
                    [](const std::pair<float, bool> lambda_and_on) { return lambda_and_on.first; });
                std::sort(sorted_lambdas.begin(), sorted_lambdas.end());
                lambda_maximum = std::max(
                    1.0f,
                    sorted_lambdas[static_cast<std::size_t>((sorted_lambdas.size() - 1) * (1.0f - cumulative_ratio))]);
            }
            for (uint16_t y = 0; y < height; ++y) {
                for (uint16_t x = 0; x < width; ++x) {
                    const auto lambda_and_on = lambdas_and_ons[x + y * width];
                    const auto scaled_lambda =
                        lambda_and_on.first > lambda_maximum ? 1.0 : lambda_and_on.first / lambda_maximum;
                    const auto r = idle_color.mix_r(lambda_and_on.second ? on_color : off_color, scaled_lambda);
                    const auto g = idle_color.mix_g(lambda_and_on.second ? on_color : off_color, scaled_lambda);
                    const auto b = idle_color.mix_b(lambda_and_on.second ? on_color : off_color, scaled_lambda);
                    for (uint16_t y_scale = 0; y_scale < _scale; ++y_scale) {
                        for (uint16_t x_scale = 0; x_scale < _scale; ++x_scale) {
                            const auto index = ((x + x_offset) * _scale + x_scale
                                                + (_height - _scale - (y + y_offset) * _scale + y_scale) * _width)
                                               * 3;
                            _bytes[index] = r;
                            _bytes[index + 1] = g;
                            _bytes[index + 2] = b;
                        }
                    }
                }
            }
        } else {
            for (uint16_t y = 0; y < height; ++y) {
                for (uint16_t x = 0; x < width; ++x) {
                    const auto& t_and_on = style_state.ts_and_ons[x + y * width];
                    auto lambda = 0.0f;
                    if (t_and_on.first < std::numeric_limits<uint64_t>::max()) {
                        switch (decay_style) {
                            case style::exponential:
                                lambda = std::exp(
                                    -static_cast<float>(frame_t - 1 - t_and_on.first) / static_cast<float>(tau));
                                break;
                            case style::linear:
                                lambda = t_and_on.first + 2 * tau > frame_t - 1 ?
                                             static_cast<float>(t_and_on.first + 2 * tau - (frame_t - 1))
                                                 / static_cast<float>(2 * tau) :
                                             0.0f;
                                break;
                            case style::window:
                                lambda = t_and_on.first + tau > frame_t - 1 ? 1.0f : 0.0f;
                                break;
                            default:
                                break;
                        }
                    }
                    const auto r = idle_color.mix_r(t_and_on.second ? on_color : off_color, lambda);
                    const auto g = idle_color.mix_g(t_and_on.second ? on_color : off_color, lambda);
                    const auto b = idle_color.mix_b(t_and_on.second ? on_color : off_color, lambda);
                    for (uint16_t y_scale = 0; y_scale < _scale; ++y_scale) {
                        for (uint16_t x_scale = 0; x_scale < _scale; ++x_scale) {
                            const auto index = ((x + x_offset) * _scale + x_scale
                                                + (_height - _scale - (y + y_offset) * _scale + y_scale) * _width)
                                               * 3;
                            _bytes[index] = r;
                            _bytes[index + 1] = g;
                            _bytes[index + 2] = b;
                        }
                    }
                }
            }
        }
    }

    virtual void paste_delta_ts(
        uint16_t width,
        uint16_t height,
        const std::vector<uint64_t>& delta_ts,
        uint16_t x_offset,
        uint16_t y_offset,
        uint64_t black,
        bool black_auto,
        uint64_t white,
        bool white_auto,
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
                auto black_candidate = sorted_delta_ts[static_cast<std::size_t>((size - 1) * (1.0f - discard_ratio))];
                auto white_candidate = sorted_delta_ts[static_cast<std::size_t>((size - 1) * discard_ratio + 0.5f)];
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
                const auto delta_t = delta_ts[x + y * width];
                if (delta_t == std::numeric_limits<uint64_t>::max()) {
                    for (uint16_t y_scale = 0; y_scale < _scale; ++y_scale) {
                        for (uint16_t x_scale = 0; x_scale < _scale; ++x_scale) {
                            const auto index = ((x + x_offset) * _scale + x_scale
                                                + (_height - _scale - (y + y_offset) * _scale + y_scale) * _width)
                                               * 3;
                            _bytes[index] = atis_color.r;
                            _bytes[index + 1] = atis_color.g;
                            _bytes[index + 2] = atis_color.b;
                        }
                    }
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
                    for (uint16_t y_scale = 0; y_scale < _scale; ++y_scale) {
                        for (uint16_t x_scale = 0; x_scale < _scale; ++x_scale) {
                            const auto index = ((x + x_offset) * _scale + x_scale
                                                + (_height - _scale - (y + y_offset) * _scale + y_scale) * _width)
                                               * 3;
                            _bytes[index] = value;
                            _bytes[index + 1] = value;
                            _bytes[index + 2] = value;
                        }
                    }
                }
            }
        }
    }

    virtual void paste_timecode(uint16_t left, uint16_t top, uint16_t font_size, uint64_t frame_t) {
        if (!_fontinfo) {
            _fontinfo = std::unique_ptr<stbtt_fontinfo>(new stbtt_fontinfo);
            if (stbtt_InitFont(_fontinfo.get(), monaco_bytes.data(), 0) == 0) {
                throw std::runtime_error("loading the font failed");
            }
        }
        int32_t ascent = 0;
        int32_t descent = 0;
        int32_t line_gap = 0;
        stbtt_GetFontVMetrics(_fontinfo.get(), &ascent, &descent, &line_gap);
        const auto scale = stbtt_ScaleForPixelHeight(_fontinfo.get(), font_size);
        const auto timecode_string = timecode(frame_t).to_timecode_string();
        int32_t width = 0;
        int32_t height = 0;
        for (std::size_t index = 0; index < timecode_string.size(); ++index) {
            int32_t advance_width = 0;
            int32_t left_side_bearing = 0;
            stbtt_GetCodepointHMetrics(_fontinfo.get(), timecode_string[index], &advance_width, &left_side_bearing);
            int32_t top = 0;
            int32_t left = 0;
            int32_t bottom = 0;
            int32_t right = 0;
            stbtt_GetCodepointBitmapBox(
                _fontinfo.get(), timecode_string[index], scale, scale, &left, &top, &right, &bottom);
            height = std::max(height, static_cast<int32_t>(std::roundf(ascent * scale)) + bottom);
            width += static_cast<int32_t>(std::roundf(advance_width * scale));
            if (index < timecode_string.size() - 1) {
                width += static_cast<int32_t>(std::roundf(
                    stbtt_GetCodepointKernAdvance(_fontinfo.get(), timecode_string[index], timecode_string[index + 1])
                    * scale));
            }
        }
        std::vector<uint8_t> bitmap(width * height);
        int32_t x = 0;
        for (std::size_t index = 0; index < timecode_string.size(); ++index) {
            int32_t advance_width = 0;
            int32_t left_side_bearing = 0;
            stbtt_GetCodepointHMetrics(_fontinfo.get(), timecode_string[index], &advance_width, &left_side_bearing);
            int32_t top = 0;
            int32_t left = 0;
            int32_t bottom = 0;
            int32_t right = 0;
            stbtt_GetCodepointBitmapBox(
                _fontinfo.get(), timecode_string[index], scale, scale, &left, &top, &right, &bottom);
            stbtt_MakeCodepointBitmap(
                _fontinfo.get(),
                bitmap.data() + x + static_cast<int32_t>(std::roundf(left_side_bearing * scale))
                    + (static_cast<int32_t>(std::roundf(ascent * scale)) + top) * width,
                right - left,
                bottom - top,
                width,
                scale,
                scale,
                timecode_string[index]);
            if (index < timecode_string.size() - 1) {
                x += static_cast<int32_t>(std::roundf(advance_width * scale))
                     + static_cast<int32_t>(std::roundf(
                         stbtt_GetCodepointKernAdvance(
                             _fontinfo.get(), timecode_string[index], timecode_string[index + 1])
                         * scale));
            }
        }
        for (int32_t y = 0; y < height; ++y) {
            for (int32_t x = 0; x < width; ++x) {
                const auto frame_x = x + left;
                const auto frame_y = y + top;
                if (frame_x >= 0 && frame_x < _width && frame_y >= 0 && frame_y < _height) {
                    const auto index = (frame_x + frame_y * _width) * 3;
                    const auto alpha = bitmap[x + y * width];
                    _bytes[index] = _bytes[index] * (255 - alpha) / 255 + alpha;
                    _bytes[index + 1] = _bytes[index + 1] * (255 - alpha) / 255 + alpha;
                    _bytes[index + 2] = _bytes[index + 2] * (255 - alpha) / 255 + alpha;
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
    const uint16_t _scale;
    std::vector<uint8_t> _bytes;
    std::unique_ptr<stbtt_fontinfo> _fontinfo;
};

int main(int argc, char* argv[]) {
    return pontella::main(
        {"es_to_frames converts an Event Stream file to video frames",
         "    Frames use the P6 Netpbm format (https://en.wikipedia.org/wiki/Netpbm) if the output is a directory",
         "    Otherwise, the output consists in raw rgb24 frames",
         "Syntax: ./es_to_frames [options]",
         "Available options:",
         "    -i file, --input file                  sets the path to the input .es file",
         "                                               defaults to standard input",
         "    -o directory, --output directory       sets the path to the output directory",
         "                                               defaults to standard output",
         "    -b timestamp, --begin timestamp        ignores events before this timestamp (timecode)",
         "                                               defaults to 00:00:00",
         "    -e timestamp, --end timestamp          ignores events after this timestamp (timecode)",
         "                                               defaults to the end of the recording",
         "    -f frametime, --frametime frametime    sets the time between two frames (timecode)",
         "                                               defaults to 00:00:00.020",
         "    -c scale, --scale scale                scale up the output by the given integer factor",
         "                                               defaults to 1",
         "    -s style, --style style                selects the decay function",
         "                                               one of exponential (default), linear, window,",
         "                                               cumulative, cumulative-shared",
         "    -t tau, --tau tau                      sets the decay function parameter (timecode)",
         "                                               defaults to 00:00:00.200",
         "                                               if style is `exponential`, the decay is set to tau",
         "                                               if style is `linear`, the decay is set to tau * 2",
         "                                               if style is `window`, the time window is set to tau",
         "                                               if style is `cumulative`, the decay is set to tau",
         "                                               if style is `cumulative-shared`, the decay is set to tau",
         "    -j color, --oncolor color              sets the color for ON events",
         "                                               color must be formatted as #hhhhhh,",
         "                                               where h is an hexadecimal digit",
         "                                               defaults to #f4c20d",
         "    -k color, --offcolor color             sets the color for OFF events",
         "                                               color must be formatted as #hhhhhh,",
         "                                               where h is an hexadecimal digit",
         "                                               defaults to #1e88e5",
         "    -l color, --idlecolor color            sets the background color",
         "                                               color must be formatted as #hhhhhh,",
         "                                               where h is an hexadecimal digit",
         "                                               defaults to #191919",
         "    -m ratio, --cumulative-ratio ratio     sets the ratio of pixels discarded for cumulative mapping",
         "                                               ignored if the style type is not",
         "                                               cumulative or cumulative-shared",
         "                                               used for lambda-max if --lambda-max is not set",
         "                                               defaults to 0.01",
         "    -n, --lambda-max                       sets the cumulative mapping maximum activity",
         "                                               defaults to automatic discard calculation",
         "    -a, --add-timecode                     adds a timecode overlay",
         "    -d digits, --digits digits             sets the number of digits in output filenames",
         "                                               ignored if the output is not a directory",
         "                                               defaults to 6",
         "    -r ratio, --discard-ratio ratio        sets the ratio of pixels discarded for tone mapping",
         "                                               ignored if the stream type is not atis",
         "                                               used for black (resp. white) if --black (resp. --white)",
         "                                               is not set",
         "                                               defaults to 0.01",
         "    -v duration, --black duration          sets the black integration duration for tone mapping (timecode)",
         "                                               defaults to automatic discard calculation",
         "    -w duration, --white duration          sets the white integration duration for tone mapping (timecode)",
         "                                               defaults to automatic discard calculation",
         "    -x color, --atiscolor color            sets the background color for ATIS exposure measurements",
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
            {"begin", {"b"}},
            {"end", {"e"}},
            {"frametime", {"f"}},
            {"scale", {"c"}},
            {"style", {"s"}},
            {"tau", {"t"}},
            {"oncolor", {"j"}},
            {"offcolor", {"k"}},
            {"idlecolor", {"l"}},
            {"cumulative-ratio", {"m"}},
            {"lambda-max", {"n"}},
            {"digits", {"d"}},
            {"discard-ratio", {"r"}},
            {"black", {"v"}},
            {"white", {"w"}},
            {"atiscolor", {"x"}},
        },
        {
            {"add-timecode", {"a"}},
        },
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
            uint16_t scale = 1;
            {
                const auto name_and_argument = command.options.find("scale");
                if (name_and_argument != command.options.end()) {
                    const auto scale_candidate = std::stoull(name_and_argument->second);
                    if (scale_candidate == 0 || scale_candidate >= 65536) {
                        throw std::runtime_error("scale must be larger than 0 and smaller than 65536");
                    }
                    scale = static_cast<uint16_t>(scale_candidate);
                }
            }
            color on_color(0xf4, 0xc2, 0x0d);
            {
                const auto name_and_argument = command.options.find("oncolor");
                if (name_and_argument != command.options.end()) {
                    on_color = color(name_and_argument->second);
                }
            }
            color off_color(0x1e, 0x88, 0xe5);
            {
                const auto name_and_argument = command.options.find("offcolor");
                if (name_and_argument != command.options.end()) {
                    off_color = color(name_and_argument->second);
                }
            }
            color idle_color(0x29, 0x29, 0x29);
            {
                const auto name_and_argument = command.options.find("idlecolor");
                if (name_and_argument != command.options.end()) {
                    idle_color = color(name_and_argument->second);
                }
            }
            auto cumulative_ratio = 0.01f;
            {
                const auto name_and_argument = command.options.find("cumulative-ratio");
                if (name_and_argument != command.options.end()) {
                    cumulative_ratio = std::stof(name_and_argument->second);
                    if (cumulative_ratio < 0.0f || cumulative_ratio > 1.0f) {
                        throw std::runtime_error("cumulative-ratio must be in the range [0, 1]");
                    }
                }
            }
            auto lambda_maximum = 0.0f;
            auto lambda_maximum_auto = true;
            {
                const auto name_and_argument = command.options.find("lambda-max");
                if (name_and_argument != command.options.end()) {
                    lambda_maximum = std::stof(name_and_argument->second);
                    lambda_maximum_auto = false;
                    if (lambda_maximum < 0.0f) {
                        throw std::runtime_error("lambda-max must be larger than 0");
                    }
                }
            }
            const auto add_timecode = command.flags.find("add-timecode") != command.flags.end();
            auto decay_style = style::exponential;
            {
                const auto name_and_argument = command.options.find("style");
                if (name_and_argument != command.options.end()) {
                    if (name_and_argument->second == "linear") {
                        decay_style = style::linear;
                    } else if (name_and_argument->second == "window") {
                        decay_style = style::window;
                    } else if (name_and_argument->second == "cumulative") {
                        decay_style = style::cumulative;
                    } else if (name_and_argument->second == "cumulative-shared") {
                        decay_style = style::cumulative_shared;
                    } else if (name_and_argument->second != "exponential") {
                        throw std::runtime_error("style must be one of {exponential, linear, window}");
                    }
                }
            }
            uint64_t tau = 200000;
            {
                const auto name_and_argument = command.options.find("tau");
                if (name_and_argument != command.options.end()) {
                    tau = timecode(name_and_argument->second).value();
                    if (tau == 0) {
                        throw std::runtime_error("tau must be larger than 0");
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
#ifdef _WIN32
                    _setmode(_fileno(stdin), _O_BINARY);
#endif
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
                    state style_state;
                    switch (decay_style) {
                        case style::cumulative:
                            style_state.on_ts_and_activities.resize(header.width * header.height, {0, 0.0});
                            style_state.off_ts_and_activities.resize(header.width * header.height, {0, 0.0});
                            break;
                        case style::cumulative_shared:
                            style_state.ts_and_activities_and_ons.resize(header.width * header.height, {0, 0.0, false});
                            break;
                        default:
                            style_state.ts_and_ons.resize(
                                header.width * header.height, {std::numeric_limits<uint64_t>::max(), false});
                            break;
                    }
                    uint64_t frame_index = 0;
                    auto first_t = std::numeric_limits<uint64_t>::max();
                    frame output_frame(header.width, header.height, scale);
                    sepia::join_observable<sepia::type::dvs>(std::move(input), header, [&](sepia::dvs_event event) {
                        if (event.t < begin_t) {
                            return;
                        }
                        if (event.t >= end_t) {
                            throw sepia::end_of_file();
                        }
                        if (first_t == std::numeric_limits<uint64_t>::max()) {
                            first_t = event.t;
                        }
                        auto frame_t = first_t + frame_index * frametime;
                        while (event.t >= frame_t) {
                            output_frame.paste_state(
                                header.width,
                                header.height,
                                style_state,
                                0,
                                0,
                                decay_style,
                                tau,
                                on_color,
                                off_color,
                                idle_color,
                                frame_t,
                                cumulative_ratio,
                                lambda_maximum,
                                lambda_maximum_auto);
                            if (add_timecode) {
                                output_frame.paste_timecode(font_left, font_top, font_size, frame_t);
                            }
                            output_frame.write(output_directory, digits, frame_index);
                            ++frame_index;
                            frame_t = first_t + frame_index * frametime;
                        }
                        const auto index = event.x + event.y * header.width;
                        switch (decay_style) {
                            case style::cumulative:
                                if (event.is_increase) {
                                    style_state.on_ts_and_activities[index].second =
                                        style_state.on_ts_and_activities[index].second
                                            * std::exp(
                                                -static_cast<float>(
                                                    event.t - style_state.on_ts_and_activities[index].first)
                                                / static_cast<float>(tau))
                                        + 1.0;
                                    style_state.on_ts_and_activities[index].first = event.t;
                                } else {
                                    style_state.off_ts_and_activities[index].second =
                                        style_state.off_ts_and_activities[index].second
                                            * std::exp(
                                                -static_cast<float>(
                                                    event.t - style_state.off_ts_and_activities[index].first)
                                                / static_cast<float>(tau))
                                        + 1.0;
                                    style_state.off_ts_and_activities[index].first = event.t;
                                }
                                break;
                            case style::cumulative_shared:
                                std::get<1>(style_state.ts_and_activities_and_ons[index]) =
                                    std::get<1>(style_state.ts_and_activities_and_ons[index])
                                        * std::exp(
                                            -static_cast<float>(
                                                event.t - std::get<0>(style_state.ts_and_activities_and_ons[index]))
                                            / static_cast<float>(tau))
                                    + 1.0;
                                std::get<0>(style_state.ts_and_activities_and_ons[index]) = event.t;
                                std::get<2>(style_state.ts_and_activities_and_ons[index]) = event.is_increase;
                                break;
                            default:
                                style_state.ts_and_ons[index].first = event.t;
                                style_state.ts_and_ons[index].second = event.is_increase;
                                break;
                        }
                    });
                    break;
                }
                case sepia::type::atis: {
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
                        }
                    }
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
                            if (!black_auto && white > black) {
                                throw std::runtime_error("white must be smaller than or equal to black");
                            }
                        }
                    }
                    auto discard_ratio = 0.01f;
                    if (black_auto || white_auto) {
                        const auto name_and_argument = command.options.find("discard-ratio");
                        if (name_and_argument != command.options.end()) {
                            discard_ratio = std::stof(name_and_argument->second);
                            if (discard_ratio < 0.0f || discard_ratio > 1.0f) {
                                throw std::runtime_error("discard-ratio must be in the range [0, 1]");
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
                    state style_state;
                    switch (decay_style) {
                        case style::cumulative:
                            style_state.on_ts_and_activities.resize(header.width * header.height, {0, 0.0});
                            style_state.off_ts_and_activities.resize(header.width * header.height, {0, 0.0});
                            break;
                        case style::cumulative_shared:
                            style_state.ts_and_activities_and_ons.resize(header.width * header.height, {0, 0.0, false});
                            break;
                        default:
                            style_state.ts_and_ons.resize(
                                header.width * header.height, {std::numeric_limits<uint64_t>::max(), false});
                            break;
                    }
                    std::vector<uint64_t> delta_ts(header.width * header.height, std::numeric_limits<uint64_t>::max());
                    uint64_t frame_index = 0;
                    auto first_t = std::numeric_limits<uint64_t>::max();
                    frame output_frame(header.width * 2, header.height, scale);
                    sepia::join_observable<sepia::type::atis>(
                        std::move(input),
                        header,
                        tarsier::make_replicate<sepia::atis_event>(
                            [&](sepia::atis_event event) {
                                if (event.t < begin_t) {
                                    return;
                                }
                                if (event.t >= end_t) {
                                    throw sepia::end_of_file();
                                }
                                if (first_t == std::numeric_limits<uint64_t>::max()) {
                                    first_t = event.t;
                                }
                                auto frame_t = first_t + frame_index * frametime;
                                while (event.t >= frame_t) {
                                    output_frame.paste_state(
                                        header.width,
                                        header.height,
                                        style_state,
                                        0,
                                        0,
                                        decay_style,
                                        tau,
                                        on_color,
                                        off_color,
                                        idle_color,
                                        frame_t,
                                        cumulative_ratio,
                                        lambda_maximum,
                                        lambda_maximum_auto);
                                    output_frame.paste_delta_ts(
                                        header.width,
                                        header.height,
                                        delta_ts,
                                        header.width,
                                        0,
                                        black,
                                        black_auto,
                                        white,
                                        white_auto,
                                        discard_ratio,
                                        atis_color);
                                    if (add_timecode) {
                                        output_frame.paste_timecode(font_left, font_top, font_size, frame_t);
                                    }
                                    output_frame.write(output_directory, digits, frame_index);
                                    ++frame_index;
                                    frame_t = first_t + frame_index * frametime;
                                }
                            },
                            sepia::make_split<sepia::type::atis>(
                                [&](sepia::dvs_event event) {
                                    const auto index = event.x + event.y * header.width;
                                    switch (decay_style) {
                                        case style::cumulative:
                                            if (event.is_increase) {
                                                style_state.on_ts_and_activities[index].second =
                                                    style_state.on_ts_and_activities[index].second
                                                        * std::exp(
                                                            -static_cast<float>(
                                                                event.t - style_state.on_ts_and_activities[index].first)
                                                            / static_cast<float>(tau))
                                                    + 1.0;
                                                style_state.on_ts_and_activities[index].first = event.t;
                                            } else {
                                                style_state.off_ts_and_activities[index].second =
                                                    style_state.off_ts_and_activities[index].second
                                                        * std::exp(
                                                            -static_cast<float>(
                                                                event.t
                                                                - style_state.off_ts_and_activities[index].first)
                                                            / static_cast<float>(tau))
                                                    + 1.0;
                                                style_state.off_ts_and_activities[index].first = event.t;
                                            }
                                            break;
                                        case style::cumulative_shared:
                                            std::get<1>(style_state.ts_and_activities_and_ons[index]) =
                                                std::get<1>(style_state.ts_and_activities_and_ons[index])
                                                    * std::exp(
                                                        -static_cast<float>(
                                                            event.t
                                                            - std::get<0>(style_state.ts_and_activities_and_ons[index]))
                                                        / static_cast<float>(tau))
                                                + 1.0;
                                            std::get<0>(style_state.ts_and_activities_and_ons[index]) = event.t;
                                            std::get<2>(style_state.ts_and_activities_and_ons[index]) =
                                                event.is_increase;
                                            break;
                                        default:
                                            style_state.ts_and_ons[index].first = event.t;
                                            style_state.ts_and_ons[index].second = event.is_increase;
                                            break;
                                    }
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
