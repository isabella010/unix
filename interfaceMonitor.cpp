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

#define SOCKET_PATH "/tmp/interfaceMon"
#define BUF_SIZE 200

using namespace std;

bool runing = true; 
int sockfd = -1;
string interface;

//sigaction handler for SIGINT
void handle_sigint(int sig){
    runing = false;
}

//sets the flags for network interface
int set_if_flags(const char *ifname, short flags){
    struct ifreq ifr;
    int res = 0;
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_flags = flags;

    //makes a socket for ioctl
    int skfd = socket(AF_INET, SOCK_DGRAM, 0); 
    if(skfd < 0){
        perror("socket error");
        return 1;
    }

    //set interface flags
    res = ioctl(skfd, SIOCSIFFLAGS, &ifr); 
    if(res < 0){
        perror("ioctl error");
    }
    
    //closes the socket
    close(skfd);
    return res;
}

//brings the interface up
int set_if_up(const char *ifname){
    return set_if_flags(ifname, IFF_UP | IFF_RUNNING);
}

//gets the stats of the spesified network interface
void get_interface_stat(const string &interface, string &data){
    int carrier_up_count = 0;
    int carrier_down_count = 0;
    
    int rx_bytes = 0;
    int rx_dropped = 0;
    int rx_errors = 0;
    int rx_packets = 0;
    
    int tx_bytes = 0;
    int tx_dropped = 0;
    int tx_errors = 0;
    int tx_packets = 0;
    
    string operstate;
    char pathBegin[400];
    
    ifstream infile;

    //reads operational state of interface
    sprintf(pathBegin, "/sys/class/net/%s/operstate", interface.c_str());
    infile.open(pathBegin);
    if(infile.is_open()){
        infile >> operstate;
        infile.close();
    }

    //read carrier up count
    sprintf(pathBegin, "/sys/class/net/%s/carrier_up_count", interface.c_str());
    infile.open(pathBegin);
    if(infile.is_open()){
        infile >> carrier_up_count;
        infile.close();
    }

    //read carrier down count
    sprintf(pathBegin, "/sys/class/net/%s/carrier_down_count", interface.c_str());
    infile.open(pathBegin);
    if(infile.is_open()){
        infile >> carrier_down_count;
        infile.close();
    }

    //read transmitted bytes
    sprintf(pathBegin, "/sys/class/net/%s/statistics/tx_bytes", interface.c_str());
    infile.open(pathBegin);
    if(infile.is_open()){
        infile >> tx_bytes;
        infile.close();
    }

    //received bytes
    sprintf(pathBegin, "/sys/class/net/%s/statistics/rx_bytes", interface.c_str());
    infile.open(pathBegin);
    if(infile.is_open()){
        infile >> rx_bytes;
        infile.close();
    }

    //received dropped packets
    sprintf(pathBegin, "/sys/class/net/%s/statistics/rx_dropped", interface.c_str());
    infile.open(pathBegin);
    if(infile.is_open()){
        infile >> rx_dropped;
        infile.close();
    }

    //received errors
    sprintf(pathBegin, "/sys/class/net/%s/statistics/rx_errors", interface.c_str());
    infile.open(pathBegin);
    if(infile.is_open()){
        infile >> rx_errors;
        infile.close();
    }

    //transmitted packets
    sprintf(pathBegin, "/sys/class/net/%s/statistics/tx_packets", interface.c_str());
    infile.open(pathBegin);
    if(infile.is_open()){
        infile >> tx_packets;
        infile.close();
    }

    //transmitted dropped packets
    sprintf(pathBegin, "/sys/class/net/%s/statistics/tx_dropped", interface.c_str());
    infile.open(pathBegin);
    if(infile.is_open()){
        infile >> tx_dropped;
        infile.close();
    }

    //transmitted errors
    sprintf(pathBegin, "/sys/class/net/%s/statistics/tx_errors", interface.c_str());
    infile.open(pathBegin);
    if(infile.is_open()){
        infile >> tx_errors;
        infile.close();
    }

    //received packets
    sprintf(pathBegin, "/sys/class/net/%s/statistics/rx_packets", interface.c_str());
    infile.open(pathBegin);
    if(infile.is_open()){
        infile >> rx_packets;
        infile.close();
    }

    //putting all the stats together into a var
    data = "Interface:" + interface +
           " state:" + operstate +
           " up_count:" + to_string(carrier_up_count) +
           " down_count:" + to_string(carrier_down_count) +
           " rx_bytes:" + to_string(rx_bytes) +
           " rx_dropped:" + to_string(rx_dropped) +
           " rx_errors:" + to_string(rx_errors) +
           " rx_packets:" + to_string(rx_packets) +
           " tx_bytes:" + to_string(tx_bytes) +
           " tx_dropped:" + to_string(tx_dropped) +
           " tx_errors:" + to_string(tx_errors) +
           " tx_packets:" + to_string(tx_packets) + 
           "\n";
}

int main(int argc, char *argv[]) {
    interface = argv[1]; //gets the interface name from cmd line

    // Set up sigaction for SIGINT
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    struct sockaddr_un addr;
    char buffer[BUF_SIZE];

    //makes the domain socket
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(sockfd < 0){
        perror("socket");
        exit(-1);
    }

    //set up sockaddr_un structure for domain socket
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    //connect to server socket
    while(connect(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) == -1){
        if(errno == ENOENT){
            sleep(1); //wait for server so its available
        } 
        else{
            perror("connect error");
            exit(-1);
        }
    }

    //tells server server that its ready
    write(sockfd, "Ready", 6);

    //main loop for talking with the server
    while(runing){
        int ret = read(sockfd, buffer, BUF_SIZE);
        if(ret <= 0){
            if(ret < 0){
                perror("read");
            }
            break;
        }

        buffer[ret] = '\0';
        if(strncmp(buffer, "Monitor", 7) == 0){
            while(runing){
                string stats;
                get_interface_stat(interface, stats); //gets the interface stats
                cout << stats << endl;

                if(stats.find("state:down") != string::npos){
                    write(sockfd, "Link Down", 10); //tells the server to set link up
                    set_if_up(interface.c_str()); //brings the interface up
                }

                write(sockfd, stats.c_str(), stats.size() + 1); //send stats to server
                sleep(1);
            }
        } 
        else if(strncmp(buffer, "Set Link Up", 11) == 0){ //for trying to bring interface up
            if(set_if_up(interface.c_str()) < 0){
                perror("set link up error");
            }
        } 
        else if(strncmp(buffer, "Shut Down", 9) == 0){
            break; //shuts down
        }
    }

    write(sockfd, "Done", 5);
    close(sockfd);
    return 0;
}
