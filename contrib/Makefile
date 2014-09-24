CC=gcc
CFLAGS=-c -Wall
LDFLAGS=
SRCS=genspec.c
OBJS=$(SRCS:.c=.o)
PROG=genspec
SPEC_IN=ndctl.spec.in
SPEC=$(SPEC_IN:.in=)

all: $(SPEC)

$(SPEC) : $(SPEC_IN) $(PROG)
	cat $(SPEC_IN) | $(dir $(PROG))$(PROG) > $@

$(PROG) : $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm $(OBJS) $(PROG) $(SPEC)

depend: .depend

.depend: $(SRCS)
	rm -f $@ > /dev/null 2>&1
	$(CC) $(CFLAGS) -MM $^ -MF $@

include .depend
