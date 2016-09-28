#include "pontella.hpp"
#include "opalKellyAtisSepia.hpp"

#include <iostream>
#include <mutex>

int main(int argc, char* argv[]) {
    auto showHelp = false;
    try {
        const auto result = pontella::parse(argc, argv, 2, {}, {{"help", "h"}});
        if (result.flags.find("help") != result.flags.end()) {
            showHelp = true;
        } else {
            if (result.arguments[0] == result.arguments[1]) {
                throw std::runtime_error("The oka input and the csv output must be different files");
            }

            std::ofstream csvFile(result.arguments[1]);
            if (!csvFile.good()) {
                throw sepia::UnreadableFile(result.arguments[1]);
            }

            std::mutex lock;
            lock.lock();
            auto logObservableException = std::current_exception();
            auto logObservable = opalKellyAtisSepia::make_logObservable(
                [&csvFile](sepia::Event event) -> void {
                    csvFile << event.x << "\t" << event.y << "\t" << event.timestamp << "\t" << event.isThresholdCrossing << "\t" << event.polarity << "\n";
                },
                [&lock, &logObservableException](std::exception_ptr exception) -> void {
                    logObservableException = exception;
                    lock.unlock();
                },
                result.arguments[0],
                sepia::LogObservable::Dispatch::asFastAsPossible
            );
            lock.lock();
            lock.unlock();
            std::rethrow_exception(logObservableException);
        }
    } catch (const sepia::EndOfFile&) {
        // normal end
    } catch (const std::runtime_error& exception) {
        showHelp = true;
        if (!pontella::test(argc, argv, {"help", "h"})) {
            std::cout << "\e[31m" << exception.what() << "\e[0m" << std::endl;
        }
    }

    if (showHelp) {
        std::cout
            << "Syntax: ./okaToCsv [options] /path/to/input.oka /path/to/output.csv\n"
            << "Available options:\n"
            << "    -h, --help    show this help message\n"
        << std::endl;
    }

    return 0;
}
