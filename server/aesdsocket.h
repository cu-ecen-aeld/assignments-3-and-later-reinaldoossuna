#ifndef AESDSOCKET_H_
#define AESDSOCKET_H_

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#include "queue.h"

// Optional: use these functions to add debug or error prints to your
// application
#ifdef DEV
#define DEBUG_LOG(msg, ...) printf("server: " msg "\n", ##__VA_ARGS__)
#define ERROR_LOG(msg, ...) printf("server ERROR: " msg "\n", ##__VA_ARGS__)
#else
#ifdef DEBUG
#define DEBUG_LOG(msg, ...) syslog(LOG_INFO, "server: " msg "\n", ##__VA_ARGS__)
#else
#define DEBUG_LOG(msg, ...)
#endif
#define ERROR_LOG(msg, ...)                                                    \
  syslog(LOG_ERR, "server: ERROR: " msg "\n", ##__VA_ARGS__)
#endif

#define PORT "9000" // the port users will be connecting to

#define BACKLOG 10 // how many pending connections queue will hold

#define MAXDATASIZE 1024
#define FILEPATH "/var/tmp/aesdsocketdata"

#define UNUSED(x) (void)(x)

void recv_to_file(int fd);
void send_file(int fd);
void sigchld_handler(int s);
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa);
int set_signal_handler(void);
int get_listener(void);
bool handle_flags(int argc, char **argv);
int reap_dead_processes(void);
bool is_error(int val);

typedef struct {
  bool is_finished;
  int fd;
  pthread_t thread;
} thread_info_t;

struct list_threads {
    thread_info_t* tinfo;
    LIST_ENTRY(list_threads) entries;
};

LIST_HEAD(list_head, list_threads) head;

void clean_threads(struct list_head* head, bool wait);

#endif // AESDSOCKET_H_
