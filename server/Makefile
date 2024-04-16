##
# aesdsocket
#
# @file
# @version 0.1

CC ?= $(CROSS_COMPILE)gcc
CFLAGS = -O2 -Wall -Werror -Wextra -pthread
LDLIBS += -pthread

all: aesdsocket

aesdsocket: aesdsocket.o

clean:
	rm -f *.o aesdsocket

# end
