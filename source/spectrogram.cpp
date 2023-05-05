#define _USE_MATH_DEFINES
#include "../third_party/lodepng/lodepng.h"
#include "../third_party/pontella/source/pontella.hpp"
#include "../third_party/sepia/source/sepia.hpp"
#include "timecode.hpp"
#include <algorithm>
#include <complex>
#include <deque>

// plot parameters
constexpr uint16_t x_offset = 80;
constexpr uint16_t y_offset = 50;
constexpr uint16_t font_size = 20;
constexpr uint16_t timecode_size = 136;
constexpr uint16_t tick_size = 8;
constexpr float font_baseline_ratio = 0.3f;
constexpr float font_width_ratio = 0.5f;
constexpr float font_exponent_ratio = 0.8f;
constexpr float font_exponent_baseline_ratio = 0.5f;

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

enum class mode {
    on,
    off,
    all,
    abs,
};

struct spectrogram {
    std::vector<uint64_t> times;
    std::vector<double> frequencies;
    std::vector<std::complex<double>> amplitudes;
};

struct region_of_interest {
    uint16_t left;
    uint16_t right;
    uint16_t bottom;
    uint16_t top;

    template <sepia::type event_stream_type>
    bool contains(sepia::event<event_stream_type> event) {
        return event.x >= left && event.x < right && event.y >= bottom && event.y < top;
    }
};

/// compute_spectrogram calculates the spectrogram.
template <sepia::type event_stream_type>
spectrogram typed_compute_spectrogram(
    std::unique_ptr<std::istream> stream,
    uint64_t begin_t,
    uint64_t end_t,
    region_of_interest roi,
    std::size_t times,
    double tau,
    std::size_t frequencies,
    double minimum_frequency,
    double maximum_frequency,
    mode polarity_mode) {
    spectrogram result{
        std::vector<uint64_t>(times, 0),
        std::vector<double>(frequencies, 0.0),
        std::vector<std::complex<double>>(times * frequencies, std::complex<double>(0.0, 0.0)),
    };
    for (std::size_t x = 0; x < times; ++x) {
        result.times[x] =
            static_cast<uint64_t>(std::floor(
                static_cast<double>(end_t - 1 - begin_t) / static_cast<double>(times - 1) * static_cast<double>(x)))
            + begin_t;
    }
    for (std::size_t y = 0; y < frequencies; ++y) {
        result.frequencies[y] =
            minimum_frequency
            * std::pow(
                maximum_frequency / minimum_frequency, static_cast<double>(y) / static_cast<double>(frequencies - 1));
    }
    uint64_t previous_update_t = 0;
    uint64_t t = 0;
    int64_t activity = 0;
    std::vector<std::complex<double>> amplitudes(frequencies, std::complex<double>(0.0, 0.0));
    auto update = [&]() {
        for (std::size_t y = 0; y < frequencies; ++y) {
            const auto rotated = static_cast<double>(activity)
                                 * std::exp(
                                     -std::complex<double>(0.0, 1.0)
                                     * (2.0 * M_PI * result.frequencies[y] * (static_cast<double>(t) / 1e6)));
            amplitudes[y] = amplitudes[y] * std::exp(-static_cast<double>(t - previous_update_t) / tau) + rotated;
        }
        previous_update_t = t;
    };
    std::size_t time_index = 0;
    sepia::join_observable<event_stream_type>(std::move(stream), [&](sepia::event<event_stream_type> event) {
        if (event.t < begin_t) {
            return;
        }
        if (event.t >= end_t) {
            throw sepia::end_of_file();
        }
        if (!roi.contains(event)) {
            return;
        }
        if (event.t > t) {
            update();
            t = event.t;
            activity = 0;
        }
        if (event.is_increase) {
            switch (polarity_mode) {
                case mode::on:
                case mode::all:
                case mode::abs:
                    ++activity;
                    break;
                case mode::off:
                    break;
            }
        } else {
            switch (polarity_mode) {
                case mode::off:
                case mode::abs:
                    ++activity;
                    break;
                case mode::all:
                    --activity;
                    break;
                case mode::on:
                    break;
            }
        }
        while (time_index < result.times.size() && event.t > result.times[time_index]) {
            for (std::size_t y = 0; y < frequencies; ++y) {
                result.amplitudes[time_index + y * times] =
                    amplitudes[y] * std::exp(-static_cast<double>(result.times[time_index] - previous_update_t) / tau);
            }
            ++time_index;
        }
    });
    for (; time_index < result.times.size(); ++time_index) {
        for (std::size_t y = 0; y < frequencies; ++y) {
            result.amplitudes[time_index + y * times] =
                amplitudes[y] * std::exp(-static_cast<double>(result.times[time_index] - previous_update_t) / tau);
        }
        ++time_index;
    }
    return result;
}

