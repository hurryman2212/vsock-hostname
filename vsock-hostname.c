#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <sys/epoll.h>
#include <sys/socket.h>

#include <linux/vm_sockets.h>

#ifndef NDEBUG
#define DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("usage: %s [PORT]\n", argv[0]);
    exit(EXIT_SUCCESS);
  }
  int port = atoi(argv[1]);

  int server_sock;
  if ((server_sock = socket(AF_VSOCK, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  int opt = 1;
  if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }
  if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_vm server_addr = {0};
  server_addr.svm_family = AF_VSOCK;
  server_addr.svm_cid = VMADDR_CID_HOST;
  server_addr.svm_port = port;
  if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
    perror("bind");
    exit(EXIT_FAILURE);
  }

  if (listen(server_sock, 0)) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  int nr_event;
  struct epoll_event ev_set, *events;
  if ((events = (struct epoll_event *)malloc(
           UINT16_MAX * sizeof(struct epoll_event))) == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  int epoll_fd;
  if ((epoll_fd = epoll_create1(0)) < 0) {
    perror("epoll_create1");
    exit(EXIT_FAILURE);
  }
  ev_set.events = EPOLLIN | EPOLLRDHUP;
  ev_set.data.fd = server_sock;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_sock, &ev_set);

  char host_hostname[HOST_NAME_MAX];
  gethostname(host_hostname, HOST_NAME_MAX);
  unsigned int len_send = strlen(host_hostname) + 1;
  ev_set.events = EPOLLRDHUP;
  struct sockaddr_vm client_addr;
  socklen_t client_addr_size;
  while (1) {
    nr_event = epoll_wait(epoll_fd, events, UINT16_MAX, -1);
    for (int i = 0; i < nr_event; ++i) {
      int fd = events[i].data.fd;
      if (fd == server_sock) {
        client_addr_size = sizeof(client_addr);
        int client_sock =
            accept(fd, (struct sockaddr *)&client_addr, &client_addr_size);

        DEBUG_PRINT("[guest_cid: %u / guest_port: %u] accept()\n",
                    client_addr.svm_cid, client_addr.svm_port);

        /* Send hostname length */
        send(client_sock, &len_send, sizeof(len_send), MSG_NOSIGNAL | MSG_MORE);
        /* Send hostname */
        send(client_sock, host_hostname, len_send, MSG_NOSIGNAL);

        ev_set.data.fd = client_sock;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_sock, &ev_set);
      } else {
        uint32_t event = events[i].events;

        client_addr_size = sizeof(client_addr);
        if (getpeername(fd, (struct sockaddr *)&client_addr,
                        &client_addr_size)) {
          perror("getpeername");
          exit(EXIT_FAILURE);
        }
        if (event & EPOLLRDHUP) {
          DEBUG_PRINT("[cid: %u / port: %u] EPOLLRDHUP -> Closing...\n",
                      client_addr.svm_cid, client_addr.svm_port);
          if (close(fd)) {
            perror("close");
            exit(EXIT_FAILURE);
          }
        } else {
          assert(0);
        }
      }
    }
  }

  return 0;
}