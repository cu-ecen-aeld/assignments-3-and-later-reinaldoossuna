#ifndef AESDSOCKET_H_
#define AESDSOCKET_H_

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>
#include <stdbool.h>

// Optional: use these functions to add debug or error prints to your
// application
#ifdef SERVER
#ifdef DEBUG
#define DEBUG_LOG(msg, ...) syslog(LOG_INFO, "server: " msg "\n", ##__VA_ARGS__)
#else
#define DEBUG_LOG(msg, ...)
#endif
#define ERROR_LOG(msg, ...)                                                    \
  syslog(LOG_ERR, "server: ERROR: " msg "\n", ##__VA_ARGS__)
#else
#define DEBUG_LOG(msg, ...) printf("server: " msg "\n", ##__VA_ARGS__)
#define ERROR_LOG(msg, ...) printf("server ERROR: " msg "\n", ##__VA_ARGS__)
#endif

#define PORT "9000" // the port users will be connecting to

#define BACKLOG 10 // how many pending connections queue will hold

#define MAXDATASIZE 1024
#define FILEPATH "/var/tmp/aesdsocketdata"

void recv_to_file(int fd);
void send_file(int fd);
void sigchld_handler(int s);
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa);

#endif // AESDSOCKET_H_