spectrogram compute_spectrogram(
    const sepia::header& header,
    std::unique_ptr<std::istream> stream,
    uint64_t begin_t,
    uint64_t end_t,
    region_of_interest roi,
    std::size_t times,
    double tau,
    std::size_t frequencies,
    double minimum_frequency,
    double maximum_frequency,
    mode polarity_mode) {
    switch (header.event_stream_type) {
        case sepia::type::generic: {
            throw std::runtime_error("unsupported event type \"generic\"");
        }
        case sepia::type::dvs: {
            return typed_compute_spectrogram<sepia::type::dvs>(
                std::move(stream),
                begin_t,
                end_t,
                roi,
                times,
                tau,
                frequencies,
                minimum_frequency,
                maximum_frequency,
                polarity_mode);
        }
        case sepia::type::atis: {
            throw std::runtime_error("unsupported event type \"atis\"");
        }
        case sepia::type::color: {
            throw std::runtime_error("unsupported event type \"color\"");
        }
    }
}

struct color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    color(uint8_t default_r, uint8_t default_g, uint8_t default_b) : r(default_r), g(default_g), b(default_b) {}
    uint32_t to_u32() const {
        return (
            static_cast<uint32_t>(r) | static_cast<uint32_t>(g << 8) | static_cast<uint32_t>(b << 16)
            | static_cast<uint32_t>(static_cast<uint32_t>(255) << 24));
    }
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

