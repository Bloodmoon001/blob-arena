#include "client/ClientGame.h"
#include "common/Config.h"

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    std::uint16_t port = blob::config::kServerPort;
    std::string name = "Player";

    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = static_cast<std::uint16_t>(std::atoi(argv[2]));
    if (argc >= 4) name = argv[3];

    try {
        blob::ClientGame game(host, port, name);
        game.Run();
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}