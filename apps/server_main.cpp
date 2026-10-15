#include "server/Server.h"
#include <iostream>

int main(int argc, char* argv[]) {
    std::uint16_t port = blob::config::kServerPort;
    if (argc >= 2) {
        port = static_cast<std::uint16_t>(std::atoi(argv[1]));
    }

    try {
        blob::Server server(port);
        blob::Server::InstallSignalHandler(&server);

        std::cout << "=== Blob Arena Server ===\n";
        std::cout << "Press Ctrl+C to stop.\n\n";

        server.Run();
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}