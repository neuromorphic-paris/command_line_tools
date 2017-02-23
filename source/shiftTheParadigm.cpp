#include "lodepng.hpp"

#include <pontella.hpp>
#include <sepia.hpp>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>

/// Rgb represents a color in the sRGB space.
/// r is in the range [0, 1]
/// g is in the range [0, 1]
/// b is in the range [0, 1]
struct Rgb {
    double r;
    double g;
    double b;
};

/// Lab represents a color in L*a*b* space.
/// l is in the range [0, 100]
/// a is in the range [-86.18270151612145, 98.2343228541257]
/// b is in the range [-107.86016452983817, 94.47796855141397]
struct Lab {
    double l;
    double a;
    double b;
};

/// labTransform is a utility function used by the sRGB to L*a*b* converter.
double labTransform(double component) {
    return (component > std::pow(6.0 / 29.0, 3.0) ?
        std::pow(component, 1.0 / 3.0)
        : 1.0 / 3.0 * std::pow(29.0 / 6.0, 2.0) * component + 4.0 / 29.0
    );
}

/// white components used to compute L*a*b* from XYZ.
const auto whiteX = 95.047;
const auto whiteY = 100.00001;
const auto whiteZ = 108.883;

/// rgbToLab turns a color in sRGB space to a color in L*a*b* space.
/// Red, green and blue must be in the range [0, 1].
Lab rgbToLab(Rgb rgb) {
    const auto linearRed = static_cast<double>(rgb.r <= 0.04045 ? rgb.r / 12.92 : std::pow((rgb.r + 0.055) / (1 + 0.055), 2.4));
    const auto linearGreen = static_cast<double>(rgb.g <= 0.04045 ? rgb.g / 12.92 : std::pow((rgb.g + 0.055) / (1 + 0.055), 2.4));
    const auto linearBlue = static_cast<double>(rgb.b <= 0.04045 ? rgb.b / 12.92 : std::pow((rgb.b + 0.055) / (1 + 0.055), 2.4));

    const auto x = 100 * (0.4124564 * linearRed + 0.3575761 * linearGreen + 0.1804375 * linearBlue);
    const auto y = 100 * (0.2126729 * linearRed + 0.7151522 * linearGreen + 0.0721750 * linearBlue);
    const auto z = 100 * (0.0193339 * linearRed + 0.1191920 * linearGreen + 0.9503041 * linearBlue);

    const auto normalizedX = x / whiteX;
    const auto normalizedY = y / whiteY;
    const auto normalizedZ = z / whiteZ;

    return Lab{
        116 * labTransform(normalizedY) - 16,
        500 * (labTransform(normalizedX) - labTransform(normalizedY)),
        200 * (labTransform(normalizedY) - labTransform(normalizedZ))
    };
};

