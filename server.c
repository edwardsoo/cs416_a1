/*
  Edward Soo 71680094
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define BACKLOG 10

typedef struct thread_args {
  int sock;
} thread_args;

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

void* handle_client(void *args_ptr) {
  thread_args *args = (thread_args*) args_ptr;
  printf("handle client\n");
  close(args->sock);
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

  if (argc < 3) {
    fprintf(stderr, "Usage:\n\t%s MAX_NUM_CLIENTS PORT_NUMBER\n", argv[0]);
    exit(1);
  }

  max_conn = atoi(argv[1]);
  num_conn = 0;

  size = sizeof(their_addr);
  serv_sock = create_server_socket(argv[2]);

  printf("Listening for connection...\n");
  while (1) {
    clnt_sock = accept(serv_sock, (struct sockaddr*) &their_addr, &size);
    if (clnt_sock < 0) {
      continue;
    }

    // Max number of connections reached, terminate this connection
    if (num_conn >= max_conn) {
      close(clnt_sock);


    // Create child thread to handle client
    } else {
      thread_args = malloc(sizeof(thread_args));
      thread_args->sock = clnt_sock;
      rc = pthread_create(&thread_id, NULL, handle_client, thread_args);
      if (rc != 0) {
        fprintf(stderr, "pthread_created() failed");
        exit(1);
      }
    }
 }

  return 0;
}
