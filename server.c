/*
  Edward Soo 71680094
*/

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "server.h"

int create_server_socket(char* port) {
  int sock;
  int rc;
  int yes = 1;
  struct addrinfo hints, *res, *p;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  rc = getaddrinfo(NULL, port, &hints, &res);
  if (rc != 0) {
    fprintf(stderr, "getaddrinfo() failed\n");
    exit(1);
  }

  for (p = res; p != NULL; p = p->ai_next) {
    sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sock < 0) {
      continue;
    }

    rc = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    if (rc != 0) {
      continue;
    }

    rc = bind(sock, p->ai_addr, p->ai_addrlen);
    if (rc != 0) {
      close(sock);
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "failed to bind to port %s\n", port);
    exit(1);
  }

  freeaddrinfo(res);

  rc = listen(sock, BACKLOG);
  if (rc != 0) {
    fprintf(stderr, "listen() failed\n");
    exit(1);
  }

  return sock;
}

int is_digit(char c) {
  return (c >= 48 && c <= 57);
}

void* handle_client(void *args_ptr) {
  command cmd = CMD_NONE;
  unsigned int sum, next_pos, prev_err;
  int msg_len, sock, *num_conn;
  char c, msg_buf[SEND_BUFLEN];
  pthread_mutex_t *mutex;

  sock = ((thread_args*) args_ptr)->sock;
  num_conn = ((thread_args*) args_ptr)->num_conn;
  mutex = ((thread_args*) args_ptr)->mutex;

  next_pos = 0;
  prev_err = 0;

  while (1) {
    if (recv(sock, &c, 1, 0) == -1) {
      fprintf(stderr, "rev() failed with errno %d\n", errno);
      goto done;
    }

new_cmd:
    if (prev_err >= 2) {
      goto done;
    }

    switch (cmd) {
      case CMD_NONE:
        if (c == UPTIME[0]) {
          cmd = CMD_UPTIME;
          next_pos = 1;

        } else if (c == LOAD[0]) {
          cmd = CMD_LOAD;
          next_pos = 1;

        } else if (c == EXIT[0]) {
          cmd = CMD_EXIT;
          next_pos = 1;

        } else if (is_digit(c)) {
          cmd = CMD_NUMBER;
          sum = (c - 48);

        } else {
          prev_err++;
          if (send(sock, "-1\n", 3, 0) < 0) {
            goto done;
          }
        }
        break;
      case CMD_UPTIME:
        if (next_pos == strlen(UPTIME) - 1 && c == UPTIME[next_pos]) {
          msg_len = snprintf(msg_buf, SEND_BUFLEN, "%lu\n", time(NULL));
          if (send(sock, msg_buf, msg_len, 0) < 0) {
            goto done;
          }
          prev_err = 0;
          cmd = CMD_NONE;

        } else if (c == UPTIME[next_pos]) {
          next_pos++;

        } else {
          prev_err++;
          if (send(sock, "-1\n", 3, 0) < 0) {
            goto done;
          }
          cmd = CMD_NONE;
          goto new_cmd;
        }
        break;
      case CMD_LOAD:
        if (next_pos == strlen(LOAD) - 1 && c == LOAD[next_pos]) {
          pthread_mutex_lock(mutex);
          msg_len = snprintf(msg_buf, SEND_BUFLEN, "%d\n", *num_conn);
          pthread_mutex_unlock(mutex);
          if (send(sock, msg_buf, msg_len, 0) < 0) {
            goto done;
          }
          prev_err = 0;
          cmd = CMD_NONE;

        } else if (c == LOAD[next_pos]) {
          next_pos++;

        } else {
          prev_err++;
          if (send(sock, "-1\n", 3, 0) < 0) {
            goto done;
          }
          cmd = CMD_NONE;
          goto new_cmd;
        }
        break;
      case CMD_NUMBER:
        if (is_digit(c)) {
          sum += (c - 48);

        } else if (c == UPTIME[0] || c == LOAD[0] || c == EXIT[0]) {
          msg_len = snprintf(msg_buf, SEND_BUFLEN, "%u\n", sum);
          if (send(sock, msg_buf, msg_len, 0) < 0) {
            goto done;
          }
          prev_err = 0;
          cmd = CMD_NONE;
          goto new_cmd;

          // DEAD CODE
          if (c == UPTIME[0])
            cmd = CMD_UPTIME;
          else if (c == LOAD[0])
            cmd = CMD_LOAD;
          else
            cmd = CMD_EXIT;

          next_pos = 1;

        } else {
          prev_err++;
          cmd = CMD_NONE;
          msg_len = snprintf(msg_buf, SEND_BUFLEN, "%u\n", sum);
          if (send(sock, msg_buf, msg_len, 0) < 0) {
            goto done;
          }
          if (send(sock, "-1\n", 3, 0) < 0) {
            goto done;
          }
          cmd = CMD_NONE;
          goto new_cmd;
        }
        break;
      case CMD_EXIT:
        if (next_pos == strlen(EXIT) - 1 && c == EXIT[next_pos]) {
          send(sock, "0\n", 2, 0);
          goto done;

        } else if (c == EXIT[next_pos]) {
          next_pos++;

        } else {
          prev_err++;
          if (send(sock, "-1\n", 3, 0) < 0) {
            goto done;
          }
          cmd = CMD_NONE;
          goto new_cmd;
        }
        break;

    }

  }

  done:
  printf("closing connection to a client\n");
  pthread_mutex_lock(mutex);
  (*num_conn)--;
  pthread_mutex_unlock(mutex);
  free(args_ptr);
  close(sock);
  return NULL;
}

int main(int argc, char *argv[]) {
  int rc;
  int serv_sock, clnt_sock;
  int num_conn, max_conn;
  struct sockaddr_storage their_addr;
  socklen_t size;
  pthread_t thread_id;
  thread_args *thread_args;
  pthread_mutex_t mutex;

  if (argc < 3) {
    fprintf(stderr, "Usage:\n\t%s MAX_NUM_CLIENTS PORT_NUMBER\n", argv[0]);
    exit(1);
  }

  max_conn = atoi(argv[1]);
  num_conn = 0;

  // Init mutex for num_conn
  pthread_mutex_init(&mutex, NULL);

  // Create a listening socket
  serv_sock = create_server_socket(argv[2]);
  size = sizeof(their_addr);

  // Ignore SIGPIPE; does nothing when writing to broken pipe
  signal(SIGPIPE, SIG_IGN);

  printf("Listening for connection...\n");
  while (1) {
    clnt_sock = accept(serv_sock, (struct sockaddr*) &their_addr, &size);
    if (clnt_sock < 0) {
      continue;
    }

    // Max number of connections reached, terminate this connection
    pthread_mutex_lock(&mutex);
    if (num_conn >= max_conn) {
      printf("Maximum number of connection had been reached\n");
      close(clnt_sock);

    // Create child thread to handle client
    } else {
      thread_args = malloc(sizeof(thread_args));
      thread_args->sock = clnt_sock;
      thread_args->num_conn = &num_conn;
      thread_args->mutex = &mutex;
      rc = pthread_create(&thread_id, NULL, handle_client, thread_args);
      printf("Accepted a connection\n");
      if (rc != 0) {
        fprintf(stderr, "pthread_created() failed");
        exit(1);
      }
      num_conn++;
    }
    pthread_mutex_unlock(&mutex);
  }

  pthread_mutex_destroy(&mutex);
  return 0;
}
