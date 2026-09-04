CC=$(CROSS_COMPILE)gcc
USE_AESD_CHAR_DEVICE ?= 1

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) -pthread -DUSE_AESD_CHAR_DEVICE=$(USE_AESD_CHAR_DEVICE) -o aesdsocket aesdsocket.c $(LDFLAGS)

clean:
	rm -f aesdsocket
