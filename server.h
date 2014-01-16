#ifndef SERVER_H
#define SERVER_H

#define BACKLOG     10
#define SEND_BUFLEN 0x40

#define UPTIME  "uptime"
#define LOAD    "load"
#define EXIT    "exit"

#define where() printf("%s: %d\n", __func__, __LINE__)

typedef enum {CMD_NONE = 0, CMD_UPTIME, CMD_LOAD, CMD_NUMBER, CMD_EXIT} command;

typedef struct thread_args {
  int sock;
  int *num_conn;
  pthread_mutex_t *mutex;
} thread_args;


int create_server_socket(char* port);
void* handle_client(void *args_ptr);

#endif
