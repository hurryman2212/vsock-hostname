#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/ioctl.h>
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

  int client_sock;
  if ((client_sock = socket(AF_VSOCK, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  /**
   * Get CID through ioctl() on /dev/vsock
   *
   * getsockname() will not return valid CID without explicit binding CID #!
   */
  int vsock_fd = open("/dev/vsock", O_RDONLY);
  if (vsock_fd < 0) {
    perror("open");
    exit(EXIT_FAILURE);
  }
  unsigned int self_cid;
  if (ioctl(vsock_fd, IOCTL_VM_SOCKETS_GET_LOCAL_CID, &self_cid)) {
    perror("ioctl");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_vm server_addr = {0};
  server_addr.svm_family = AF_VSOCK;
  server_addr.svm_cid = VMADDR_CID_HOST;
  server_addr.svm_port = port;
  while (1) {
    if (connect(client_sock, (struct sockaddr *)&server_addr,
                sizeof(server_addr))) {
      perror("connect");
      exit(EXIT_FAILURE);
    }

    struct sockaddr_vm client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    if (getsockname(client_sock, (struct sockaddr *)&client_addr,
                    &client_addr_len)) {
      perror("getsockname");
      exit(EXIT_FAILURE);
    }
    unsigned int len_recv;
    char recv_hostname[HOST_NAME_MAX];
    /* Get hostname length */
    recv(client_sock, &len_recv, sizeof(len_recv), MSG_WAITALL);
    /* Get hostname */
    recv(client_sock, recv_hostname, len_recv, MSG_WAITALL);

    DEBUG_PRINT(
        "[cid = %u, src_port = %u] connect: Connected! (host_hostname = %s)\n",
        self_cid, client_addr.svm_port, recv_hostname);

    char dummy;
    ssize_t ret = recv(client_sock, &dummy, sizeof(dummy), 0);
    assert(ret <= 0);
    DEBUG_PRINT("recv: returned %ld: %s\n", ret, strerror(errno));

    if (close(client_sock)) {
      perror("close");
      exit(EXIT_FAILURE);
    }
  }

  return 0;
}
