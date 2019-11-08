TOP_DIR = .
INC_DIR = $(TOP_DIR)/inc
SRC_DIR = $(TOP_DIR)/src
BUILD_DIR = $(TOP_DIR)/build
CC=gcc
FLAGS = -pthread -fPIC -g -ggdb -Wall -DDEBUG -I$(INC_DIR)
OBJS = $(BUILD_DIR)/cmu_packet.o \
	$(BUILD_DIR)/cmu_tcp.o \
	$(BUILD_DIR)/backend.o

default:all
all: receiver sender client server

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(FLAGS) -c -o $@ $<

receiver: $(OBJS)
	$(CC) $(FLAGS) $(SRC_DIR)/receiver.c -o receiver $(OBJS)

sender: $(OBJS)
	$(CC) $(FLAGS) $(SRC_DIR)/sender.c -o sender $(OBJS)

server: $(OBJS)
	$(CC) $(FLAGS) $(SRC_DIR)/server.c -o server $(OBJS)

client: $(OBJS)
	$(CC) $(FLAGS) $(SRC_DIR)/client.c -o client $(OBJS)


clean:
	-rm -f $(BUILD_DIR)/*.o peer sender receiver client server
test: all