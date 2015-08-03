CC=gcc
CFLAGS=-c -Wall
LDFLAGS=
SRCS=genspec.c
OBJS=$(SRCS:.c=.o)
PROG=genspec
SPEC_IN=ndctl.spec.in
RHEL_SPEC=rhel/$(SPEC_IN:.in=)
SLES_SPEC=sles/$(SPEC_IN:.in=)
COMMIT_ID=git log --pretty=format:"%h" -n 1

all: $(RHEL_SPEC) $(SLES_SPEC)

$(RHEL_SPEC) : $(SPEC_IN) $(PROG)
	@mkdir -p rhel
	cat $(SPEC_IN) | $(dir $(PROG))$(PROG) `$(COMMIT_ID)` rhel > $@

$(SLES_SPEC) : $(SPEC_IN) $(PROG)
	@mkdir -p sles
	cat $(SPEC_IN) | $(dir $(PROG))$(PROG) `$(COMMIT_ID)` sles > $@

$(PROG) : $(OBJS) Makefile
	$(CC) $(LDFLAGS) $(OBJS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm $(OBJS) $(PROG) $(RHEL_SPEC) $(SLES_SPEC)
	@rmdir rhel sles

depend: .depend

.depend: $(SRCS)
	rm -f $@ > /dev/null 2>&1
	$(CC) $(CFLAGS) -MM $^ -MF $@

include .depend
