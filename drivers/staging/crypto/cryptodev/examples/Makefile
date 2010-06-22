KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build

hostprogs := cipher hmac speed
example-cipher-objs := cipher.o
example-hmac-objs := hmac.o
example-speed-objs := speed.c

check: $(hostprogs)
	./cipher
	./hmac

clean:
	rm -f *.o *~ hmac cipher speed
