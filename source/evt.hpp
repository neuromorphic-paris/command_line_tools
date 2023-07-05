#pragma once

#include "../third_party/sepia/source/sepia.hpp"
#include <algorithm>
#include <iostream>
#include <limits>

namespace evt {
    /// header bundles a .dat file header's information.
    struct header {
        uint16_t width;
        uint16_t height;
    };

    /// read_header retrieves header information from a .dat file.
    inline header read_header(std::istream& stream, header default_header) {
        std::vector<std::string> header_lines;
        for (;;) {
            if (stream.peek() != '%' || stream.eof()) {
                break;
            }
            stream.ignore();
            header_lines.emplace_back();
            for (;;) {
                const auto character = stream.get();
                if (stream.eof()) {
                    break;
                }
                if (character == '\n') {
                    break;
                }
                header_lines.back().push_back(character);
            }
        }
        if (header_lines.empty()
            || std::any_of(header_lines.begin(), header_lines.end(), [](const std::string& header_line) {
                   return std::any_of(header_line.begin(), header_line.end(), [](char character) {
                       return !std::isprint(character) && !std::isspace(character);
                   });
               })) {
            stream.seekg(0, std::istream::beg);
            return {1280, 720};
        }
        for (const auto& header_line : header_lines) {
            std::vector<std::string> words;
            for (auto character : header_line) {
                if (std::isspace(character)) {
                    if (!words.empty() && !words.back().empty()) {
                        words.emplace_back();
                    }
                } else {
                    if (words.empty()) {
                        words.emplace_back();
                    }
                    words.back().push_back(character);
                }
            }
            if (words.size() > 1) {
                try {
                    if (words[0] == "Width") {
                        default_header.width = static_cast<uint16_t>(stoul(words[1]));
                    } else if (words[0] == "Height") {
                        default_header.height = static_cast<uint16_t>(stoul(words[1]));
                    } else if (words[0] == "geometry") {
                        const auto separator_position = words[1].find("x");
                        if (separator_position > 0 && separator_position != std::string::npos) {
                            default_header.width = static_cast<uint16_t>(stoul(words[1].substr(0, separator_position)));
                            default_header.height =
                                static_cast<uint16_t>(stoul(words[1].substr(separator_position + 1)));
                        }
                    }
                } catch (const std::invalid_argument&) {
                } catch (const std::out_of_range&) {
                }
            }
        }
        return default_header;
    }

    /// observable dispatches DVS events from a stream.
    /// The header must be read from the stream before calling this function.
    template <typename HandleEvent>
    inline void observable_2(std::istream& stream, header stream_header, bool normalize, HandleEvent handle_event) {
        uint64_t offset = 0;
        uint64_t t_without_offset = 0;
        uint64_t reference_t = 0;
        uint64_t first_t = normalize ? std::numeric_limits<uint64_t>::max() : 0;
        uint64_t previous_t = 0;
        std::vector<uint8_t> bytes(1 << 16);
        for (;;) {
            stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
            for (std::size_t index = 0; index < (stream.gcount() / 4) * 4; index += 4) {
                switch (bytes[index + 3] >> 4) {
                    case 0b0000:   // CD_OFF
                    case 0b0001: { // CD_ON
                        sepia::dvs_event event = {};
                        event.t = reference_t
                                  + ((static_cast<uint64_t>(bytes[index + 2]) >> 6)
                                     | (static_cast<uint64_t>(bytes[index + 3] & 0b1111) << 2));
                        if (first_t == std::numeric_limits<uint64_t>::max()) {
                            first_t = event.t;
                        }
                        event.t -= first_t;
                        if (event.t < previous_t) {
                            event.t = previous_t;
                        } else {
                            previous_t = event.t;
                        }
                        event.x = static_cast<uint16_t>(bytes[index + 1] >> 3)
                                  | (static_cast<uint16_t>(bytes[index + 2] & 0b111111) << 5);
                        event.y = static_cast<uint16_t>(bytes[index])
                                  | (static_cast<uint16_t>(bytes[index + 1] & 0b111) << 8);
                        event.is_increase = (bytes[index + 3] >> 4) == 0b0001;
                        if (event.x < stream_header.width && event.y < stream_header.height) {
                            event.y = stream_header.height - 1 - event.y;
                            handle_event(event);
                        } else {
                            std::cerr << "out of bounds event (t=" << event.t << ", x=" << event.x << ", y=" << event.y
                                      << ", on=" << (event.is_increase ? "true" : "false") << ")" << std::endl;
                        }
                        break;
                    }
                    case 0b1000: { // EVT_TIME_HIGH
                        const auto new_t_without_offset =
                            ((static_cast<uint32_t>(bytes[index]) | (static_cast<uint32_t>(bytes[index + 1]) << 8)
                              | (static_cast<uint32_t>(bytes[index + 2]) << 16)
                              | (static_cast<uint32_t>(bytes[index + 3] & 0b1111) << 24)))
                            << 6;
                        if (new_t_without_offset < t_without_offset) {
                            offset += (1ull << 34);
                        }
                        t_without_offset = new_t_without_offset;
                        reference_t = t_without_offset + offset;
                        break;
                    }
                    case 0b1010: // EXT_TRIGGER
                        break;
                    case 0b1110: // OTHERS
                        break;
                    case 0b1111: // CONTINUED
                        break;
                }
            }
            if (stream.eof()) {
                break;
            }
        }
    }

