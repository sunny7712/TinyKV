#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8379

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) { 
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return -1;
    }
    return 0;
}

static int create_listener(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0); // address family, socket type and protocol. TCP is the default protocol for SOCK_STREAM socket type
    if (fd < 0) {
        perror("socket creation failed");
        return -1;
    }
    int yes = 1; // optvalue to set socket option `SO_REUSEADDR`
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) { // SO_REUSEADDR allows binding to a port which is in TIME_WAIT.
        perror("setsockopt(SO_REUSEADDR)");
        close(fd);
        return -1;
    }

    if(set_nonblocking(fd) < 0) {
        perror("set_nonblocking(listener)");
        close(fd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; // Any network interface on the machine
    // The port is a part of TCP header and sent over the network. Hence we must convert this to network byte order (Big endian)
    addr.sin_port = htons(PORT);

    if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("error binding socket");
        close(fd);
        return -1;
    }

    if(listen(fd, SOMAXCONN) < 0) {
        perror("error listening");
        close(fd);
        return -1;
    }
    return 0;
}

int main() {

}