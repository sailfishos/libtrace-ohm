CC        := gcc
WARNFLAGS := -Wall

ifeq ($(strip $(debug)),yes)
DBGFLAGS := -g3
OPTFLAGS := -O0
else
OPTFLAGS := -O2
endif

INCDIRS  := .. .
LIBDIRS  := ../trace

CFLAGS   := $(WARNFLAGS) $(DBGFLAGS) $(OPTFLAGS) \
            $(foreach d,$(INCDIRS),-I$(d))
LDFLAGS  := $(foreach d,$(LIBDIRS),-L$(d))

SOURCES := tracetest.c
OBJECTS := $(patsubst %.c,%.o,$(SOURCES))

all: tracetest

tracetest: $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS) -ltrace

%.o: %.c
	$(CC) -c $(CFLAGS) $<

%.E: %.c
	$(CC) -E $(CFLAGS) $< -o $@

clean:
	rm -f *.o *~ tracetest *.a log.html report.html output.xml
	
distclean: clean

install:

sample.out: tracetest
	export LD_LIBRARY_PATH="$$LD_LIBRARY_PATH:`pwd`/../trace"; \
	    ./tracetest | \
	        sed 's/^.* tracetest: /__TIME__ tracetest: /g' | \
	        sed 's/[^ ]*@.*:[0-9]*$$/__LOCATION__/g' | ./sorttags > $@

unittest: all sample.out
	./run.sh robot || exit 1
	@# This is an ugly hack to run this only in asgard...
	@if [ "`hostname`" = "asgard.emea.nsn-net.net" ]; then \
	    cp ../../log/test/robot/resources/libtest-py_run.sh \
	    robot/tmp-libtest-py_run.sh && \
	    chmod u+x robot/tmp-libtest-py_run.sh && \
	    . robot/tmp-libtest-py_run.sh && \
	    rm robot/tmp-libtest-py_run.sh; \
	fi

