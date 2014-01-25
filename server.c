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

const char *cmd_str[] = {"", "uptime", "load", "", "exit"};
pthread_mutex_t mutex;
int num_conn;
thread_list *list;

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

void cleanup_routine(void* ptr) {
  pthread_mutex_lock(&mutex);
  num_conn--;
  pthread_mutex_unlock(&mutex);
  close(((thread_arg*) ptr)->sock);
  ((thread_arg*) ptr)->heartbeat = 0;
  printf("Closed connection to a client\n");
}

void* check_liveness(void* ptr) {
  thread_list **pp, *p;
  int now;

  while (1) {
    now = time(NULL);
    // printf("now %i\n", now);
    pp = &list;

    // For each active connection, check last heartbeat
    while (*pp != NULL) {
      p = *pp;
      // printf("last heartbeat %i\n", p->arg->heartbeat);

      // Last heartbeat was more than TIMEOUT ago
      if (now > p->arg->heartbeat + TIMEOUT) {
        if (p->arg->heartbeat) {
          printf("A connection has been idle for too long\n");
        }
        pthread_cancel(p->thread_id);
        pthread_join(p->thread_id, NULL);
        *pp = p->next;
        free(p->arg);
        free(p);
        
      } else {
        pp = &(p->next);
      }
    }
    sleep(1);
  }
  return NULL;
}

void send_int32(int sock, int val) {
  uint32_t nint;
  //nint = htonl(val);
  nint = val;
  printf("sending integer %d\n", val);
  if (write(sock, &nint, sizeof(nint)) < sizeof(nint)) {
    pthread_exit(NULL);
  }
}

void* handle_client(void *ptr) {
  command cmd = CMD_NONE;
  unsigned int sum, next_pos, prev_err;
  int sock;
  char c;
  // char msg_buf[SEND_BUFLEN];
  // int msg_len;

  pthread_cleanup_push(cleanup_routine, ptr);

  sock = ((thread_arg*) ptr)->sock;

  if (pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL) != 0) {
    pthread_exit(NULL);
  }

  next_pos = 0;
  prev_err = 0;

  while (1) {
    if (recv(sock, &c, 1, 0) <= 0) {
      fprintf(stderr, "recv() failed with errno %d\n", errno);
      pthread_exit(NULL);
    }

    // Update heartbeat
    ((thread_arg*) ptr)->heartbeat = time(NULL);

new_cmd:
    if (prev_err > 2) {
      pthread_exit(NULL);
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
          send_int32(sock, -1);
        }
        break;
      case CMD_UPTIME:
      case CMD_LOAD:
      case CMD_EXIT:
        if (next_pos == strlen(cmd_str[cmd]) - 1 && c == cmd_str[cmd][next_pos]) {
          if (cmd == CMD_UPTIME) {
         
            send_int32(sock, time(NULL)); 
            // msg_len = snprintf(msg_buf, SEND_BUFLEN, "%lu\n", time(NULL));
            // if (send(sock, msg_buf, msg_len, 0) < 0) {
            //   pthread_exit(NULL);
            // }

          } else if (cmd == CMD_LOAD) {
            pthread_cleanup_push(pthread_mutex_unlock, (void*) &mutex);
            pthread_mutex_lock(&mutex);
            send_int32(sock, num_conn);
            // msg_len = snprintf(msg_buf, SEND_BUFLEN, "%d\n", num_conn);
            // if (send(sock, msg_buf, msg_len, 0) < 0) {
            //   pthread_exit(NULL);
            // }
            pthread_cleanup_pop(1);

          } else {
            send_int32(sock, 0);
            // send(sock, "0\n", 2, 0);
            pthread_exit(NULL);
          }

          prev_err = 0;
          cmd = CMD_NONE;

        } else if (c == cmd_str[cmd][next_pos]) {
          next_pos++;

        } else {
          prev_err++;
          send_int32(sock, -1);
          // if (send(sock, "-1\n", 3, 0) < 0) {
          //   pthread_exit(NULL);
          // }
          cmd = CMD_NONE;
          goto new_cmd;
        }
        break;
      case CMD_NUMBER:
        if (is_digit(c)) {
          sum += (c - 48);

        } else {
          send_int32(sock, sum);
          // msg_len = snprintf(msg_buf, SEND_BUFLEN, "%u\n", sum);
          // if (send(sock, msg_buf, msg_len, 0) < 0) {
          //   pthread_exit(NULL);
          // }

          prev_err = 0;
          cmd = CMD_NONE;
          goto new_cmd;
        }
        break;
    }
    if (prev_err > 2) {
      pthread_exit(NULL);
    }
  }

  // Should not reach here
  pthread_cleanup_pop(1);
  return NULL;
}

int main(int argc, char *argv[]) {
  int rc;
  int serv_sock, clnt_sock;
  int max_conn;
  struct sockaddr_storage their_addr;
  socklen_t size;
  pthread_t chk_live;
  thread_list *l;

  if (argc < 3) {
    fprintf(stderr, "Usage:\n\t%s MAX_NUM_CLIENTS PORT_NUMBER\n", argv[0]);
    exit(1);
  }

  max_conn = atoi(argv[1]);
  num_conn = 0;

  // Create a thread to check liveness of client connection
  list = NULL;
  pthread_create(&chk_live, NULL, check_liveness, NULL);

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
      l = malloc(sizeof(thread_list));
      l->next = list;
      l->arg = malloc(sizeof(thread_arg));
      l->arg->sock = clnt_sock;
      l->arg->heartbeat = time(NULL);
      rc = pthread_create(&(l->thread_id), NULL, handle_client, l->arg);
      printf("Accepted a connection\n");
      if (rc != 0) {
        fprintf(stderr, "pthread_created() failed");
        exit(1);
      }
      list = l;
      num_conn++;
    }
    pthread_mutex_unlock(&mutex);
  }

  pthread_mutex_destroy(&mutex);
  return 0;
}
