KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build

hostprogs := cipher hmac speed async_cipher async_hmac async_speed
example-cipher-objs := cipher.o
example-hmac-objs := hmac.o
example-speed-objs := speed.c
example-async-cipher-objs := async_cipher.o
example-async-hmac-objs := async_hmac.o
example-async-speed-objs := async_speed.o

check: $(hostprogs)
	./cipher
	./hmac
	./async_cipher
	./async_hmac

clean:
	rm -f *.o *~ hmac cipher speed async_hmac async_cipher async_speed
