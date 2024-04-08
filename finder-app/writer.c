#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>

int main(int argc, char* argv[]) {
  openlog(NULL,0,LOG_USER);

  if (argc < 3) {
    syslog(LOG_ERR, "Invalid number of arguments: %d", argc);
    printf("use:\nwrite filepath writestr\n");
    exit(1);
  }
  const char* filename = argv[1];
  const char* text = argv[2];

  FILE* fptr = fopen(filename, "w");

  if (fptr == NULL) {
    fprintf(stderr, "value of errno attempting to open file %s: %d\n", argv[1], errno);
    printf("Error opening file %s: %s\n", filename, strerror(errno));
    syslog(LOG_ERR, "Error opening file %s",filename);
    exit(1);
  }

  syslog(LOG_DEBUG, "Writing %s to %s", text, filename);
  fprintf(fptr, "%s", text);

  fclose(fptr);
  return 0;
}
