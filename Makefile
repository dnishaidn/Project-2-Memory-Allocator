CC = gcc
CPPFLAGS = -I.
CFLAGS = -Wall -std=gnu17
LDFLAGS = -L.
LDLIBS = -pthread -lm
export CC CPPFLAGS CFLAGS LDFLAGS LDLIBS

SUBDIRS = liballocator
.PHONY: default debug clean $(SUBDIRS)

default: tester

debug: export CFLAGS += -g -fsanitize=thread
debug: default

$(SUBDIRS):
	$(MAKE) -C $@

tester: main.c liballocator
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ main.c -Iliballocator -Lliballocator -lallocator $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf tester output
	@for d in $(SUBDIRS); do $(MAKE) -C $$d clean; done
