#include "Server.h"

int main() {
    Server server(9000, 4, "matchcore.db");
    server.run();
    return 0;
}