int main(int argc, char* argv[]) {
    auto showHelp = false;
    try {
        const auto command = pontella::parse(argc, argv, 2, {
            {"framerate", "f"},
            {"refractory", "r"},
            {"threshold", "t"},
            {"black", "b"},
            {"white", "w"},
        }, {
            {"dvs", "d"},
            {"color", "c"},
            {"help", "h"}
        });
        if (command.flags.find("help") != command.flags.end()) {
            showHelp = true;
        } else {

            // retrieve the sharps position and number in the input
            auto beforeSharps = std::string();
            auto afterSharps = std::string();
            std::size_t numberOfSharps = 0;
            {
                auto sharpsRbegin = command.arguments[0].rend();
                auto sharpsRend = command.arguments[0].rend();
                auto sharpsStarted = false;
                for (auto characterIterator = command.arguments[0].rbegin(); characterIterator != command.arguments[0].rend(); ++characterIterator) {
                    if (!sharpsStarted) {
                        if (*characterIterator == '#') {
                            sharpsRbegin = characterIterator;
                            sharpsStarted = true;
                        }
                    } else if (*characterIterator != '#') {
                        sharpsRend = characterIterator;
                        break;
                    }
                }
                if (sharpsRbegin == command.arguments[0].rend()) {
                    throw std::runtime_error("the input filename does not contain sharps");
                }
                beforeSharps = std::string(sharpsRend, command.arguments[0].rend());
                std::reverse(beforeSharps.begin(), beforeSharps.end());
                afterSharps = std::string(command.arguments[0].rbegin(), sharpsRbegin);
                std::reverse(afterSharps.begin(), afterSharps.end());
                numberOfSharps = sharpsRend - sharpsRbegin;
            }

            // retrive the framerate
            double framerate = 1000;
            {
                const auto framerateCandidate = command.options.find("framerate");
                if (framerateCandidate != command.options.end()) {
                    try {
                        framerate = std::stod(framerateCandidate->second);
                        if (framerate == 0) {
                            throw std::invalid_argument("");
                        }
                    } catch (const std::invalid_argument&) {
                        throw std::runtime_error("framerate must be a positive number");
                    }
                }
            }

            // retrive the refractory period
            uint64_t refractory = 1000;
            {
                const auto refractoryCandidate = command.options.find("refractory");
                if (refractoryCandidate != command.options.end()) {
                    try {
                        refractory = std::stoull(refractoryCandidate->second);
                    } catch (const std::invalid_argument&) {
                        throw std::runtime_error("framerate must be a positive number");
                    }
                }
            }

            // retrieve the threshold
            auto threshold = 0.1;
            if (command.flags.find("color") != command.flags.end()) {
                threshold = 10;
            }
            {
                const auto thresholdCandidate = command.options.find("threshold");
                if (thresholdCandidate != command.options.end()) {
                    try {
                        threshold = std::stod(thresholdCandidate->second);
                        if (threshold <= 0) {
                            throw std::invalid_argument("");
                        }
                    } catch (const std::invalid_argument&) {
                        throw std::runtime_error("threshold must be a positive number");
                    }
                }
            }

            if (command.flags.find("dvs") != command.flags.end()) {

                // open the output
                sepia::EventStreamWriter eventStreamWriter;
                eventStreamWriter.open(command.arguments[1]);
                auto exposures = std::vector<double>(304 * 240, 0.0);

                // compute constants
                const auto frameDuration = 1e6 / framerate;

                // load the frames while available
                auto timestampsThresholds = std::vector<double>(304 * 240, 0);
                for (std::size_t frameIndex = 0; frameIndex < std::pow(10, numberOfSharps); ++frameIndex) {
                    auto frame = std::vector<uint8_t>();
                    {
                        const auto filename = (
                            std::stringstream()
                            << beforeSharps
                            << std::setfill('0')
                            << std::setw(static_cast<int>(numberOfSharps))
                            << frameIndex
                            << afterSharps
                        ).str();
                        uint32_t width = 0;
                        uint32_t height = 0;
                        const auto error = lodepng::decode(frame, width, height, filename);
                        if (error == 78) {
                            if (frameIndex > 0) {
                                break;
                            }
                            throw sepia::UnreadableFile(filename);
                        } else if (error > 0) {
                            throw std::runtime_error("error thrown while decoding '" + filename + "': " + lodepng_error_text(error));
                        }
                        if (width != 304 || height != 240) {
                            throw std::runtime_error("the file '" + filename + "' does not have the expected dimensions");
                        }
                        std::cout << "Loaded '" << filename << "'" << std::endl;
                    }
                    for (auto pixel = frame.begin(); pixel != frame.end(); std::advance(pixel, 4)) {
                        const auto pixelIndex = (pixel - frame.begin()) / 4;
                        const auto lab = rgbToLab(Rgb{*pixel / 255.0, *std::next(pixel, 1) / 255.0, *std::next(pixel, 2) / 255.0});
                        const auto exposureRatio = (exposures[pixelIndex] == 0 ?
                            (lab.l == 0 ? 1 : std::numeric_limits<double>::infinity())
                            :
                            lab.l / exposures[pixelIndex]
                        );

                        if (frameIndex * frameDuration >= timestampsThresholds[pixelIndex]) {
                            if (exposureRatio > (1.0 + threshold)) {
                                eventStreamWriter(sepia::Event{
                                    static_cast<uint16_t>(pixelIndex % 304),
                                    static_cast<uint16_t>(240 - 1 - pixelIndex / 304),
                                    static_cast<uint64_t>(frameIndex * frameDuration),
                                    false,
                                    true
                                });
                                exposures[pixelIndex] = lab.l;
                                timestampsThresholds[pixelIndex] = frameIndex * frameDuration + refractory;
                            } else if (exposureRatio < (1.0 - threshold)) {
                                eventStreamWriter(sepia::Event{
                                    static_cast<uint16_t>(pixelIndex % 304),
                                    static_cast<uint16_t>(240 - 1 - pixelIndex / 304),
                                    static_cast<uint64_t>(frameIndex * frameDuration),
                                    false,
                                    false
                                });
                                exposures[pixelIndex] = lab.l;
                                timestampsThresholds[pixelIndex] = frameIndex * frameDuration + refractory;
                            }
                        }
                    }
                }
            } else if (command.flags.find("color") != command.flags.end()) {

                // open the output
                sepia::ColorEventStreamWriter colorEventStreamWriter;
                colorEventStreamWriter.open(command.arguments[1]);
                auto labs = std::vector<Lab>(304 * 240, Lab{0, 0, 0});

                // compute constants
                const auto squaredThreshold = std::pow(threshold, 2);
                const auto frameDuration = 1e6 / framerate;

                // load the frames while available
                auto timestampsThresholds = std::vector<double>(304 * 240, 0);
                for (std::size_t frameIndex = 0; frameIndex < std::pow(10, numberOfSharps); ++frameIndex) {
                    auto frame = std::vector<uint8_t>();
                    {
                        const auto filename = (
                            std::stringstream()
                            << beforeSharps
                            << std::setfill('0')
                            << std::setw(static_cast<int>(numberOfSharps))
                            << frameIndex
                            << afterSharps
                        ).str();
                        uint32_t width = 0;
                        uint32_t height = 0;
                        const auto error = lodepng::decode(frame, width, height, filename);
                        if (error == 78) {
                            if (frameIndex > 0) {
                                break;
                            }
                            throw sepia::UnreadableFile(filename);
                        } else if (error > 0) {
                            throw std::runtime_error("error thrown while decoding '" + filename + "': " + lodepng_error_text(error));
                        }
                        if (width != 304 || height != 240) {
                            throw std::runtime_error("the file '" + filename + "' does not have the expected dimensions");
                        }
                        std::cout << "Loaded '" << filename << "'" << std::endl;
                    }

                    for (auto pixel = frame.begin(); pixel != frame.end(); std::advance(pixel, 4)) {
                        const auto pixelIndex = (pixel - frame.begin()) / 4;
                        const auto lab = rgbToLab(Rgb{*pixel / 255.0, *std::next(pixel, 1) / 255.0, *std::next(pixel, 2) / 255.0});
                        if (
                            std::pow(lab.l - labs[pixelIndex].l, 2) + std::pow(lab.a - labs[pixelIndex].a, 2) + std::pow(lab.b - labs[pixelIndex].b, 2)
                            >
                            squaredThreshold
                            &&
                            frameIndex * frameDuration >= timestampsThresholds[pixelIndex]
                        ) {
                            colorEventStreamWriter(sepia::ColorEvent{
                                static_cast<uint16_t>(pixelIndex % 304),
                                static_cast<uint16_t>(240 - 1 - pixelIndex / 304),
                                static_cast<uint64_t>(frameIndex * frameDuration),
                                *pixel,
                                *std::next(pixel, 1),
                                *std::next(pixel, 2),
                            });
                            labs[pixelIndex] = lab;
                            timestampsThresholds[pixelIndex] = frameIndex * frameDuration + refractory;
                        }
                    }
                }
            } else {

                // retrieve the black and white exposure times
                uint64_t black = 100000;
                {
                    const auto blackCandidate = command.options.find("black");
                    if (blackCandidate != command.options.end()) {
                        try {
                            black = std::stoull(blackCandidate->second);
                            if (black == 0) {
                                throw std::invalid_argument("");
                            }
                        } catch (const std::invalid_argument&) {
                            throw std::runtime_error("black exposure time must be a positive integer");
                        }
                    }
                }
                uint64_t white = 1000;
                {
                    const auto whiteCandidate = command.options.find("white");
                    if (whiteCandidate != command.options.end()) {
                        try {
                            white = std::stoull(whiteCandidate->second);
                            if (white == 0) {
                                throw std::invalid_argument("");
                            }
                        } catch (const std::invalid_argument&) {
                            throw std::runtime_error("white exposure time must be a positive integer");
                        }
                    }
                }
                if (white >= black) {
                    throw std::runtime_error("the white exposure time must be smaller than the black exposure time");
                }

                // open the output
                sepia::EventStreamWriter eventStreamWriter;
                eventStreamWriter.open(command.arguments[1]);
                auto exposures = std::vector<double>(304 * 240, 0.0);
                auto charges = std::vector<double>(304 * 240, 0.0);
                auto acquiring = std::vector<bool>(304 * 240, false);

                // compute constants
                const auto chargeConstant = 1.0 / black;
                const auto chargeRatio = 0.01 * (1.0 / white - chargeConstant);
                const auto frameDuration = 1e6 / framerate;

                // load the frames while available
                auto timestampsThresholds = std::vector<double>(304 * 240, 0);
                for (std::size_t frameIndex = 0; frameIndex < std::pow(10, numberOfSharps); ++frameIndex) {
                    auto frame = std::vector<uint8_t>();
                    {
                        const auto filename = (
                            std::stringstream()
                            << beforeSharps
                            << std::setfill('0')
                            << std::setw(static_cast<int>(numberOfSharps))
                            << frameIndex
                            << afterSharps
                        ).str();
                        uint32_t width = 0;
                        uint32_t height = 0;
                        const auto error = lodepng::decode(frame, width, height, filename);
                        if (error == 78) {
                            if (frameIndex > 0) {
                                break;
                            }
                            throw sepia::UnreadableFile(filename);
                        } else if (error > 0) {
                            throw std::runtime_error("error thrown while decoding '" + filename + "': " + lodepng_error_text(error));
                        }
                        if (width != 304 || height != 240) {
                            throw std::runtime_error("the file '" + filename + "' does not have the expected dimensions");
                        }
                        std::cout << "Loaded '" << filename << "'" << std::endl;
                    }
                    auto secondThresholdCrossings = std::vector<sepia::Event>();
                    for (auto pixel = frame.begin(); pixel != frame.end(); std::advance(pixel, 4)) {
                        const auto pixelIndex = (pixel - frame.begin()) / 4;
                        const auto lab = rgbToLab(Rgb{*pixel / 255.0, *std::next(pixel, 1) / 255.0, *std::next(pixel, 2) / 255.0});
                        const auto exposureRatio = (exposures[pixelIndex] == 0 ?
                            (lab.l == 0 ? 1 : std::numeric_limits<double>::infinity())
                            :
                            lab.l / exposures[pixelIndex]
                        );

                        if (frameIndex * frameDuration >= timestampsThresholds[pixelIndex]) {
                            if (exposureRatio > (1.0 + threshold)) {
                                eventStreamWriter(sepia::Event{
                                    static_cast<uint16_t>(pixelIndex % 304),
                                    static_cast<uint16_t>(240 - 1 - pixelIndex / 304),
                                    static_cast<uint64_t>(frameIndex * frameDuration),
                                    false,
                                    true
                                });
                                eventStreamWriter(sepia::Event{
                                    static_cast<uint16_t>(pixelIndex % 304),
                                    static_cast<uint16_t>(240 - 1 - pixelIndex / 304),
                                    static_cast<uint64_t>(frameIndex * frameDuration),
                                    true,
                                    false
                                });
                                exposures[pixelIndex] = lab.l;
                                charges[pixelIndex] = 0;
                                timestampsThresholds[pixelIndex] = frameIndex * frameDuration + refractory;
                                acquiring[pixelIndex] = true;
                            } else if (exposureRatio < (1.0 - threshold)) {
                                eventStreamWriter(sepia::Event{
                                    static_cast<uint16_t>(pixelIndex % 304),
                                    static_cast<uint16_t>(240 - 1 - pixelIndex / 304),
                                    static_cast<uint64_t>(frameIndex * frameDuration),
                                    false,
                                    false
                                });
                                eventStreamWriter(sepia::Event{
                                    static_cast<uint16_t>(pixelIndex % 304),
                                    static_cast<uint16_t>(240 - 1 - pixelIndex / 304),
                                    static_cast<uint64_t>(frameIndex * frameDuration),
                                    true,
                                    false
                                });
                                exposures[pixelIndex] = lab.l;
                                charges[pixelIndex] = 0;
                                timestampsThresholds[pixelIndex] = frameIndex * frameDuration + refractory;
                                acquiring[pixelIndex] = true;
                            }
                        }
                        if (acquiring[pixelIndex]) {
                            const auto newCharge = charges[pixelIndex] + (chargeRatio * lab.l + chargeConstant) * frameDuration;
                            if (newCharge < 1) {
                                charges[pixelIndex] = newCharge;
                            } else {
                                secondThresholdCrossings.push_back(sepia::Event{
                                    static_cast<uint16_t>(pixelIndex % 304),
                                    static_cast<uint16_t>(240 - 1 - pixelIndex / 304),
                                    static_cast<uint64_t>(frameIndex * frameDuration + (1 - charges[pixelIndex]) / (chargeRatio * lab.l + chargeConstant)),
                                    true,
                                    true
                                });
                                charges[pixelIndex] = 0;
                                acquiring[pixelIndex] = false;
                            }
                        }
                    }
                    std::sort(secondThresholdCrossings.begin(), secondThresholdCrossings.end(), [](const sepia::Event& first, const sepia::Event& second) {
                        return first.timestamp < second.timestamp;
                    });
                    for (auto&& event : secondThresholdCrossings) {
                        eventStreamWriter(event);
                    }
                }
            }
        }
    } catch (const std::runtime_error& exception) {
        showHelp = true;
        if (!pontella::test(argc, argv, {"help", "h"})) {
            std::cout << "\e[31m" << exception.what() << "\e[0m" << std::endl;
        }
    }

    if (showHelp) {
        std::cout <<
            "ShiftTheParadigm converts a set of png frames into an Event Stream file.\n"
            "Syntax: ./shiftTheParadigm [options] /path/to/frame#####.png /path/to/output.es\n"
            "    the sharps in the input filename are replaced by numbers (starting at 0), and can appear anywhere in the name\n"
            "    if there are several distinct sets of sharps in the filename (as an example, '/path/to/directory#/frame#####_####.png'),\n"
            "    the last one is used (with the previous example, the first frame would be '/path/to/directory#/frame#####_0000.png')\n"
            "    the input frames must be 304 pixels wide and 240 pixels tall\n"
            "Available options:\n"
            "    -f [framerate], --framerate [framerate]                    sets the input number of frames per second (defaults to 1000)\n"
            "    -r [refractory], --refractory [refractory]                 sets the pixel refractory period in microseconds (defaults to 1000)\n"
            "    -d, --dvs                                                  generates only change detections instead of ATIS events\n"
            "    -c, --color                                                generates color events instead of ATIS events\n"
            "                                                                   ignored when using the dvs switch\n"
            "    -t [threshold], --threshold [threshold]                    sets the relative luminance threshold for triggering an event (defaults to 0.1)\n"
            "                                                                   when using the color switch, represents the minimum distance in L*a*b*\n"
            "                                                                   space instead (defaults to 10)\n"
            "    -b [black exposure time], --black [black exposure time]    sets the black exposure time in microseconds (defaults to 100000)\n"
            "                                                                   ignored when using the dvs or color switches\n"
            "    -w [white exposure time], --white [white exposure time]    sets the white exposure time in microseconds (defaults to 1000)\n"
            "                                                                   ignored when using the dvs or color switches\n"
            "                                                                   the white exposure time must be smaller than the black exposure time\n"
            "    -h, --help                                                 shows this help message\n"
        << std::endl;
    }

    return 0;
}
