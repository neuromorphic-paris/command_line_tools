#define _USE_MATH_DEFINES
#include "../third_party/pontella/source/pontella.hpp"
#include "../third_party/sepia/source/sepia.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb_truetype.hpp"
#include "../third_party/tarsier/source/replicate.hpp"
#include "../third_party/tarsier/source/stitch.hpp"
#include "font.hpp"
#include "timecode.hpp"
#include <complex>
#include <iomanip>
#include <sstream>
#include <tuple>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

constexpr uint16_t font_left = 20;
constexpr uint16_t font_top = 20;
constexpr uint16_t font_size = 30;

struct color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    color(uint8_t default_r, uint8_t default_g, uint8_t default_b) : r(default_r), g(default_g), b(default_b) {}
};

enum class mode {
    on,
    off,
    all,
    abs,
};

std::array<color, 256> magma_colors = {
    {{0, 0, 4},       {1, 0, 5},       {1, 1, 6},       {1, 1, 8},       {2, 1, 9},       {2, 2, 11},
     {2, 2, 13},      {3, 3, 15},      {3, 3, 18},      {4, 4, 20},      {5, 4, 22},      {6, 5, 24},
     {6, 5, 26},      {7, 6, 28},      {8, 7, 30},      {9, 7, 32},      {10, 8, 34},     {11, 9, 36},
     {12, 9, 38},     {13, 10, 41},    {14, 11, 43},    {16, 11, 45},    {17, 12, 47},    {18, 13, 49},
     {19, 13, 52},    {20, 14, 54},    {21, 14, 56},    {22, 15, 59},    {24, 15, 61},    {25, 16, 63},
     {26, 16, 66},    {28, 16, 68},    {29, 17, 71},    {30, 17, 73},    {32, 17, 75},    {33, 17, 78},
     {34, 17, 80},    {36, 18, 83},    {37, 18, 85},    {39, 18, 88},    {41, 17, 90},    {42, 17, 92},
     {44, 17, 95},    {45, 17, 97},    {47, 17, 99},    {49, 17, 101},   {51, 16, 103},   {52, 16, 105},
     {54, 16, 107},   {56, 16, 108},   {57, 15, 110},   {59, 15, 112},   {61, 15, 113},   {63, 15, 114},
     {64, 15, 116},   {66, 15, 117},   {68, 15, 118},   {69, 16, 119},   {71, 16, 120},   {73, 16, 120},
     {74, 16, 121},   {76, 17, 122},   {78, 17, 123},   {79, 18, 123},   {81, 18, 124},   {82, 19, 124},
     {84, 19, 125},   {86, 20, 125},   {87, 21, 126},   {89, 21, 126},   {90, 22, 126},   {92, 22, 127},
     {93, 23, 127},   {95, 24, 127},   {96, 24, 128},   {98, 25, 128},   {100, 26, 128},  {101, 26, 128},
     {103, 27, 128},  {104, 28, 129},  {106, 28, 129},  {107, 29, 129},  {109, 29, 129},  {110, 30, 129},
     {112, 31, 129},  {114, 31, 129},  {115, 32, 129},  {117, 33, 129},  {118, 33, 129},  {120, 34, 129},
     {121, 34, 130},  {123, 35, 130},  {124, 35, 130},  {126, 36, 130},  {128, 37, 130},  {129, 37, 129},
     {131, 38, 129},  {132, 38, 129},  {134, 39, 129},  {136, 39, 129},  {137, 40, 129},  {139, 41, 129},
     {140, 41, 129},  {142, 42, 129},  {144, 42, 129},  {145, 43, 129},  {147, 43, 128},  {148, 44, 128},
     {150, 44, 128},  {152, 45, 128},  {153, 45, 128},  {155, 46, 127},  {156, 46, 127},  {158, 47, 127},
     {160, 47, 127},  {161, 48, 126},  {163, 48, 126},  {165, 49, 126},  {166, 49, 125},  {168, 50, 125},
     {170, 51, 125},  {171, 51, 124},  {173, 52, 124},  {174, 52, 123},  {176, 53, 123},  {178, 53, 123},
     {179, 54, 122},  {181, 54, 122},  {183, 55, 121},  {184, 55, 121},  {186, 56, 120},  {188, 57, 120},
     {189, 57, 119},  {191, 58, 119},  {192, 58, 118},  {194, 59, 117},  {196, 60, 117},  {197, 60, 116},
     {199, 61, 115},  {200, 62, 115},  {202, 62, 114},  {204, 63, 113},  {205, 64, 113},  {207, 64, 112},
     {208, 65, 111},  {210, 66, 111},  {211, 67, 110},  {213, 68, 109},  {214, 69, 108},  {216, 69, 108},
     {217, 70, 107},  {219, 71, 106},  {220, 72, 105},  {222, 73, 104},  {223, 74, 104},  {224, 76, 103},
     {226, 77, 102},  {227, 78, 101},  {228, 79, 100},  {229, 80, 100},  {231, 82, 99},   {232, 83, 98},
     {233, 84, 98},   {234, 86, 97},   {235, 87, 96},   {236, 88, 96},   {237, 90, 95},   {238, 91, 94},
     {239, 93, 94},   {240, 95, 94},   {241, 96, 93},   {242, 98, 93},   {242, 100, 92},  {243, 101, 92},
     {244, 103, 92},  {244, 105, 92},  {245, 107, 92},  {246, 108, 92},  {246, 110, 92},  {247, 112, 92},
     {247, 114, 92},  {248, 116, 92},  {248, 118, 92},  {249, 120, 93},  {249, 121, 93},  {249, 123, 93},
     {250, 125, 94},  {250, 127, 94},  {250, 129, 95},  {251, 131, 95},  {251, 133, 96},  {251, 135, 97},
     {252, 137, 97},  {252, 138, 98},  {252, 140, 99},  {252, 142, 100}, {252, 144, 101}, {253, 146, 102},
     {253, 148, 103}, {253, 150, 104}, {253, 152, 105}, {253, 154, 106}, {253, 155, 107}, {254, 157, 108},
     {254, 159, 109}, {254, 161, 110}, {254, 163, 111}, {254, 165, 113}, {254, 167, 114}, {254, 169, 115},
     {254, 170, 116}, {254, 172, 118}, {254, 174, 119}, {254, 176, 120}, {254, 178, 122}, {254, 180, 123},
     {254, 182, 124}, {254, 183, 126}, {254, 185, 127}, {254, 187, 129}, {254, 189, 130}, {254, 191, 132},
     {254, 193, 133}, {254, 194, 135}, {254, 196, 136}, {254, 198, 138}, {254, 200, 140}, {254, 202, 141},
     {254, 204, 143}, {254, 205, 144}, {254, 207, 146}, {254, 209, 148}, {254, 211, 149}, {254, 213, 151},
     {254, 215, 153}, {254, 216, 154}, {253, 218, 156}, {253, 220, 158}, {253, 222, 160}, {253, 224, 161},
     {253, 226, 163}, {253, 227, 165}, {253, 229, 167}, {253, 231, 169}, {253, 233, 170}, {253, 235, 172},
     {252, 236, 174}, {252, 238, 176}, {252, 240, 178}, {252, 242, 180}, {252, 244, 182}, {252, 246, 184},
     {252, 247, 185}, {252, 249, 187}, {252, 251, 189}, {252, 253, 191}}};

