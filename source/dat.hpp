#pragma once

#include "../third_party/sepia/source/sepia.hpp"
#include <algorithm>

namespace dat {
    /// header bundles a .dat file header's information.
    struct header {
        uint8_t version;
        uint16_t width;
        uint16_t height;
    };

    /// read_header retrieves header information from a .dat file.
    inline header read_header(std::istream& stream) {
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
            return {0, 304, 240};
        }
        stream.ignore(2);
        header stream_header = {};
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
                    if (words[0] == "Version") {
                        stream_header.version = static_cast<uint8_t>(stoul(words[1]));
                    } else if (words[0] == "Width") {
                        stream_header.width = static_cast<uint16_t>(stoul(words[1]));
                    } else if (words[0] == "Height") {
                        stream_header.height = static_cast<uint16_t>(stoul(words[1]));
                    }
                } catch (const std::invalid_argument&) {
                } catch (const std::out_of_range&) {
                }
            }
        }
        if (stream_header.version >= 2 && stream_header.width > 0 && stream_header.height > 0) {
            return stream_header;
        }
        return {stream_header.version, 304, 240};
    }

    /// bytes_to_dvs_event converts raw bytes to a polarized event.
    /// The DVS event type is used for both td and aps .dat files.
    inline sepia::dvs_event bytes_to_dvs_event(std::array<uint8_t, 8> bytes, header stream_header) {
        if (stream_header.version < 2) {
            return {
                static_cast<uint64_t>(bytes[0]) | (static_cast<uint64_t>(bytes[1]) << 8)
                    | (static_cast<uint64_t>(bytes[2]) << 16) | (static_cast<uint64_t>(bytes[3]) << 24),
                static_cast<uint16_t>(static_cast<uint16_t>(bytes[4]) | (static_cast<uint16_t>(bytes[5] & 1) << 8)),
                static_cast<uint16_t>(
                    stream_header.height - 1
                    - (static_cast<uint16_t>(bytes[5] >> 1) | (static_cast<uint16_t>(bytes[6] & 1) << 7))),
                (bytes[6] & 0b10) == 0b10,
            };
        }
        return {
            static_cast<uint64_t>(bytes[0]) | (static_cast<uint64_t>(bytes[1]) << 8)
                | (static_cast<uint64_t>(bytes[2]) << 16) | (static_cast<uint64_t>(bytes[3]) << 24),
            static_cast<uint16_t>(static_cast<uint16_t>(bytes[4]) | (static_cast<uint16_t>(bytes[5] & 0b111111) << 8)),
            static_cast<uint16_t>(
                stream_header.height - 1
                - (static_cast<uint16_t>(bytes[5] >> 6) | (static_cast<uint16_t>(bytes[6]) << 2)
                   | (static_cast<uint16_t>(bytes[7] & 0b1111) << 10))),
            (bytes[7] & 0b10000) == 0b10000,
        };
    }

    /// td_observable dispatches DVS events from a td stream.
    /// The header must be read from the stream before calling this function.
    template <typename HandleEvent>
    inline void td_observable(std::istream& stream, header stream_header, HandleEvent handle_event) {
        uint64_t previous_t = 0;
        for (;;) {
            std::array<uint8_t, 8> bytes;
            stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
            if (stream.eof()) {
                break;
            }
            const auto dvs_event = bytes_to_dvs_event(bytes, stream_header);
            if (dvs_event.t >= previous_t && dvs_event.x < stream_header.width && dvs_event.y < stream_header.height) {
                handle_event(dvs_event);
                previous_t = dvs_event.t;
            }
        }
    }

    /// aps_observable dispatches ATIS events from an aps stream.
    /// The header must be read from the stream before calling this function.
    template <typename HandleEvent>
    inline void aps_observable(std::istream& stream, header stream_header, HandleEvent handle_event) {
        uint64_t previous_t = 0;
        for (;;) {
            std::array<uint8_t, 8> bytes;
            stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
            if (stream.eof()) {
                break;
            }
            const auto dvs_event = bytes_to_dvs_event(bytes, stream_header);
            if (dvs_event.t >= previous_t && dvs_event.x < stream_header.width && dvs_event.y < stream_header.height) {
                handle_event(sepia::atis_event{dvs_event.t, dvs_event.x, dvs_event.y, true, dvs_event.is_increase});
                previous_t = dvs_event.t;
            }
        }
    }

    /// td_aps_observable dispatches ATIS events from a td stream and an aps stream.
    /// The headers must be read from both streams before calling this function.
    template <typename HandleEvent>
    inline void td_aps_observable(
        std::istream& td_stream,
        std::istream& aps_stream,
        header stream_header,
        HandleEvent handle_event) {
        sepia::dvs_event td_event = {};
        sepia::dvs_event aps_event = {};
        uint64_t previous_t = 0;
        for (;;) {
            std::array<uint8_t, 8> bytes;
            td_stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
            if (td_stream.eof()) {
                break;
            } else {
                td_event = bytes_to_dvs_event(bytes, stream_header);
                if (td_event.x < stream_header.width && td_event.y < stream_header.height) {
                    break;
                }
            }
        }
        for (;;) {
            std::array<uint8_t, 8> bytes;
            aps_stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
            if (aps_stream.eof()) {
                break;
            } else {
                aps_event = bytes_to_dvs_event(bytes, stream_header);
                if (aps_event.x < stream_header.width && aps_event.y < stream_header.height) {
                    break;
                }
            }
        }
        while (!td_stream.eof() && !aps_stream.eof()) {
            if (td_event.t <= aps_event.t) {
                handle_event(sepia::atis_event{td_event.t, td_event.x, td_event.y, false, td_event.is_increase});
                previous_t = td_event.t;
                for (;;) {
                    std::array<uint8_t, 8> bytes;
                    td_stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                    if (td_stream.eof()) {
                        break;
                    }
                    td_event = bytes_to_dvs_event(bytes, stream_header);
                    if (td_event.t >= previous_t && td_event.x < stream_header.width
                        && td_event.y < stream_header.height) {
                        break;
                    }
                }
            } else {
                handle_event(sepia::atis_event{aps_event.t, aps_event.x, aps_event.y, true, aps_event.is_increase});
                previous_t = aps_event.t;
                for (;;) {
                    std::array<uint8_t, 8> bytes;
                    aps_stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                    if (aps_stream.eof()) {
                        break;
                    }
                    aps_event = bytes_to_dvs_event(bytes, stream_header);
                    if (aps_event.t >= previous_t && aps_event.x < stream_header.width
                        && aps_event.y < stream_header.height) {
                        break;
                    }
                }
            }
        }
        if (td_stream) {
            handle_event(sepia::atis_event{td_event.t, td_event.x, td_event.y, false, td_event.is_increase});
            for (;;) {
                std::array<uint8_t, 8> bytes;
                td_stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                if (td_stream.eof()) {
                    break;
                }
                td_event = bytes_to_dvs_event(bytes, stream_header);
                if (td_event.t >= previous_t && td_event.x < stream_header.width && td_event.y < stream_header.height) {
                    handle_event(sepia::atis_event{td_event.t, td_event.x, td_event.y, false, td_event.is_increase});
                    previous_t = td_event.t;
                }
            }
        } else {
            handle_event(sepia::atis_event{aps_event.t, aps_event.x, aps_event.y, false, aps_event.is_increase});
            for (;;) {
                std::array<uint8_t, 8> bytes;
                aps_stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                if (aps_stream.eof()) {
                    break;
                }
                aps_event = bytes_to_dvs_event(bytes, stream_header);
                if (aps_event.t >= previous_t && aps_event.x < stream_header.width
                    && aps_event.y < stream_header.height) {
                    handle_event(
                        sepia::atis_event{aps_event.t, aps_event.x, aps_event.y, false, aps_event.is_increase});
                    previous_t = aps_event.t;
                }
            }
        }
    }
}
