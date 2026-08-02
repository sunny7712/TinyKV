#include "client.h"
#include "resp.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8379
#define MAX_CLIENTS 10
#define READBUF_SIZE 4096

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
    return fd;
}

static int add_client(int cfd, struct pollfd *pfds, client_t *clients,
                      nfds_t *nfds) {
    if (*nfds >= MAX_CLIENTS + 1) {
        return -1;
    }
    if (set_nonblocking(cfd) < 0) {
        perror("client non blocking");
        close(cfd);
        return -1;
    }
    pfds[*nfds].fd = cfd;
    pfds[*nfds].events = POLLIN;
    pfds[*nfds].revents = 0;

    client_init(cfd, &clients[*nfds]);

    (*nfds)++;
    return 0;
}

static void close_client(int idx, struct pollfd *pfds, client_t *clients,
                         nfds_t *nfds) {
    client_free(&clients[idx]);

    int last = (*nfds) - 1;
    if (idx != last) {
        clients[idx] = clients[last];
        pfds[idx] = pfds[last];
    }
    (*nfds)--;
}

static void update_events(client_t *client, struct pollfd *pfd) {
    pfd->events = POLLIN;
    if (client->out_sent < client->out_queued) {
        pfd->events |= POLLOUT;
    }
}

int main() {
    signal(SIGPIPE, SIG_IGN);

    int server_fd = create_server();
    if (server_fd < 0) {
        perror("error creating server");
        return 1;
    }

    struct pollfd pfds[MAX_CLIENTS + 1];
    client_t clients[MAX_CLIENTS + 1];
    nfds_t nfds = 1;

    pfds[0].fd = server_fd;
    pfds[0].events = POLLIN;
    pfds[0].revents = 0;

    printf("Server listening on port %d\n", PORT);

    for (;;) {
        int rc = poll(pfds, nfds, -1);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            break;
        }

        // New connections
        if (pfds[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);
            int cfd = accept(server_fd, (struct sockaddr *)&client_addr,
                             &client_addr_len);
            if (cfd < 0) {
                perror("accept");
                break;
            }

            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
            printf("Accepted %s:%d (fd=%d)\n", ip, ntohs(client_addr.sin_port),
                   cfd);

            if (add_client(cfd, pfds, clients, &nfds)) {
                perror("error adding client");
                close(cfd);
            }
        }

        // Existing connections
        for (long unsigned int i = 1; i < nfds;) {
            int re = pfds[i].revents;

            if (re & (POLLERR | POLLHUP |
                      POLLNVAL)) { // POLLHUP is peer disconnected. There may be
                                   // more data to read. TODO
                printf("Client disconnected/error fd=%d", pfds[i].fd);
                close_client(i, pfds, clients, &nfds);
                continue;
            }

            int closed = 0;

            // Read data
            if (re & POLLIN) {
                char buf[READBUF_SIZE];
                for (;;) {
                    ssize_t n = recv(pfds[i].fd, buf, sizeof(buf), 0);
                    if (n > 0) {
                        if (append_inbuf(&clients[i], buf, (size_t)n) < 0) {
                            printf("Client fd=%d input buffer full. Closing "
                                   "client\n",
                                   pfds[i].fd);
                            close_client(i, pfds, clients, &nfds);
                            closed = 1;
                            break;
                        }

                        parse_status_t parse_status = parse_inbuf(&clients[i]);
                        if (parse_status == PARSE_ERR ||
                            compact_inbuf(&clients[i]) < 0) {
                            printf("Client fd=%d parse error. Closing client\n",
                                   pfds[i].fd);
                            close_client(i, pfds, clients, &nfds);
                            closed = 1;
                            break;
                        }
                    } else if (n == 0) {
                        // peer closed cleanly. EOF
                        printf("Client close fd=%d", pfds[i].fd);
                        close_client(i, pfds, clients, &nfds);
                        closed = 1;
                        break;
                    } else {
                        if (errno == EAGAIN ||
                            errno ==
                                EWOULDBLOCK) { // AGAIN and EWOULDBLOCK are
                                               // basically two names for the
                                               // same condition on many systems
                            break;
                        }
                        perror("recv");
                        close_client(i, pfds, clients, &nfds);
                        closed = 1;
                        break;
                    }
                }
            }

            if (closed) {
                continue;
            }

            // Write data
            if (re & POLLOUT) {
                while (clients[i].out_sent < clients[i].out_queued) {
                    ssize_t n = send(
                        pfds[i].fd, clients[i].outbuf + clients[i].out_sent,
                        clients[i].out_queued - clients[i].out_sent, 0);
                    if (n > 0) {
                        clients[i].out_sent += (size_t)n;
                    } else if (n < 0 &&
                               (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        break;
                    } else {
                        perror("send");
                        close_client(i, pfds, clients, &nfds);
                        closed = 1;
                        break;
                    }
                }

                if (closed) {
                    continue;
                }

                if (clients[i].out_queued == clients[i].out_sent) {
                    clients[i].out_queued = 0;
                    clients[i].out_sent = 0;
                }
            }

            if (i < nfds) {
                update_events(&clients[i], &pfds[i]);
                i++;
            }
        }

        for (long unsigned int i = 0; i < nfds; i++) {
            pfds[i].revents = 0;
        }
    }

    return 0;
}
