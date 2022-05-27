#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>

class timecode {
    public:
    timecode(uint64_t value) : _value(value) {}
    timecode(const std::string& representation) : _value(0) {
        if (std::all_of(representation.begin(), representation.end(), [](uint8_t character) {
                return std::isdigit(character);
            })) {
            _value = std::stoull(representation);
        } else {
            std::regex timecode_pattern("^(\\d+):(\\d+):(\\d+)(?:\\.(\\d+))?$");
            std::smatch match;
            if (std::regex_match(representation, match, timecode_pattern)) {
                _value =
                    (std::stoull(std::string(match[1].first, match[1].second)) * 3600000000ull
                     + std::stoull(std::string(match[2].first, match[2].second)) * 60000000ull
                     + std::stoull(std::string(match[3].first, match[3].second)) * 1000000ull);
                if (match[4].matched) {
                    const auto fraction_string = std::string(match[4].first, match[4].second);
                    if (fraction_string.size() == 6) {
                        _value += std::stoull(fraction_string);
                    } else if (fraction_string.size() < 6) {
                        _value += std::stoull(fraction_string + std::string(6 - fraction_string.size(), '0'));
                    } else {
                        _value =
                            static_cast<uint64_t>(std::llround(std::stod(std::string("0.") + fraction_string) * 1e6));
                    }
                }
            } else {
                throw std::runtime_error(std::string("parsing the timecode '") + representation + "' failed");
            }
        }
    }
    timecode(const timecode&) = default;
    timecode(timecode&&) = default;
    timecode& operator=(const timecode&) = default;
    timecode& operator=(timecode&&) = default;
    virtual ~timecode() {}

    /// to_string converts the timecode to a human-friendly string.
    virtual std::string to_string() const {
        const auto hours = _value / 3600000000ull;
        auto remainder = _value - hours * 3600000000ull;
        const auto minutes = remainder / 60000000ull;
        remainder -= minutes * 60000000ull;
        const auto seconds = remainder / 1000000ull;
        remainder -= seconds * 1000000ull;
        std::stringstream stream;
        stream.fill('0');
        stream << _value << " µs (" << std::setw(2) << hours << ':' << std::setw(2) << minutes << ':' << std::setw(2)
               << seconds << '.' << std::setw(6) << remainder << ")";
        return stream.str();
    }

    /// to_timecode_string converts the timecode to a human-friendly string without microseconds.
    virtual std::string to_timecode_string() const {
        const auto hours = _value / 3600000000ull;
        auto remainder = _value - hours * 3600000000ull;
        const auto minutes = remainder / 60000000ull;
        remainder -= minutes * 60000000ull;
        const auto seconds = remainder / 1000000ull;
        remainder -= seconds * 1000000ull;
        std::stringstream stream;
        stream.fill('0');
        stream << std::setw(2) << hours << ':' << std::setw(2) << minutes << ':' << std::setw(2) << seconds << '.'
               << std::setw(6) << remainder;
        return stream.str();
    }

    /// value returns a number of microseconds.
    virtual uint64_t value() const {
        return _value;
    }

    protected:
    uint64_t _value;
};
