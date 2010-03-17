KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build

hostprogs := cipher hmac
example-cipher-objs := cipher.o
example-hmac-objs := hmac.o

check: $(hostprogs)
	./cipher
	./hmac

clean:
	rm -f *.o *~ hmac cipher
