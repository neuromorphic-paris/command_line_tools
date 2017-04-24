#include <pontella.hpp>
#include <sepia.hpp>

#include <iostream>
#include <mutex>

int main(int argc, char* argv[]) {
    auto showHelp = false;
    try {
        const auto command = pontella::parse(argc, argv, 2, {}, {{"help", "h"}});
        if (command.flags.find("help") != command.flags.end()) {
            showHelp = true;
        } else {
            if (command.arguments[0] == command.arguments[1]) {
                throw std::runtime_error("The es input and the csv output must be different files");
            }

            std::ofstream csvFile(command.arguments[1]);
            if (!csvFile.good()) {
                throw sepia::UnreadableFile(command.arguments[1]);
            }

            std::mutex lock;
            lock.lock();
            auto eventStreamObservableException = std::current_exception();
            auto atisEventStreamObservable = sepia::make_atisEventStreamObservable(
                command.arguments[0],
                [&](sepia::AtisEvent atisEvent) -> void {
                    csvFile
                        << atisEvent.x
                        << ",\t"
                        << atisEvent.y
                        << ",\t"
                        << atisEvent.timestamp
                        << ",\t"
                        << atisEvent.isThresholdCrossing
                        << ",\t"
                        << atisEvent.polarity
                    << "\n";
                },
                [&](std::exception_ptr exception) -> void {
                    eventStreamObservableException = exception;
                    lock.unlock();
                },
                sepia::falseFunction,
                sepia::EventStreamObservable::Dispatch::asFastAsPossible
            );
            lock.lock();
            lock.unlock();
            std::rethrow_exception(eventStreamObservableException);
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
        std::cout <<
            "EsToCsv converts an Event Stream file into a csv file (compatible with Excel and Matlab)\n"
            "Syntax: ./esToCsv [options] /path/to/input.es /path/to/output.csv\n"
            "Available options:\n"
            "    -h, --help    shows this help message\n"
        << std::endl;
    }

    return 0;
}
