#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 9090
#define MAX_CLIENTS 10
#define BUFFER_SIZE 4096
#define BACKLOG 5

typedef enum { STATE_NEW, STATE_CONNECTED, STATE_DISCONNECTED } state_e;

typedef struct {
  int sockfd;
  state_e state;
  int start;
  size_t offset;
  size_t remaining;
} clientstate_t;

clientstate_t clients[MAX_CLIENTS];

void init_clients(size_t moby_len) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    clients[i].sockfd = -1;
    clients[i].state = STATE_NEW;
    clients[i].start = 0;
    clients[i].offset = 0;
    clients[i].remaining = moby_len;
  }
}

int set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    return -1;
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    return -1;
  return 0;
}

int find_free_slot() {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].sockfd == -1) {
      return i;
    }
  }
  return -1;
}

int find_slot_by_id(int sockfd) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].sockfd == sockfd) {
      return i;
    }
  }
  return -1;
}

char *read_file(size_t *out_len) {
  int fd = open("2701.txt.utf-8", O_RDONLY);
  if (fd < 0) {
    perror("open");
    return NULL;
  }

  size_t buf_size = 8;
  char *moby = malloc(sizeof(char) * buf_size);
  if (!moby) {
    perror("malloc");
    return NULL;
  }

  int bytes_read = 0;
  while (1) {
    if (*out_len + 1 >= buf_size) {
      buf_size *= 2;
      char *new_buf = realloc(moby, buf_size);
      if (!new_buf) {
        perror("realloc");
        free(moby);
        close(fd);
        return NULL;
      }
      moby = new_buf;
    }
    int n = read(fd, moby + *out_len, buf_size - *out_len);
    if (n < 0) {
      perror("read");
      free(moby);
      close(fd);
      return NULL;
    } else if (n == 0) {
      break; // EOF
    }
    *out_len += n;
  }
  moby[*out_len] = '\0'; // Null-terminate the string
  close(fd);
  return moby;
}

int start_multi_server(char *moby, size_t moby_len) {
  int listen_fd, nfds, free_slot;
  struct sockaddr_in server_addr, client_addr = {0};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);

  socklen_t client_len = sizeof(client_addr);
  fd_set read_fds, write_fds, except_fds;
  init_clients(moby_len);

  listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd == -1) {
    perror("socket");
    return -1;
  }

  if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    perror("bind");
    close(listen_fd);
    return -1;
  }

  if (listen(listen_fd, BACKLOG) == -1) {
    perror("listen");
    close(listen_fd);
    return -1;
  }

  if (set_nonblocking(listen_fd) == -1) {
    perror("fcntl");
    close(listen_fd);
    return -1;
  }

  printf("Listening on port %d\n", PORT);

  while (1) {
    // Zero
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&except_fds);

    // Make sure listen fd is available
    FD_SET(listen_fd, &read_fds);
    nfds = listen_fd + 1;

    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (clients[i].sockfd != -1) {
        FD_SET(clients[i].sockfd, &read_fds);
        if (clients[i].remaining > 0) {
          FD_SET(clients[i].sockfd, &write_fds);
        }
        if (clients[i].sockfd >= nfds) {
          nfds = clients[i].sockfd + 1;
        }
      }
    }

    if (select(nfds, &read_fds, &write_fds, &except_fds, NULL) == -1) {
      perror("select");
      break;
    }

    if (FD_ISSET(listen_fd, &read_fds)) {
      for (;;) {
        client_len = sizeof(client_addr);
        int client_sock =
            accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock == -1) {
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break; // No more incoming connections
          }
          perror("accept");
          break;
        }

        int snd = 1;
        setsockopt(client_sock, SOL_SOCKET, SO_SNDBUF, &snd, sizeof(snd));
        if (set_nonblocking(client_sock) == -1) {
          perror("fcntl");
          close(client_sock);
          continue;
        }

        printf("new connection from %s:%d\n", inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        free_slot = find_free_slot();
        if (free_slot != -1) {
          clients[free_slot].sockfd = client_sock;
          clients[free_slot].state = STATE_CONNECTED;
          clients[free_slot].offset = 0;
          clients[free_slot].remaining = moby_len;
        } else {
          fprintf(stderr, "Server full, rejecting connection\n");
          close(client_sock);
        }
      }
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (clients[i].sockfd == -1) {
        continue;
      }

      if (FD_ISSET(clients[i].sockfd, &read_fds)) {
        char tmp[BUFFER_SIZE];
        ssize_t r = read(clients[i].sockfd, tmp, sizeof(tmp));
        if (r == 0) {
          close(clients[i].sockfd);
          clients[i].sockfd = -1;
          clients[i].state = STATE_DISCONNECTED;
          continue;
        }
        if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
          perror("read");
          close(clients[i].sockfd);
          clients[i].sockfd = -1;
          clients[i].state = STATE_DISCONNECTED;
          continue;
        }

        if ((r == 3 && memcmp(tmp, "run", 3) == 0) ||
            (r == 4 && memcmp(tmp, "run\n", 4) == 0) ||
            (r == 5 && memcmp(tmp, "run\r\n", 5) == 0)) {
          clients[i].start = 1;
          clients[i].offset = 0;
          clients[i].remaining = moby_len;
        }
      }

      if (FD_ISSET(clients[i].sockfd, &write_fds)) {
        while (clients[i].start == 1 && clients[i].remaining > 0) {
          size_t to_send = clients[i].remaining;
          if (to_send > BUFFER_SIZE)
            to_send = BUFFER_SIZE;
          ssize_t n =
              write(clients[i].sockfd, moby + clients[i].offset, to_send);
          if (n > 0) {
            clients[i].offset += n;
            clients[i].remaining -= n;
          } else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break; // Can't write more right now
          } else {
            close(clients[i].sockfd);
            clients[i].sockfd = -1;
            clients[i].state = STATE_DISCONNECTED;
            break;
          }
        }
        if (clients[i].sockfd != -1 && clients[i].remaining == 0) {
          shutdown(clients[i].sockfd, SHUT_WR);
        }
      }
    }
  }
  close(listen_fd);
  return 0;
}

