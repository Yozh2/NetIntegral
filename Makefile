.PHONY: all clean

TARGET = ./server ./client
OBJS = server.o client.o
DEPS = $(OBJS:.o=.d)
FLAGS = -Wall -Wextra -Wextra -Werror -Wpedantic -g -O3
ifeq ($(shell uname),Linux)
	FLAGS += -pthread -DLINUX
endif


all: $(TARGET)

server: server.o
	gcc $(FLAGS) -o $@ $<

client: client.o
	gcc $(FLAGS) -o $@ $<

-include $(DEPS)

%.o: %.c
	gcc $(FLAGS) -c -o $@ $<
	gcc $(FLAGS) -MM -o $(patsubst %.o, %.d, $@) $<

clean:
	-@rm $(OBJS) $(TARGET) -rf
	-@find . -name "*.o" | xargs rm -rf
	-@find . -name "*.d" | xargs rm -rf
