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

int serverFd;

//signal handler for SIGINT
void handleSigint(int sigint) {
    //tells clients to shut down
    for(auto i = 0; i < clientFds.size(); ++i){
        write(clientFds[i], "shut down", 10);
    }
    
    //close server socket and remove the file
    close(serverFd);
    unlink(SOCKET_PATH);
    
    //close client sockets
    for(auto i = 0; i < clientFds.size(); ++i){
        close(clientFds[i]);
    }
    exit(0);
}

int main() {
    struct sigaction sa;
    sa.sa_handler = handleSigint;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    struct sockaddr_un name;
    char buffer[BUF_SIZE];

    int interfaceCount;
    cout << "enter # interfaces to monitor: ";
    cin >> interfaceCount;

    //get interface names
    for (int i = 0; i < interfaceCount; ++i) {
        string interface;
        cout << "Enter name of interface " << i + 1 << ": ";
        cin >> interface;
        interfaces.push_back(interface);
    }

    //create server socket
    serverFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serverFd < 0) {
        perror("socket");
        exit(-1);
    }

    //initialize sockaddr_un structure and set socket path
    memset(&name, 0, sizeof(name));
    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, SOCKET_PATH, sizeof(name.sun_path) - 1);

    //bind server socket to the path
    if (bind(serverFd, (struct sockaddr*)&name, sizeof(name)) < 0) {
        perror("bind");
        exit(-1);
    }

    //listen for connections
    if(listen(serverFd, interfaceCount) < 0){
        perror("listen");
        exit(-1);
    }

    cout << "waiting for connections" << endl;

    fd_set active_fd_set; //set of active file descriptors
    fd_set read_fd_set; //set of file descriptors for reading
    FD_ZERO(&active_fd_set); //set the set to empty
    FD_SET(serverFd, &active_fd_set); //add server socket to set
    int maxFd = serverFd; // the highest fd number

    //fork child process for each of the interfaces 
    for(auto i = 0; i < interfaces.size(); ++i){
        const string &interface = interfaces[i];
        
        if(fork() == 0){
            //executes the interface monitor process
            execl("./interfaceMon", "./interfaceMon", interface.c_str(), NULL);
            exit(0);
        }
    }

    //main loop for the connections/ data coming in
    while(true){
        read_fd_set = active_fd_set; //copy the active file descriptor set
        
        //wait for activity from the fd's
        if(select(maxFd + 1, &read_fd_set, NULL, NULL, NULL) < 0){
            perror("select");
            break;
        }

        //loops thorugh all fds
        for(int i = 0; i <= maxFd; ++i){
            if (FD_ISSET(i, &read_fd_set)){
                if (i == serverFd) {
                    //to handel new client connection
                    int clientFd = accept(serverFd, NULL, NULL);
                    if (clientFd < 0) {
                        perror("accept");
                    } 
                    else {
                        //add new client to the active set and update maxFd
                        FD_SET(clientFd, &active_fd_set);
                        if(clientFd > maxFd){
                            maxFd = clientFd;
                        }
                        
                        clientFds.push_back(clientFd); //add client fd to the vector
                        cout << "Accepted connection from client" << endl;
                    }
                } 
                else {
                    //handle data from an existing client
                    int ret = read(i, buffer, BUF_SIZE);
                    
                    if(ret <= 0){
                        if (ret < 0){
                            perror("read");
                        }
                        
                        //close client if theres an error or they are disconnection
                        close(i);
                        FD_CLR(i, &active_fd_set);
                        clientFds.erase(remove(clientFds.begin(), clientFds.end(), i), clientFds.end());
                        cout << "Client disconnected" << endl;
                    } 
                    else {
                        buffer[ret] = '\0';
                        cout << buffer << endl;
                        
                        //responses for client messages
                        if (strncmp(buffer, "Ready", 5) == 0) {
                            write(i, "Monitor", 7);
                        } 
                        else if (strncmp(buffer, "Link Down", 9) == 0) {
                            write(i, "Set Link Up", 11);
                        } 
                        else if (strncmp(buffer, "Done", 4) == 0) {
                            //close client when it is done
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

    //cleans code
    close(serverFd);
    unlink(SOCKET_PATH);
    for (auto i = 0; i < clientFds.size(); ++i) {
        close(clientFds[i]);
    }

    return 0;
}
