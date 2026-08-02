#include "Server.h"

int main() {
    Server server(9000, 4); // 4 worker threads
    server.run();
    return 0;
}