struct state {
    int64_t activity;
    uint64_t current_t;
    uint64_t previous_t;
    std::vector<std::complex<double>> amplitudes;
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
        uint64_t frame_t,
        const std::vector<state>& states,
        double tau,
        double frequency_gamma,
        double amplitude_gamma,
        double discard,
        std::size_t frequencies_count) {
        std::vector<std::pair<double, std::size_t>> amplitudes_and_frequencies_indices(states.size(), {0.0, 0});
        std::vector<double> amplitudes(states.size(), 0.0);
        for (std::size_t index = 0; index < states.size(); ++index) {
            const auto& state = states[index];
            amplitudes_and_frequencies_indices[index].first = 0.0;
            for (std::size_t frequency_index = 0; frequency_index < frequencies_count; ++frequency_index) {
                const auto amplitude = std::abs(
                    state.amplitudes[frequency_index]
                    * std::exp(-static_cast<double>(frame_t - state.previous_t) / tau));
                if (amplitude > amplitudes_and_frequencies_indices[index].first) {
                    amplitudes_and_frequencies_indices[index].first = amplitude;
                    amplitudes_and_frequencies_indices[index].second = frequency_index;
                }
            }
            amplitudes[index] = amplitudes_and_frequencies_indices[index].first;
        }
        std::sort(amplitudes.begin(), amplitudes.end());
        const auto maximum_amplitude =
            amplitudes[static_cast<std::size_t>(std::floor(amplitudes.size() * (1.0 - discard)))];
        for (uint16_t y = 0; y < _height; ++y) {
            for (uint16_t x = 0; x < _width; ++x) {
                const auto amplitude_and_frequency_index = amplitudes_and_frequencies_indices[x + y * _width];
                const auto theta = std::pow(
                                       static_cast<double>(amplitude_and_frequency_index.second)
                                           / static_cast<double>(frequencies_count - 1),
                                       frequency_gamma)
                                   * static_cast<double>(magma_colors.size());
                const auto theta_integer = static_cast<uint16_t>(std::floor(theta));
                auto frequency_color = magma_colors[0];
                if (theta_integer >= magma_colors.size() - 1) {
                    frequency_color = magma_colors[magma_colors.size() - 1];
                } else {
                    const auto ratio = theta - theta_integer;
                    frequency_color = color{
                        static_cast<uint8_t>(
                            magma_colors[theta_integer + 1].r * ratio + magma_colors[theta_integer].r * (1.0f - ratio)),
                        static_cast<uint8_t>(
                            magma_colors[theta_integer + 1].g * ratio + magma_colors[theta_integer].g * (1.0f - ratio)),
                        static_cast<uint8_t>(
                            magma_colors[theta_integer + 1].b * ratio + magma_colors[theta_integer].b * (1.0f - ratio)),
                    };
                }
                const auto alpha =
                    std::pow(std::min(amplitude_and_frequency_index.first / maximum_amplitude, 1.0), amplitude_gamma);
                const auto selected_color = color{
                    static_cast<uint8_t>(frequency_color.r * alpha + magma_colors[0].r * (1.0f - alpha)),
                    static_cast<uint8_t>(frequency_color.g * alpha + magma_colors[0].g * (1.0f - alpha)),
                    static_cast<uint8_t>(frequency_color.b * alpha + magma_colors[0].b * (1.0f - alpha)),
                };
                for (uint16_t y_scale = 0; y_scale < _scale; ++y_scale) {
                    for (uint16_t x_scale = 0; x_scale < _scale; ++x_scale) {
                        const auto index =
                            (x * _scale + x_scale + (_height - _scale - y * _scale + y_scale) * _width) * 3;
                        _bytes[index] = selected_color.r;
                        _bytes[index + 1] = selected_color.g;
                        _bytes[index + 2] = selected_color.b;
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
        {"spatiospectrogram generates video frames where color indicates frequency",
         "    Frames use the P6 Netpbm format (https://en.wikipedia.org/wiki/Netpbm) if the output is a directory",
         "    Otherwise, the output consists of raw rgb24 frames",
         "Syntax: ./es_to_frames [options]",
         "Available options:",
         "    -i [path], --input [path]                sets the path to the input .es file",
         "                                                 defaults to standard input",
         "    -o [path], --output [path]               sets the path to the output directory",
         "                                                 defaults to standard output",
         "    -b [timecode], --begin [timecode]        ignores events before this timestamp (timecode)",
         "                                                 defaults to 00:00:00",
         "    -e [timecode], --end [timecode]          ignores events after this timestamp (timecode)",
         "                                                 defaults to the end of the recording",
         "    -f [timecode], --frametime [timecode]    sets the time between two frames (timecode)",
         "                                                 defaults to 00:00:00.020",
         "    -c [int], --scale [int]                  scale up the output by the given integer factor",
         "                                                 defaults to 1",
          "    -t [timecode], --tau [timecode]         decay (timecode)",
         "                                                 defaults to 00:00:00.100000",
         "    -m [mode], --mode [mode]                 polarity mode, one of \"on\", \"off\", \"all\", \"abs\"",
         "                                                 \"on\" only uses ON events",
         "                                                 \"off\" only uses OFF events",
         "                                                 \"all\" multiplies the complex activity by 1 for ON events,",
         "                                                 and -1 for OFF events",
         "                                                 \"abs\" multiplies the complex activity by 1 for all events",
         "                                                 defaults to \"all\"",
         "    -p [float], --minimum [float]            minimum frequency in Hertz",
         "                                                 defaults 10.0",
         "    -q [float], --maximum [float]            maximum frequency in Hertz",
         "                                                 defaults to 10000.0",
         "    -u [int], --frequencies [int]            number of frequencies",
         "                                                 defaults to 100",
         "    -g [float], --frequency-gamma [float]    gamma ramp (power) to apply to the output frequency",
         "                                                 defaults to 0.5",
         "    -k [float], --amplitude-gamma [float]    gamma ramp (power) to apply to the output amplitude",
         "                                                 defaults to 0.5",
         "    -r [float], --discard [float]            amplitude discard ratio for tone-mapping",
         "                                                 defaults to 0.001",
         "    -a, --add-timecode                       adds a timecode overlay",
         "    -d [int], --digits [int]                 sets the number of digits in output filenames",
         "                                                 ignored if the output is not a directory",
         "                                                 defaults to 6",
         "    -h, --help                               shows this help message"},
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
            {"tau", {"t"}},
            {"mode", {"m"}},
            {"minimum", {"p"}},
            {"maximum", {"q"}},
            {"frequencies", {"u"}},
            {"frequency-gamma", {"g"}},
            {"amplitude-gamma", {"k"}},
            {"discard", {"r"}},
            {"digits", {"d"}},
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
            const auto add_timecode = command.flags.find("add-timecode") != command.flags.end();
            uint64_t tau = 100000;
            {
                const auto name_and_argument = command.options.find("tau");
                if (name_and_argument != command.options.end()) {
                    tau = timecode(name_and_argument->second).value();
                    if (tau == 0) {
                        throw std::runtime_error("tau must be larger than 0");
                    }
                }
            }
            auto minimum_frequency = 10.0;
            {
                const auto name_and_argument = command.options.find("minimum");
                if (name_and_argument != command.options.end()) {
                    minimum_frequency = std::stod(name_and_argument->second);
                    if (minimum_frequency <= 0) {
                        throw std::runtime_error("minimum must be larger than 0");
                    }
                }
            }
            auto maximum_frequency = 10000.0;
            {
                const auto name_and_argument = command.options.find("maximum");
                if (name_and_argument != command.options.end()) {
                    maximum_frequency = std::stod(name_and_argument->second);
                    if (maximum_frequency <= 0) {
                        throw std::runtime_error("maximum must be larger than 0");
                    }
                    if (minimum_frequency >= maximum_frequency) {
                        throw std::runtime_error(
                            std::string("the minimum frequency (") + std::to_string(minimum_frequency)
                            + " Hz) must be smaller than the maximum frequency (" + std::to_string(maximum_frequency)
                            + " Hz)");
                    }
                }
            }
            std::size_t frequencies_count = 100;
            {
                const auto name_and_argument = command.options.find("frequencies");
                if (name_and_argument != command.options.end()) {
                    frequencies_count = static_cast<std::size_t>(std::stoull(name_and_argument->second));
                    if (frequencies_count == 0) {
                        throw std::runtime_error("frequencies must be larger than 0");
                    }
                }
            }
            auto frequency_gamma = 0.5;
            {
                const auto name_and_argument = command.options.find("frequency-gamma");
                if (name_and_argument != command.options.end()) {
                    frequency_gamma = std::stod(name_and_argument->second);
                    if (frequency_gamma <= 0) {
                        throw std::runtime_error("frequency-gamma must be larger than 0");
                    }
                }
            }
            auto amplitude_gamma = 0.5;
            {
                const auto name_and_argument = command.options.find("amplitude-gamma");
                if (name_and_argument != command.options.end()) {
                    amplitude_gamma = std::stod(name_and_argument->second);
                    if (amplitude_gamma <= 0) {
                        throw std::runtime_error("amplitude-gamma must be larger than 0");
                    }
                }
            }
            auto discard = 0.001;
            {
                const auto name_and_argument = command.options.find("discard");
                if (name_and_argument != command.options.end()) {
                    discard = std::stod(name_and_argument->second);
                    if (discard < 0.0 || discard >= 1.0) {
                        throw std::runtime_error("discard must in the range [0, 1[");
                    }
                }
            }
            auto polarity_mode = mode::all;
            {
                const auto name_and_argument = command.options.find("mode");
                if (name_and_argument != command.options.end()) {
                    if (name_and_argument->second == "on") {
                        polarity_mode = mode::on;
                    } else if (name_and_argument->second == "off") {
                        polarity_mode = mode::off;
                    } else if (name_and_argument->second == "all") {
                        polarity_mode = mode::all;
                    } else if (name_and_argument->second == "abs") {
                        polarity_mode = mode::abs;
                    } else {
                        throw std::runtime_error(std::string("unknown mode \"") + name_and_argument->second + "\"");
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
            if (header.event_stream_type != sepia::type::dvs) {
                throw std::runtime_error("unsupported event stream type");
            }
            std::vector<double> frequencies(frequencies_count, 0.0);
            for (std::size_t y = 0; y < frequencies_count; ++y) {
                frequencies[y] = minimum_frequency
                                 * std::pow(
                                     maximum_frequency / minimum_frequency,
                                     static_cast<double>(y) / static_cast<double>(frequencies_count - 1));
            }
            std::vector<state> states(
                header.width * header.height,
                state{0, 0, 0, std::vector<std::complex<double>>(frequencies_count, std::complex<double>{0.0, 0.0})});
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
                const auto index = event.x + event.y * header.width;
                auto& pixel_state = states[index];
                if (event.t > pixel_state.current_t) {
                    for (std::size_t y = 0; y < frequencies_count; ++y) {
                        const auto rotated =
                            static_cast<double>(pixel_state.activity)
                            * std::exp(
                                -std::complex<double>(0.0, 1.0)
                                * (2.0 * M_PI * frequencies[y] * (static_cast<double>(pixel_state.current_t) / 1e6)));
                        pixel_state.amplitudes[y] =
                            pixel_state.amplitudes[y]
                                * std::exp(-static_cast<double>(pixel_state.current_t - pixel_state.previous_t) / tau)
                            + rotated;
                    }
                    pixel_state.previous_t = pixel_state.current_t;
                    pixel_state.current_t = event.t;
                    pixel_state.activity = 0;
                }
                if (event.is_increase) {
                    switch (polarity_mode) {
                        case mode::on:
                        case mode::all:
                        case mode::abs:
                            ++pixel_state.activity;
                            break;
                        case mode::off:
                            break;
                    }
                } else {
                    switch (polarity_mode) {
                        case mode::off:
                        case mode::abs:
                            ++pixel_state.activity;
                            break;
                        case mode::all:
                            --pixel_state.activity;
                            break;
                        case mode::on:
                            break;
                    }
                }
                auto frame_t = first_t + frame_index * frametime;
                while (event.t >= frame_t) {
                    output_frame.paste_state(
                        frame_t, states, tau, frequency_gamma, amplitude_gamma, discard, frequencies_count);
                    if (add_timecode) {
                        output_frame.paste_timecode(font_left, font_top, font_size, frame_t);
                    }
                    output_frame.write(output_directory, digits, frame_index);
                    ++frame_index;
                    frame_t = first_t + frame_index * frametime;
                }
            });
        });
}
