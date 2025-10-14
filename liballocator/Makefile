.PHONY: default clean

default: liballocator.a

liballocator.a: my_memory.o interface.o init.o
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c $< $(LDFLAGS) $(LDLIBS)

clean:
	rm -f liballocator.a *.o