int start_poll_server(char *moby, size_t moby_len) {
  int listen_fd, conn_fd, free_slot;
  struct sockaddr_in server_addr, client_addr = {0};
  socklen_t client_len = sizeof(client_addr);

  struct pollfd fds[MAX_CLIENTS + 1];
  int nfds = 1;
  int opt = 1;

  if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    return -1;
  }

  if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    perror("setsockopt");
    close(listen_fd);
    return -1;
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);

  if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("bind");
    close(listen_fd);
    return -1;
  }

  if (listen(listen_fd, BACKLOG) < 0) {
    perror("listen");
    close(listen_fd);
    return -1;
  }

  printf("Listening on port %d\n", PORT);
  memset(fds, 0, sizeof(fds));
  fds[0].fd = listen_fd;
  fds[0].events = POLLIN;
  nfds = 1;

  while (1) {
    int ii = 1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (clients[i].sockfd != -1) {
        fds[ii].fd = clients[i].sockfd;
        fds[ii].events = POLLIN;
        ii++;
      }
    }

    int n_events = poll(fds, nfds, -1);
    if (n_events == -1) {
      perror("poll");
      return -1;
    }

    if (fds[0].revents & POLLIN) {
      if ((conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr,
                            &client_len)) == -1) {
        perror("accept");
        continue;
      }
      printf("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr),
             ntohs(client_addr.sin_port));

      free_slot = find_free_slot();
      if (free_slot == -1) {
        fprintf(stderr, "Server full, rejecting connection\n");
        close(conn_fd);
      } else {
        clients[free_slot].sockfd = conn_fd;
        clients[free_slot].state = STATE_CONNECTED;
        clients[free_slot].offset = 0;
        clients[free_slot].remaining = moby_len;
        nfds++;
      }
      n_events--;
    }
    for (int i = 0; i <= nfds && n_events > 0; i++) {
      if (fds[i].revents & POLLIN) {
        int slot = find_slot_by_id(fds[i].fd);
        if (slot == -1) {
          continue;
        }
        char tmp[BUFFER_SIZE];
        ssize_t r = read(clients[slot].sockfd, tmp, sizeof(tmp));
        if (r == 0) {
          close(clients[slot].sockfd);
          clients[slot].sockfd = -1;
          clients[slot].state = STATE_DISCONNECTED;
          nfds--;
          continue;
        }
        if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
          perror("read");
          close(clients[slot].sockfd);
          clients[slot].sockfd = -1;
          clients[slot].state = STATE_DISCONNECTED;
          nfds--;
          continue;
        }

        if ((r == 3 && memcmp(tmp, "run", 3) == 0) ||
            (r == 4 && memcmp(tmp, "run\n", 4) == 0) ||
            (r == 5 && memcmp(tmp, "run\r\n", 5) == 0)) {
          clients[slot].start = 1;
          clients[slot].offset = 0;
          clients[slot].remaining = moby_len;
        }
        n_events--;
      }
    }
  }
}

int start_server(void) {
  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1)
    return -1;

  struct sockaddr_in server_info = {0};
  server_info.sin_family = AF_INET;
  server_info.sin_addr.s_addr = INADDR_ANY;
  server_info.sin_port = htons(PORT);

  if (bind(sfd, (struct sockaddr *)&server_info, sizeof(server_info)) == -1) {
    perror("bind");
    return -1;
  }

  if (listen(sfd, BACKLOG) == -1) {
    perror("listen");
    return 0;
  }

  while (1) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_sock =
        accept(sfd, (struct sockaddr *)&client_addr, &client_addr_len);

    if (client_sock == -1) {
      perror("accept");
      continue;
    }
    printf("Client connected\n");
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = read(client_sock, buffer, BUFFER_SIZE)) > 0) {
      write(client_sock, buffer, bytes_read);
    }

    printf("Client disconnected\n");
    close(client_sock);
  }
  close(sfd);
  return 0;
}

int main() {
  size_t moby_len = 0;
  char *moby = read_file(&moby_len);
  if (!moby) {
    perror("Failed to read file");
    return -1;
  }

  int s = start_multi_server(moby, moby_len);
  free(moby);
  return s;
}
