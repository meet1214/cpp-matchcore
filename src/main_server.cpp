#include "Server.h"
#include <csignal>

namespace {
Server* g_server = nullptr;

void handleSignal(int) {
    if (g_server) g_server->stop();
}
}

int main() {
    Server server(9000, 4, "matchcore.db");
    g_server = &server;

    std::signal(SIGINT, handleSignal);

    server.run();
    return 0;
}