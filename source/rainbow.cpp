#include "../third_party/lodepng/lodepng.h"
#include "../third_party/pontella/source/pontella.hpp"
#include "../third_party/sepia/source/sepia.hpp"
#include "../third_party/tarsier/source/stitch.hpp"
#include "html.hpp"
#include "timecode.hpp"
#include <numeric>

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
};

std::array<color, 64> parula_colors = {
    {{53, 42, 135},   {54, 48, 147},   {54, 55, 160},   {53, 61, 173},   {50, 67, 186},   {44, 74, 199},
     {32, 83, 212},   {15, 92, 221},   {3, 99, 225},    {2, 104, 225},   {4, 109, 224},   {8, 113, 222},
     {13, 117, 220},  {16, 121, 218},  {18, 125, 216},  {20, 129, 214},  {20, 133, 212},  {19, 137, 211},
     {16, 142, 210},  {12, 147, 210},  {9, 152, 209},   {7, 156, 207},   {6, 160, 205},   {6, 164, 202},
     {6, 167, 198},   {7, 169, 194},   {10, 172, 190},  {15, 174, 185},  {21, 177, 180},  {29, 179, 175},
     {37, 181, 169},  {46, 183, 164},  {56, 185, 158},  {66, 187, 152},  {77, 188, 146},  {89, 189, 140},
     {101, 190, 134}, {113, 191, 128}, {124, 191, 123}, {135, 191, 119}, {146, 191, 115}, {156, 191, 111},
     {165, 190, 107}, {174, 190, 103}, {183, 189, 100}, {192, 188, 96},  {200, 188, 93},  {209, 187, 89},
     {217, 186, 86},  {225, 185, 82},  {233, 185, 78},  {241, 185, 74},  {248, 187, 68},  {253, 190, 61},
     {255, 195, 55},  {254, 200, 50},  {252, 206, 46},  {250, 211, 42},  {247, 216, 38},  {245, 222, 33},
     {245, 228, 29},  {245, 235, 24},  {246, 243, 19},  {249, 251, 14}}};

template <sepia::type event_stream_type>
std::pair<uint64_t, uint64_t> read_begin_t_and_end_t(std::unique_ptr<std::istream> stream) {
    std::pair<uint64_t, uint64_t> result = {std::numeric_limits<uint64_t>::max(), 1};
    sepia::join_observable<event_stream_type>(std::move(stream), [&](sepia::event<event_stream_type> event) {
        if (result.first == std::numeric_limits<uint64_t>::max()) {
            result.first = event.t;
        }
        result.second = event.t;
    });
    if (result.first == std::numeric_limits<uint64_t>::max()) {
        result.first = 0;
        result.second = 1;
    } else if (result.first == result.second) {
        ++result.second;
    }
    return result;
}

template <sepia::type event_stream_type>
std::vector<uint8_t> rainbow(
    const sepia::header& header,
    std::unique_ptr<std::istream> stream,
    color initial_color,
    float alpha,
    std::pair<uint64_t, uint64_t> begin_t_and_end_t) {
    const auto t_scale = parula_colors.size() / static_cast<float>(begin_t_and_end_t.second - begin_t_and_end_t.first);
    auto t_to_color = [=](uint64_t t) {
        const auto theta = (t - begin_t_and_end_t.first) * t_scale;
        const auto theta_integer = static_cast<uint16_t>(std::floor(theta));
        if (theta_integer >= parula_colors.size() - 1) {
            return parula_colors[parula_colors.size() - 1];
        }
        const auto ratio = theta - theta_integer;
        return color{
            static_cast<uint8_t>(
                parula_colors[theta_integer + 1].r * ratio + parula_colors[theta_integer].r * (1.0f - ratio)),
            static_cast<uint8_t>(
                parula_colors[theta_integer + 1].g * ratio + parula_colors[theta_integer].g * (1.0f - ratio)),
            static_cast<uint8_t>(
                parula_colors[theta_integer + 1].b * ratio + parula_colors[theta_integer].b * (1.0f - ratio)),
        };
    };
    std::vector<uint8_t> frame(header.width * header.height * 3, initial_color.r);
    for (std::size_t index = 0; index < header.width * header.height; ++index) {
        frame[index * 3 + 1] = initial_color.g;
        frame[index * 3 + 2] = initial_color.b;
    }
    sepia::join_observable<event_stream_type>(std::move(stream), [&](sepia::event<event_stream_type> event) {
        const auto color = t_to_color(event.t);
        const auto index = event.x + (header.height - 1 - event.y) * header.width;
        frame[3 * index + 0] = static_cast<uint8_t>((1.0f - alpha) * frame[3 * index + 0] + alpha * color.r);
        frame[3 * index + 1] = static_cast<uint8_t>((1.0f - alpha) * frame[3 * index + 1] + alpha * color.g);
        frame[3 * index + 2] = static_cast<uint8_t>((1.0f - alpha) * frame[3 * index + 2] + alpha * color.b);
    });
    return frame;
}

