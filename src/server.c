#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <poll.h>

#define PORT 8379
#define MAX_CLIENTS 10
#define OUTBUF_SIZE 8192
#define READBUF_SIZE 4096

typedef struct {
	int fd;
	char outbuf[OUTBUF_SIZE];
	size_t out_queued;
	size_t out_sent;

} client_t;

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

static int create_server(void) {
    int fd = socket(AF_INET, SOCK_STREAM,
                    0); // address family, socket type and protocol. TCP is the
                        // default protocol for SOCK_STREAM socket type
    if (fd < 0) {
        perror("socket creation failed");
        return -1;
    }
    int yes = 1; // optvalue to set socket option `SO_REUSEADDR`
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) <
        0) { // SO_REUSEADDR allows binding to a port which is in TIME_WAIT.
        perror("setsockopt(SO_REUSEADDR)");
        close(fd);
        return -1;
    }

    if (set_nonblocking(fd) < 0) {
        perror("set_nonblocking(listener)");
        close(fd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; // Any network interface on the machine
    // The port is a part of TCP header and sent over the network. Hence we must
    // convert this to network byte order (Big endian)
    addr.sin_port = htons(PORT);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("error binding socket");
        close(fd);
        return -1;
    }

    if (listen(fd, SOMAXCONN) < 0) {
        perror("error listening");
        close(fd);
        return -1;
    }
    return 0;
}

int main() {
    signal(SIGPIPE, SIG_IGN);
	
	int server = create_server();
	if(server < 0) {
		perror("error creating server");
		return 1;
	}

	struct pollfd pfds[MAX_CLIENTS + 1];
	client_t clients[MAX_CLIENTS + 1];
	size_t nfds = 1;

	pfds[0].fd = server;
	pfds[0].events = POLLIN;
	pfds[0].revents = 0;

	printf("Server listening on port %d\n", PORT);

	for (;;) {
		int rc = poll(pfds, nfds, -1);
		if (rc < 0) {
			
		}

	}


    return 0;
}