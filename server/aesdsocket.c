#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aesdsocket.h"

bool should_close = false;
bool as_daemon = false;
static void signal_handler(int signo);

int main(int argc, char **argv) {
  openlog(NULL, 0, LOG_USER);

  // handle d flags
  int c;
  while ((c = getopt(argc, argv, "d")) != -1) {
    switch (c) {
    case 'd':
      as_daemon = true;
      break;
    default:
      abort();
    }
  }

  struct sigaction new_action;
  memset(&new_action, 0, sizeof(struct sigaction));
  new_action.sa_handler = signal_handler;

  /* Register signal_handler as our signal handler for SIGINT*/
  if (sigaction(SIGINT, &new_action, NULL) != 0) {
    ERROR_LOG("Cannot handle SIGINT");
    return -1;
  }

  /* Register signal_handler as our signal handler for SIGTERM*/
  if (sigaction(SIGTERM, &new_action, NULL) != 0) {
    ERROR_LOG("Cannot handle SIGTERM");
    return -1;
  }

  int sockfd; // listen on sock_fd, new connection on new_fd
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr; // connector's address information
  socklen_t sin_size;
  struct sigaction sa;
  int yes = 1;
  char s[INET6_ADDRSTRLEN];
  int rv;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // use my IP

  if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
    ERROR_LOG("getaddrinfo: %s", gai_strerror(rv));
    return 1;
  }

  DEBUG_LOG("Binding");

  // loop through all the results and bind to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      ERROR_LOG("server: socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      ERROR_LOG("setsockopt");
      exit(1);
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      ERROR_LOG("server: bind");
      continue;
    }

    break;
  }

  freeaddrinfo(servinfo); // all done with this structure

  if (p == NULL) {
    ERROR_LOG("server: failed to bind");
    exit(1);
  }

  if (as_daemon) {
    if (daemon(0, 0) < 0) {
      ERROR_LOG("daemon() failed");
      exit(1);
    }
  }

  DEBUG_LOG("Listening");

  if (listen(sockfd, BACKLOG) == -1) {
    ERROR_LOG("listen");
    exit(1);
  }

  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    ERROR_LOG("sigaction");
    exit(1);
  }

  DEBUG_LOG("server: waiting for connections...");

  while (!should_close) { // main accept() loop
    sin_size = sizeof their_addr;

    int new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

    if (new_fd == -1) {
      ERROR_LOG("accept");
      continue;
    }

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),
              s, sizeof s);
    DEBUG_LOG("Accept connection from %s", s);

    recv_to_file(new_fd);
    send_file(new_fd);

    close(new_fd); // parent doesn't need this
    DEBUG_LOG("Closed connection from %s", s);
  }

  DEBUG_LOG("Deleting file %s", FILEPATH);
  remove(FILEPATH);
  return 0;
}

void recv_to_file(int fd) {
  char *recv_buf = (char *)malloc(MAXDATASIZE);

  FILE *fptr = fopen(FILEPATH, "a+");
  if (fptr == NULL) {
    ERROR_LOG("Error opening file FILEPATH");
    exit(1);
  }

  int bytes_recv;
  do {
    if ((bytes_recv = recv(fd, recv_buf, MAXDATASIZE, 0)) == -1) {
      ERROR_LOG("recv");
      exit(0);
    }
    fwrite(recv_buf, bytes_recv, 1, fptr);
  } while (bytes_recv != 0);

  DEBUG_LOG("receive from client: %s", recv_buf);
  fwrite(recv_buf, bytes_recv, 1, fptr);

  fclose(fptr);
  free(recv_buf);
}

void send_file(int fd) {
  FILE *fptr = fopen(FILEPATH, "r");
  if (fptr == NULL) {
    ERROR_LOG("Error opening file FILEPATH");
    exit(1);
  }

  char sendbuffer[124];
  int b;
  while ((b = fread(sendbuffer, 1, sizeof(sendbuffer), fptr)) > 0) {
    DEBUG_LOG("Sending to client: %s", sendbuffer);
    send(fd, sendbuffer, b, 0);
  }
  fclose(fptr);
}

void sigchld_handler(int s) {
  (void) s; //unused variable
  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;

  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

static void signal_handler(int signo) {
  if (signo == SIGINT || signo == SIGTERM) {
    DEBUG_LOG("Shutdown Requested");
    should_close = true;
  }
}
