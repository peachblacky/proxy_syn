

#include <Constants.h>
#include <proxy/Proxy.h>

#define TAG "main"

int main(int argc, char **argv) {
    Logger log = *new Logger(Constants::DEBUG, std::cout);

    log.info(TAG, "Starting application on port " + std::string(argv[1]));

    auto proxy = new Proxy(std::stoi(argv[1]));
    proxy->start_listening_mode();
    proxy->launch();
}