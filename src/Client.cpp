#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace {

int sockFd;

std::string sendAndReceive(const std::string& line) {
    std::string msg = line + "\n";
    write(sockFd, msg.c_str(), msg.size());

    char buf[1024];
    ssize_t n = read(sockFd, buf, sizeof(buf) - 1);
    if (n <= 0) return "";
    buf[n] = '\0';
    return std::string(buf);
}

bool authMenu() {
    while (true) {
        std::cout << "\n===== MatchCore =====\n"
                   << "1) Register\n2) Login\n3) Quit\n"
                   << "Choice: ";
        std::string choice;
        std::getline(std::cin, choice);

        if (choice == "3") return false;
        if (choice != "1" && choice != "2") {
            std::cout << "Invalid choice, try again.\n";
            continue;
        }

        if (choice == "1") {
            std::string name, password;
            std::cout << "Name: ";
            std::getline(std::cin, name);
            std::cout << "Password: ";
            std::getline(std::cin, password);
            std::cout << "\n" << sendAndReceive("REGISTER " + password + " " + name);
        } else {
            std::string accountNumber, password;
            std::cout << "Account number: ";
            std::getline(std::cin, accountNumber);
            std::cout << "Password: ";
            std::getline(std::cin, password);
            std::string resp = sendAndReceive("LOGIN " + accountNumber + " " + password);
            std::cout << "\n" << resp;
            if (resp.find("logged in") != std::string::npos) return true;
        }
    }
}

}

int main() {
    sockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFd < 0) { perror("socket failed"); return 1; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9000);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sockFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect failed");
        close(sockFd);
        return 1;
    }

    char buf[1024];
    ssize_t n = read(sockFd, buf, sizeof(buf) - 1);
    if (n > 0) { buf[n] = '\0'; std::cout << buf; }

    if (!authMenu()) {
        close(sockFd);
        return 0;
    }

    std::cout << "\n===== Welcome! =====\n"
               << "Commands: BUY <symbol> <price> <qty> [IOC] | SELL <price> <qty> | CANCEL <id> | BOOK | HELP | QUIT\n\n";

    std::string line;
    while (true) {
        std::cout << "matchcore> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        std::cout << sendAndReceive(line) << "\n";
        if (line == "QUIT") break;
    }

    close(sockFd);
    return 0;
}