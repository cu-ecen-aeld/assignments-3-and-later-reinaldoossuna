#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aesdsocket.h"
bool should_close = false;
static void signal_handler(int signo);

int main(int argc, char **argv) {
  openlog(NULL, 0, LOG_USER);

  bool as_daemon = handle_flags(argc, argv);
  if (as_daemon) {
    if (is_error(daemon(0, 0))) {
      ERROR_LOG("daemon() failed");
      exit(1);
    }
  }

  if (is_error(set_signal_handler())) {
    ERROR_LOG("Not able to set signal handler");
    exit(1);
  }

  int sockfd = get_listener(); // listen on sock_fd, new connection on new_fd
  if (is_error(sockfd)) {
    ERROR_LOG("Failed to get listener");
    exit(1);
  }

  DEBUG_LOG("Listening");

  if (is_error(listen(sockfd, BACKLOG))) {
    ERROR_LOG("listen");
    exit(1);
  }

  if (is_error(reap_dead_processes())) {
    ERROR_LOG("Not able to reap dead processes");
    exit(1);
  }

  DEBUG_LOG("waiting for connections...");

  struct pollfd pfds = {.fd = sockfd, .events = POLLIN};

  while (!should_close) { // main accept() loop
    int poll_count = poll(&pfds, 1, 5000);
    if (is_error(poll_count)) {
      ERROR_LOG("poll");
      exit(1);
    }

    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size = sizeof their_addr;
    if (pfds.revents & POLLIN) {

      char s[INET6_ADDRSTRLEN];
      int new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

      if (is_error(new_fd)) {
        ERROR_LOG("accept");
        continue;
      }

      inet_ntop(their_addr.ss_family,
                get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
      DEBUG_LOG("Accept connection from %s", s);

      recv_to_file(new_fd);
      send_file(new_fd);

      close(new_fd); // parent doesn't need this
      DEBUG_LOG("Closed connection from %s", s);
    } else {
      DEBUG_LOG("No request. trying again");
    }
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
    DEBUG_LOG("receive from client: %s", recv_buf);
    fwrite(recv_buf, bytes_recv, 1, fptr);
  } while (bytes_recv == MAXDATASIZE);

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
  (void)s; // unused variable
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

int set_signal_handler(void) {

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

  return 1;
}

int get_listener(void) {

  struct addrinfo hints;
  struct addrinfo *servinfo;
  int yes = 1;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // use my IP

  int rv;
  if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
    ERROR_LOG("getaddrinfo: %s", gai_strerror(rv));
    return -1;
  }

  DEBUG_LOG("Binding");

  // loop through all the results and bind to the first we can
  int sockfd = -1;
  struct addrinfo *p;
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      ERROR_LOG("socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      ERROR_LOG("setsockopt");
      continue;
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      ERROR_LOG("bind");
      continue;
    }

    break;
  }

  freeaddrinfo(servinfo); // all done with this structure

  if (p == NULL) {
    ERROR_LOG("failed to bind");
    return -1;
  }

  return sockfd;
}

bool handle_flags(int argc, char **argv) {
  int c;
  bool as_daemon = false;
  while ((c = getopt(argc, argv, "d")) != -1) {
    switch (c) {
    case 'd':
      as_daemon = true;
      break;
    default:
      ERROR_LOG("Wrong flag %c", c);
      ERROR_LOG("aesdsocket [-d]");
      abort();
    }
  }
  return as_daemon;
}

int reap_dead_processes(void) {

  struct sigaction sa;
  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    ERROR_LOG("sigaction");
    return -1;
  }
  return 1;
}

bool is_error(int val) { return val < 0; }
