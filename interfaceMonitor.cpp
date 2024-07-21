#include <iostream>
#include <string>
#include <vector>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <sys/select.h>

#define SOCKET_PATH "/tmp/interfaceMonitor.socket"
#define BUF_SIZE 200

using namespace std;

int serverFd;
vector<int> clientFds;
vector<string> interfaces;

void handleSigint(int sig) {
    for (int fd : clientFds) {
        write(fd, "Shut Down", 10);
    }
    close(serverFd);
    unlink(SOCKET_PATH);
    for (int fd : clientFds) {
        close(fd);
    }
    exit(0);
}

int main() {
    signal(SIGINT, handleSigint);

    struct sockaddr_un name;
    char buffer[BUF_SIZE];

    int numInterfaces;
    cout << "Enter # interfaces to monitor: ";
    cin >> numInterfaces;

    for (int i = 0; i < numInterfaces; ++i) {
        string interface;
        cout << "Enter name of interface " << i + 1 << ": ";
        cin >> interface;
        interfaces.push_back(interface);
    }

    serverFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serverFd < 0) {
        perror("socket");
        exit(-1);
    }

    memset(&name, 0, sizeof(struct sockaddr_un));
    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, SOCKET_PATH, sizeof(name.sun_path) - 1);

    if (bind(serverFd, (struct sockaddr*)&name, sizeof(name)) < 0) {
        perror("bind");
        exit(-1);
    }

    if (listen(serverFd, numInterfaces) < 0) {
        perror("listen");
        exit(-1);
    }

    cout << "Waiting for connections..." << endl;

    fd_set active_fd_set;
    fd_set read_fd_set;
    FD_ZERO(&active_fd_set);
    FD_SET(serverFd, &active_fd_set);
    int max_fd = serverFd;

    while (true) {
        read_fd_set = active_fd_set;
        if (select(max_fd + 1, &read_fd_set, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        for (int i = 0; i <= max_fd; ++i) {
            if (FD_ISSET(i, &read_fd_set)) {
                if (i == serverFd) {
                    int clientFd = accept(serverFd, NULL, NULL);
                    if (clientFd < 0) {
                        perror("accept");
                    } else {
                        FD_SET(clientFd, &active_fd_set);
                        if (clientFd > max_fd) {
                            max_fd = clientFd;
                        }
                        clientFds.push_back(clientFd);
                        cout << "Accepted connection from client" << endl;
                    }
                } else {
                    int ret = read(i, buffer, BUF_SIZE);
                    if (ret <= 0) {
                        if (ret < 0) {
                            perror("read");
                        }
                        close(i);
                        FD_CLR(i, &active_fd_set);
                        clientFds.erase(remove(clientFds.begin(), clientFds.end(), i), clientFds.end());
                        cout << "Client disconnected" << endl;
                    } else {
                        buffer[ret] = '\0';
                        cout << "Received from client: " << buffer << endl; // Debug output
                        if (strncmp(buffer, "Ready", 5) == 0) {
                            write(i, "Monitor", 7);
                        } else if (strncmp(buffer, "Link Down", 9) == 0) {
                            write(i, "Set Link Up", 11);
                        } else if (strncmp(buffer, "Done", 4) == 0) {
                            close(i);
                            FD_CLR(i, &active_fd_set);
                            clientFds.erase(remove(clientFds.begin(), clientFds.end(), i), clientFds.end());
                            cout << "Client marked as done" << endl;
                        }
                    }
                }
            }
        }
    }

    close(serverFd);
    unlink(SOCKET_PATH);
    for (int fd : clientFds) {
        close(fd);
    }

    return 0;
}
