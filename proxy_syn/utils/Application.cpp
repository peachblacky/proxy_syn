

#include <Constants.h>
#include <proxy/Proxy.h>

#define TAG "main"

int main(int argc, char **argv) {
    Logger log(Constants::DEBUG, std::cout);

    if(argv[1] == nullptr) {
        log.err(TAG, "Please enter port to start on in arguments");
        exit(EXIT_FAILURE);
    }

    log.info(TAG, "Starting application on port " + std::string(argv[1]));

    auto proxy = new Proxy(std::stoi(argv[1]));
    proxy->startListeningMode();
    proxy->launch();

    delete proxy;
}