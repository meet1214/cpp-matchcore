#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace {

std::atomic<int> failures{0};

int connectToServer() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9000);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect failed");
        return -1;
    }
    return fd;
}

std::string sendAndReceive(int fd, const std::string& line) {
    std::string msg = line + "\n";
    write(fd, msg.c_str(), msg.size());
    char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) return "";
    buf[n] = '\0';
    return std::string(buf);
}

void workerThread(int threadId, const std::string& accountNumber, const std::string& password, int ordersPerThread) {
    int fd = connectToServer();
    if (fd < 0) { failures++; return; }

    char buf[1024];
    read(fd, buf, sizeof(buf)); // discard welcome banner

    std::string loginResp = sendAndReceive(fd, "LOGIN " + accountNumber + " " + password);
    if (loginResp.find("logged in") == std::string::npos) {
        std::cerr << "Thread " << threadId << " login failed\n";
        failures++;
        close(fd);
        return;
    }

    for (int i = 0; i < ordersPerThread; i++) {
        sendAndReceive(fd, "BUY 100 1");
        sendAndReceive(fd, "SELL 100 1");
    }

    close(fd);
}

}

int main() {
    // Register the shared stress-test account first, on its own connection.
    int setupFd = connectToServer();
    if (setupFd < 0) return 1;
    char buf[1024];
    read(setupFd, buf, sizeof(buf)); // discard welcome

    std::string regResp = sendAndReceive(setupFd, "REGISTER stresspass Stress Tester");
    std::string accountNumber;
    // Extract "MC..........." from the response text.
    size_t pos = regResp.find("MC");
    if (pos != std::string::npos) {
        accountNumber = regResp.substr(pos, 12); // "MC" + 10 digits
    }
    close(setupFd);

    if (accountNumber.empty()) {
        std::cerr << "Could not register stress-test account, aborting.\n";
        return 1;
    }
    std::cout << "Registered stress account: " << accountNumber << "\n";

    const int numThreads = 10;
    const int ordersPerThread = 50; // 50 BUY + 50 SELL per thread

    std::vector<std::thread> workers;
    for (int i = 0; i < numThreads; i++) {
        workers.emplace_back(workerThread, i, accountNumber, "stresspass", ordersPerThread);
    }
    for (auto& t : workers) t.join();

    std::cout << "\nAll threads finished. Failures: " << failures.load() << "\n";
    std::cout << "Total orders sent: " << (numThreads * ordersPerThread * 2) << "\n";
    std::cout << "Now run: matchcore_client -> BOOK  (expect depth 0/0)\n";
    std::cout << "And check: sqlite3 matchcore.db \"SELECT COUNT(*) FROM trades;\"\n";
    std::cout << "Expected trade count: " << (numThreads * ordersPerThread) << "\n";

    return failures.load() == 0 ? 0 : 1;
}