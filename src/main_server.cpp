#include "Server.h"
#include <csignal>
#include <fstream>
#include <unordered_map>
#include <iostream>

namespace {
Server* g_server = nullptr;

void handleSignal(int) {
    if (g_server) g_server->stop();
}

std::unordered_map<std::string, std::string> loadConfig(const std::string& path) {
    std::unordered_map<std::string, std::string> config;
    std::ifstream file(path);
    if (!file) {
        std::cout << "No config file found at " << path << ", using defaults.\n";
        return config;
    }
    std::string line;
    while (std::getline(file, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        config[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return config;
}
}

int main() {
    auto config = loadConfig("matchcore.conf");

    int port = config.count("port") ? std::stoi(config["port"]) : 9000;
    int threads = config.count("threads") ? std::stoi(config["threads"]) : 4;
    std::string dbPath = config.count("db_path") ? config["db_path"] : "matchcore.db";

    Server server(port, threads, dbPath);
    g_server = &server;

    std::signal(SIGINT, handleSignal);

    server.run();
    return 0;
}