int main(int argc, char* argv[]) {
    return pontella::main(
        {"rainbow represents events by mapping time to colors",
         "Syntax: ./rainbow [options] /path/to/input.es /path/to/output.ppm",
         "Available options:",
         "    -a alpha, --alpha alpha        sets the transparency level for each event",
         "                                       must be in the range ]0, 1]",
         "                                       defaults to 0.1",
         "    -l color, --idlecolor color    sets the background color",
         "                                       color must be formatted as #hhhhhh,",
         "                                       where h is an hexadecimal digit",
         "                                       defaults to #191919",
         "    -h, --help                     shows this help message"},
        argc,
        argv,
        2,
        {
            {"alpha", {"a"}},
            {"idlecolor", {"l"}},
        },
        {},
        [](pontella::command command) {
            float alpha = 0.1;
            {
                const auto name_and_argument = command.options.find("alpha");
                if (name_and_argument != command.options.end()) {
                    alpha = std::stof(name_and_argument->second);
                    if (alpha <= 0.0 || alpha > 1.0) {
                        throw std::runtime_error("alpha must be in the range ]0, 1]");
                    }
                }
            }
            color idle_color(0x29, 0x29, 0x29);
            {
                const auto name_and_argument = command.options.find("idlecolor");
                if (name_and_argument != command.options.end()) {
                    idle_color = color(name_and_argument->second);
                }
            }
            std::pair<uint64_t, uint64_t> begin_t_and_end_t;
            const auto header = sepia::read_header(sepia::filename_to_ifstream(command.arguments[0]));
            switch (header.event_stream_type) {
                case sepia::type::generic: {
                    throw std::runtime_error("generic events are not compatible with this application");
                    break;
                }
                case sepia::type::dvs: {
                    begin_t_and_end_t =
                        read_begin_t_and_end_t<sepia::type::dvs>(sepia::filename_to_ifstream(command.arguments[0]));
                    break;
                }
                case sepia::type::atis: {
                    begin_t_and_end_t =
                        read_begin_t_and_end_t<sepia::type::atis>(sepia::filename_to_ifstream(command.arguments[0]));
                    break;
                }
                case sepia::type::color: {
                    begin_t_and_end_t =
                        read_begin_t_and_end_t<sepia::type::color>(sepia::filename_to_ifstream(command.arguments[0]));
                    break;
                }
            }
            std::vector<uint8_t> frame;
            switch (header.event_stream_type) {
                case sepia::type::generic: {
                    throw std::runtime_error("generic events are not compatible with this application");
                    break;
                }
                case sepia::type::dvs: {
                    frame = rainbow<sepia::type::dvs>(
                        header,
                        sepia::filename_to_ifstream(command.arguments[0]),
                        idle_color,
                        alpha,
                        begin_t_and_end_t);
                    break;
                }
                case sepia::type::atis: {
                    frame = rainbow<sepia::type::atis>(
                        header,
                        sepia::filename_to_ifstream(command.arguments[0]),
                        idle_color,
                        alpha,
                        begin_t_and_end_t);
                    break;
                }
                case sepia::type::color: {
                    frame = rainbow<sepia::type::color>(
                        header,
                        sepia::filename_to_ifstream(command.arguments[0]),
                        idle_color,
                        alpha,
                        begin_t_and_end_t);
                    break;
                }
            }
            auto output = sepia::filename_to_ofstream(command.arguments[1]);
            *output << "P6\n" << header.width << " " << header.height << "\n255\n";
            output->write(reinterpret_cast<const char*>(frame.data()), frame.size());
        });
}
