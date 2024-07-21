#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <net/if.h>
#include <csignal>
#include <errno.h>

#define SOCKET_PATH "/tmp/interfaceMonitor.socket"
#define BUF_SIZE 200

using namespace std;

bool is_running = true;
int sockfd = -1;
string interface;

void handle_sigint(int sig) {
    is_running = false;
}

int set_if_flags(const char *ifname, short flags) {
    struct ifreq ifr;
    int res = 0;
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_flags = flags;

    int skfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (skfd < 0) {
        perror("socket error");
        return 1;
    }

    res = ioctl(skfd, SIOCSIFFLAGS, &ifr);
    if (res < 0) {
        perror("ioctl error");
    }
    close(skfd);
    return res;
}

int set_if_up(const char *ifname) {
    return set_if_flags(ifname, IFF_UP | IFF_RUNNING);
}

void get_interface_stat(const string &interface, string &data) {
    string operstate;
    int carrier_up_count = 0;
    int carrier_down_count = 0;
    int tx_bytes = 0;
    int rx_bytes = 0;
    int rx_dropped = 0;
    int rx_errors = 0;
    int tx_packets = 0;
    int rx_packets = 0;
    int tx_dropped = 0;
    int tx_errors = 0;
    char statPath[BUF_SIZE*2];
    ifstream infile;

    sprintf(statPath, "/sys/class/net/%s/operstate", interface.c_str());
    infile.open(statPath);
    if (infile.is_open()) {
        infile >> operstate;
        infile.close();
    }

    sprintf(statPath, "/sys/class/net/%s/carrier_up_count", interface.c_str());
    infile.open(statPath);
    if (infile.is_open()) {
        infile >> carrier_up_count;
        infile.close();
    }

    sprintf(statPath, "/sys/class/net/%s/carrier_down_count", interface.c_str());
    infile.open(statPath);
    if (infile.is_open()) {
        infile >> carrier_down_count;
        infile.close();
    }

    sprintf(statPath, "/sys/class/net/%s/statistics/tx_bytes", interface.c_str());
    infile.open(statPath);
    if (infile.is_open()) {
        infile >> tx_bytes;
        infile.close();
    }

    sprintf(statPath, "/sys/class/net/%s/statistics/rx_bytes", interface.c_str());
    infile.open(statPath);
    if (infile.is_open()) {
        infile >> rx_bytes;
        infile.close();
    }

    sprintf(statPath, "/sys/class/net/%s/statistics/rx_dropped", interface.c_str());
    infile.open(statPath);
    if (infile.is_open()) {
        infile >> rx_dropped;
        infile.close();
    }

    sprintf(statPath, "/sys/class/net/%s/statistics/rx_errors", interface.c_str());
    infile.open(statPath);
    if (infile.is_open()) {
        infile >> rx_errors;
        infile.close();
    }

    sprintf(statPath, "/sys/class/net/%s/statistics/tx_packets", interface.c_str());
    infile.open(statPath);
    if (infile.is_open()) {
        infile >> tx_packets;
        infile.close();
    }

    sprintf(statPath, "/sys/class/net/%s/statistics/tx_dropped", interface.c_str());
    infile.open(statPath);
    if (infile.is_open()) {
        infile >> tx_dropped;
        infile.close();
    }

    sprintf(statPath, "/sys/class/net/%s/statistics/tx_errors", interface.c_str());
    infile.open(statPath);
    if (infile.is_open()) {
        infile >> tx_errors;
        infile.close();
    }

    sprintf(statPath, "/sys/class/net/%s/statistics/rx_packets", interface.c_str());
    infile.open(statPath);
    if (infile.is_open()) {
        infile >> rx_packets;
        infile.close();
    }

    if (operstate == "down") {
        set_if_up(interface.c_str());
    }

    data = "Interface: " + interface + " state: " + operstate + " up_count: " + to_string(carrier_up_count) + " down_count: " + to_string(carrier_down_count) + "\n" +
           "rx_bytes: " + to_string(rx_bytes) + " rx_dropped: " + to_string(rx_dropped) + " rx_errors: " + to_string(rx_errors) + " rx_packets: " + to_string(rx_packets) + "\n" +
           "tx_bytes: " + to_string(tx_bytes) + " tx_dropped: " + to_string(tx_dropped) + " tx_errors: " + to_string(tx_errors) + " tx_packets: " + to_string(tx_packets) + "\n\n";
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <interface>" << endl;
        return EXIT_FAILURE;
    }

    char buffer[BUF_SIZE];
    signal(SIGINT, handle_sigint);
    interface = argv[1];

    struct sockaddr_un addr;
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket error");
        return EXIT_FAILURE;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    while (connect(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) < 0) {
        sleep(1);
    }

    cout << "Connected to server" << endl; // Debug output

    write(sockfd, "Ready", 5);

    while (is_running) {
        int bytes_read = read(sockfd, buffer, BUF_SIZE);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            cout << "Received command: " << buffer << endl; // Debug output
            if (strncmp(buffer, "Monitor", 7) == 0) {
                while (is_running) {
                    string data;
                    get_interface_stat(interface, data);
                    write(sockfd, data.c_str(), data.size());
                    sleep(1);
                }
            } else if (strncmp(buffer, "Set Link Up", 11) == 0) {
                set_if_up(interface.c_str());
                write(sockfd, "Link Up", 7);
            } else if (strncmp(buffer, "Shut Down", 9) == 0) {
                is_running = false;
                write(sockfd, "Done", 4);
                break;
            }
        }
    }

    close(sockfd);
    return 0;
}