    /// observable dispatches DVS events from a stream.
    /// The header must be read from the stream before calling this function.
    template <typename HandleEvent>
    inline void observable_3(std::istream& stream, header stream_header, bool normalize, HandleEvent handle_event) {
        uint32_t previous_msb_t = 0;
        uint32_t previous_lsb_t = 0;
        uint32_t overflows = 0;
        uint64_t first_t = normalize ? std::numeric_limits<uint64_t>::max() : 0;
        sepia::dvs_event event = {};
        std::vector<uint8_t> bytes(1 << 16);
        for (;;) {
            stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
            for (std::size_t index = 0; index < (stream.gcount() / 2) * 2; index += 2) {
                switch (bytes[index + 1] >> 4) {
                    case 0b0000: // EVT_ADDR_Y
                        event.y = static_cast<uint16_t>(
                            stream_header.height - 1
                            - (bytes[index] | (static_cast<uint16_t>(bytes[index + 1] & 0b111) << 8)));
                        break;
                    case 0b0001:
                        break;
                    case 0b0010: // EVT_ADDR_X
                        event.x = static_cast<uint16_t>(
                            bytes[index] | (static_cast<uint16_t>(bytes[index + 1] & 0b111) << 8));
                        event.is_increase = ((bytes[index + 1] >> 3) & 1) == 1;
                        if (event.x < stream_header.width && event.y < stream_header.height) {
                            handle_event(event);
                        } else {
                            std::cerr << "out of bounds event (t=" << event.t << ", x=" << event.x << ", y=" << event.y
                                      << ", on=" << (event.is_increase ? "true" : "false") << ")" << std::endl;
                        }
                        break;
                    case 0b0011: // VECT_BASE_X
                        event.x = static_cast<uint16_t>(
                            bytes[index] | (static_cast<uint16_t>(bytes[index + 1] & 0b111) << 8));
                        event.is_increase = ((bytes[index + 1] >> 3) & 1) == 1;
                        break;
                    case 0b0100: // VECT_12
                        for (uint8_t bit = 0; bit < 8; ++bit) {
                            if (((bytes[index] >> bit) & 1) == 1) {
                                if (event.x < stream_header.width && event.y < stream_header.height) {
                                    handle_event(event);
                                }
                            }
                            ++event.x;
                        }
                        for (uint8_t bit = 0; bit < 4; ++bit) {
                            if (((bytes[index + 1] >> bit) & 1) == 1) {
                                if (event.x < stream_header.width && event.y < stream_header.height) {
                                    handle_event(event);
                                }
                            }
                            ++event.x;
                        }
                        break;
                    case 0b0101: // VECT_8
                        for (uint8_t bit = 0; bit < 8; ++bit) {
                            if (((bytes[index] >> bit) & 1) == 1) {
                                if (event.x < stream_header.width && event.y < stream_header.height) {
                                    handle_event(event);
                                }
                            }
                            ++event.x;
                        }
                        break;
                    case 0b0110: { // EVT_TIME_LOW
                        const auto lsb_t = static_cast<uint32_t>(
                            bytes[index] | (static_cast<uint32_t>(bytes[index + 1] & 0b1111) << 8));
                        if (lsb_t != previous_lsb_t) {
                            previous_lsb_t = lsb_t;
                            auto t = static_cast<uint64_t>(previous_lsb_t | (previous_msb_t << 12))
                                     + (static_cast<uint64_t>(overflows) << 24);
                            if (first_t == std::numeric_limits<uint64_t>::max()) {
                                first_t = t;
                            }
                            t -= first_t;
                            if (t >= event.t) {
                                event.t = t;
                            }
                        }
                        break;
                    }
                    case 0b0111:
                        break;
                    case 0b1000: { // EVT_TIME_HIGH
                        const auto msb_t = static_cast<uint32_t>(
                            bytes[index] | (static_cast<uint32_t>(bytes[index + 1] & 0b1111) << 8));
                        if (msb_t != previous_msb_t) {
                            if (msb_t > previous_msb_t) {
                                if (msb_t - previous_msb_t < static_cast<uint32_t>((1 << 12) - 2)) {
                                    previous_lsb_t = 0;
                                    previous_msb_t = msb_t;
                                }
                            } else {
                                if (previous_msb_t - msb_t > static_cast<uint32_t>((1 << 12) - 2)) {
                                    ++overflows;
                                    previous_lsb_t = 0;
                                    previous_msb_t = msb_t;
                                }
                            }
                            auto t = static_cast<uint64_t>(previous_lsb_t | (previous_msb_t << 12))
                                     + (static_cast<uint64_t>(overflows) << 24);
                            if (first_t == std::numeric_limits<uint64_t>::max()) {
                                first_t = t;
                            }
                            t -= first_t;
                            if (t >= event.t) {
                                event.t = t;
                            }
                        }
                    }
                    case 0b1001:
                        break;
                    case 0b1010: // EXT_TRIGGER
                        break;
                    case 0b1011:
                    case 0b1100:
                    case 0b1101:
                    case 0b1110:
                    case 0b1111:
                        break;
                    default:
                        break;
                }
            }
            if (stream.eof()) {
                break;
            }
        }
    }
}
