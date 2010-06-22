KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build

hostprogs := cipher hmac cryptodev_test
example-cipher-objs := cipher.o
example-hmac-objs := hmac.o
example-cryptodev_test-objs := cryptodev_test.c

check: $(hostprogs)
	./cipher
	./hmac

clean:
	rm -f *.o *~ hmac cipher cryptodev_test
