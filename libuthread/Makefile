# Target library
lib := libuthread.a
objs := queue.o thread.o tps.o sem.o
objs_to_compile := tps.o sem.o
CC := gcc
CC_Lib := ar rcs
CFLAGS := -Wall
##CFLAGS += -g -Werror

ifneq ($(V),1)
	Q = @
endif

all: $(lib)

deps := $(patsubt %.o,%.d,$(objs_to_compile))
-include $(deps)
DEPFLAGS = -MMD -MF $(@:.o=.d)

$(lib): $(objs)
	@echo "CC $@"
	$(Q)$(CC_Lib) -o $@ $^

%.o: %.c
	@echo "CC $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $< $(DEPFLAGS)

clean:
	@echo "clean"
	$(Q)rm -f $(lib) $(objs_to_compile) $(deps)



## TODO: Phase 1 and Phase 2
