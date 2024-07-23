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

#define SOCKET_PATH "/tmp/interfaceMon"
#define BUF_SIZE 200

using namespace std;

//keep track of client file descriptors and interface names
vector<int> clientFds;
vector<string> interfaces;

int serverFd; // File descriptor for the server socket

// Signal handler to manage graceful shutdown on SIGINT (Ctrl+C)
void handleSigint(int sigint) {
    // Notify all clients to shut down
    for(auto i = 0; i < clientFds.size(); ++i){
        write(clientFds[i], "shut down", 10);
    }
    
    // Close the server socket and remove the socket file
    close(serverFd);
    unlink(SOCKET_PATH);
    
    // Close all client sockets
    for(auto i = 0; i < clientFds.size(); ++i){
        close(clientFds[i]);
    }
    exit(0);
}

int main() {
    // Register signal handler for SIGINT
    signal(SIGINT, handleSigint);

    struct sockaddr_un name;
    char buffer[BUF_SIZE];

    int interfaceCount;
    cout << "enter # interfaces to monitor: ";
    cin >> interfaceCount;

    // Get interface names from user input and store them in the vector
    for (int i = 0; i < interfaceCount; ++i) {
        string interface;
        cout << "Enter name of interface " << i + 1 << ": ";
        cin >> interface;
        interfaces.push_back(interface);
    }

    // Create the server socket
    serverFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serverFd < 0) {
        perror("socket");
        exit(-1);
    }

    // Initialize the sockaddr_un structure and set the socket path
    memset(&name, 0, sizeof(struct sockaddr_un));
    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, SOCKET_PATH, sizeof(name.sun_path) - 1);

    // Bind the server socket to the specified path
    if (bind(serverFd, (struct sockaddr*)&name, sizeof(name)) < 0) {
        perror("bind");
        exit(-1);
    }

    // Listen for incoming connections, with a backlog of 'interfaceCount'
    if(listen(serverFd, interfaceCount) < 0){
        perror("listen");
        exit(-1);
    }

    cout << "Waiting for connections..." << endl;

    fd_set active_fd_set; // Set of active file descriptors
    fd_set read_fd_set; // Set of file descriptors ready for reading
    FD_ZERO(&active_fd_set); // Initialize the set to empty
    FD_SET(serverFd, &active_fd_set); // Add the server socket to the set
    int max_fd = serverFd; // Track the highest file descriptor number

    // Fork a child process for each interface to monitor
    for(auto i = 0; i < interfaces.size(); ++i){
        const string &interface = interfaces[i];
        
        if(fork() == 0){
            execl("./interfaceMon", "./interfaceMon", interface.c_str(), NULL); // Execute the interface monitor process
            exit(0); // Ensure the child process exits after execution
        }
    }

    // Main loop to handle incoming connections and data
    while(true){
        read_fd_set = active_fd_set; // Copy the active file descriptor set
        
        // Wait for activity on any of the file descriptors
        if(select(max_fd + 1, &read_fd_set, NULL, NULL, NULL) < 0){
            perror("select");
            break; // Break the loop on error
        }

        // Iterate over all file descriptors up to the highest one
        for(int i = 0; i <= max_fd; ++i){
            if (FD_ISSET(i, &read_fd_set)){
                if (i == serverFd) {
                    // Handle new client connection
                    int clientFd = accept(serverFd, NULL, NULL);
                    if (clientFd < 0) {
                        perror("accept");
                    } 
                    else {
                        // Add the new client to the active set and update max_fd
                        FD_SET(clientFd, &active_fd_set);
                        if(clientFd > max_fd){
                            max_fd = clientFd;
                        }
                        
                        clientFds.push_back(clientFd); // Store the client's file descriptor
                        cout << "Accepted connection from client" << endl;
                    }
                } 
                else {
                    // Handle data from an existing client
                    int ret = read(i, buffer, BUF_SIZE);
                    
                    if(ret <= 0){
                        if (ret < 0){
                            perror("read");
                        }
                        
                        // Close and remove the client on read error or disconnection
                        close(i);
                        FD_CLR(i, &active_fd_set);
                        clientFds.erase(remove(clientFds.begin(), clientFds.end(), i), clientFds.end());
                        cout << "Client disconnected" << endl;
                    } 
                    else {
                        buffer[ret] = '\0'; // Null-terminate the received data
                        cout << buffer << endl;
                        
                        // Respond to specific messages from the client
                        if (strncmp(buffer, "Ready", 5) == 0) {
                            write(i, "Monitor", 7);
                        } 
                        else if (strncmp(buffer, "Link Down", 9) == 0) {
                            write(i, "Set Link Up", 11);
                        } 
                        else if (strncmp(buffer, "Done", 4) == 0) {
                            // Close and remove the client when it is done
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

    // Cleanup code: Close all open file descriptors and remove the socket file
    close(serverFd);
    unlink(SOCKET_PATH);
    for (auto i = 0; i < clientFds.size(); ++i) {
        close(clientFds[i]);
    }

    return 0;
}