int main(int argc, char* argv[]) {
    return pontella::main(
        {"spectrogram plots a short-time Fourier transform.",
         "Syntax: ./spectrogram [options] /path/to/input.es /path/to/output.png /path/to/output.json",
         "Available options:",
         "    -b [timecode], --begin [timecode]    ignores events before this timestamp (timecode)",
         "                                             defaults to 00:00:00",
         "    -e [timecode], --end [timecode]      ignores events after this timestamp (timecode)",
         "                                             defaults to the end of the recording",
         "    -l [int], --left [int]               input region of interest in pixels",
         "                                              defaults to 0",
         "    -o [int], --bottom [int]             input region of interest in pixels",
         "                                              defaults to 0",
         "    -c [int], --width [int]              input region of interest in pixels",
         "                                              defaults to input width",
         "    -d [int], --height [int]             input region of interest in pixels",
         "                                              defaults to input height",
          "    -t [timecode], --tau [timecode]     decay (timecode)",
         "                                             defaults to 00:00:00.100000",
         "    -m [mode], --mode [mode]             polarity mode, one of \"on\", \"off\", \"all\", \"abs\"",
         "                                             \"on\" only uses ON events",
         "                                             \"off\" only uses OFF events",
         "                                             \"all\" multiplies the complex activity by 1 for ON events,",
         "                                             and -1 for OFF events",
         "                                             \"abs\" multiplies the complex activity by 1 for all events",
         "                                             defaults to \"all\"",
         "    -i [float], --minimum [float]        minimum frequency in Hertz",
         "                                             defaults to 5e6 / (end - begin)",
         "    -j [float], --maximum [float]        maximum frequency in Hertz",
         "                                             defaults to 10000.0",
         "    -f [int], --frequencies [int]        number of frequencies",
         "                                             defaults to 100",
         "    -s [int], --times [int]              number of time samples",
         "                                             defaults to 1000",
         "    -g [float], --gamma [float]          gamma ramp (power) to apply to the output image",
         "                                             defaults to 0.5",
         "    -h, --help                           shows this help message"},

        argc,
        argv,
        3,
        {
            {"begin", {"b"}},
            {"end", {"e"}},
            {"left", {"l"}},
            {"bottom", {"o"}},
            {"width", {"c"}},
            {"height", {"d"}},
            {"tau", {"t"}},
            {"mode", {"m"}},
            {"minimum", {"i"}},
            {"maximum", {"j"}},
            {"frequencies", {"f"}},
            {"times", {"s"}},
            {"gamma", {"g"}},
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
            auto maximum_frequency = 10000.0;
            {
                const auto name_and_argument = command.options.find("maximum");
                if (name_and_argument != command.options.end()) {
                    maximum_frequency = std::stod(name_and_argument->second);
                    if (maximum_frequency <= 0) {
                        throw std::runtime_error("maximum must be larger than 0");
                    }
                }
            }
            std::size_t frequencies = 100;
            {
                const auto name_and_argument = command.options.find("frequencies");
                if (name_and_argument != command.options.end()) {
                    frequencies = std::stoull(name_and_argument->second);
                    if (frequencies == 0) {
                        throw std::runtime_error("frequencies must be larger than 0");
                    }
                }
            }
            std::size_t times = 1000;
            {
                const auto name_and_argument = command.options.find("times");
                if (name_and_argument != command.options.end()) {
                    times = std::stoull(name_and_argument->second);
                    if (times == 0) {
                        throw std::runtime_error("times must be larger than 0");
                    }
                }
            }
            auto gamma = 0.5;
            {
                const auto name_and_argument = command.options.find("gamma");
                if (name_and_argument != command.options.end()) {
                    gamma = std::stod(name_and_argument->second);
                    if (gamma <= 0) {
                        throw std::runtime_error("gamma must be larger than 0");
                    }
                }
            }
            const auto header = sepia::read_header(sepia::filename_to_ifstream(command.arguments[0]));
            region_of_interest roi{0, header.width, 0, header.height};
            {
                const auto name_and_argument = command.options.find("left");
                if (name_and_argument != command.options.end()) {
                    roi.left = static_cast<uint16_t>(std::stoull(name_and_argument->second));
                }
            }
            {
                const auto name_and_argument = command.options.find("bottom");
                if (name_and_argument != command.options.end()) {
                    roi.bottom = static_cast<uint16_t>(std::stoull(name_and_argument->second));
                }
            }
            {
                const auto name_and_argument = command.options.find("width");
                if (name_and_argument != command.options.end()) {
                    roi.right = roi.left + static_cast<uint16_t>(std::stoull(name_and_argument->second));
                }
            }
            {
                const auto name_and_argument = command.options.find("height");
                if (name_and_argument != command.options.end()) {
                    roi.top = roi.bottom + static_cast<uint16_t>(std::stoull(name_and_argument->second));
                }
            }
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
                throw std::runtime_error("begin must be smaller than the end of the recording");
            }
            auto minimum_frequency = 5e6 / static_cast<double>(first_and_last_t.second - first_and_last_t.first);
            {
                const auto name_and_argument = command.options.find("minimum");
                if (name_and_argument != command.options.end()) {
                    minimum_frequency = std::stod(name_and_argument->second);
                    if (minimum_frequency <= 0) {
                        throw std::runtime_error("minimum must be larger than 0");
                    }
                }
            }
            if (minimum_frequency >= maximum_frequency) {
                throw std::runtime_error(
                    std::string("the minimum frequency (") + std::to_string(minimum_frequency)
                    + " Hz) must be smaller than the maximum frequency (" + std::to_string(maximum_frequency) + " Hz)");
            }
            const auto complex_spectrogram = compute_spectrogram(
                header,
                sepia::filename_to_ifstream(command.arguments[0]),
                first_and_last_t.first,
                first_and_last_t.second,
                roi,
                times,
                tau,
                frequencies,
                minimum_frequency,
                maximum_frequency,
                polarity_mode);
            std::vector<double> real_amplitudes(complex_spectrogram.amplitudes.size());
            for (uint16_t y = 0; y < frequencies; ++y) {
                std::transform(
                    std::next(complex_spectrogram.amplitudes.begin(), times * y),
                    std::next(complex_spectrogram.amplitudes.begin(), times * (y + 1)),
                    std::next(real_amplitudes.begin(), times * (frequencies - 1 - y)),
                    [](std::complex<double> amplitude) { return std::abs(amplitude); });
            }
            const auto minmax = std::minmax_element(real_amplitudes.begin(), real_amplitudes.end());
            std::vector<uint8_t> frame(frequencies * times * 4, 0);
            std::transform(
                real_amplitudes.begin(),
                real_amplitudes.end(),
                reinterpret_cast<uint32_t*>(frame.data()),
                [=](double amplitude) {
                    const auto theta =
                        std::pow((amplitude - *(minmax.first)) / (*(minmax.second) - *(minmax.first)), gamma)
                        * static_cast<double>(magma_colors.size());
                    const auto theta_integer = static_cast<uint16_t>(std::floor(theta));
                    if (theta_integer >= magma_colors.size() - 1) {
                        return magma_colors[magma_colors.size() - 1].to_u32();
                    }
                    const auto ratio = theta - theta_integer;
                    return (color{
                                static_cast<uint8_t>(
                                    magma_colors[theta_integer + 1].r * ratio
                                    + magma_colors[theta_integer].r * (1.0f - ratio)),
                                static_cast<uint8_t>(
                                    magma_colors[theta_integer + 1].g * ratio
                                    + magma_colors[theta_integer].g * (1.0f - ratio)),
                                static_cast<uint8_t>(
                                    magma_colors[theta_integer + 1].b * ratio
                                    + magma_colors[theta_integer].b * (1.0f - ratio)),
                            })
                        .to_u32();
                });
            std::vector<uint8_t> png_bytes;
            if (lodepng::encode(png_bytes, frame, times, frequencies) != 0) {
                throw std::logic_error("encoding the base frame failed");
            }
            sepia::filename_to_ofstream(command.arguments[1])
                ->write(reinterpret_cast<const char*>(png_bytes.data()), png_bytes.size());
            auto json_output = sepia::filename_to_ofstream(command.arguments[2]);
            (*json_output) << "{\n    \"frequencies\": [\n";
            for (std::size_t index = 0; index < complex_spectrogram.frequencies.size(); ++index) {
                (*json_output) << "        " << complex_spectrogram.frequencies[index];
                if (index < complex_spectrogram.frequencies.size() - 1) {
                    (*json_output) << ",\n";
                }
            }
            (*json_output) << "\n    ],\n    \"times\": [\n";
            for (std::size_t index = 0; index < complex_spectrogram.times.size(); ++index) {
                (*json_output) << "        " << complex_spectrogram.times[index];
                if (index < complex_spectrogram.times.size() - 1) {
                    (*json_output) << ",\n";
                }
            }
            (*json_output) << "\n    ]\n}\n";
        });
}
