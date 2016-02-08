.SUFFIXES: .cpp .o

CC=g++

SRCS=./rwlock.cpp\
			./rwlock_main.cpp
			
OBJS=$(SRCS:.cpp=.o)
EXEC=rwlock_main

all: $(OBJS)
	$(CC) -o $(EXEC) $(OBJS) -lpthread
	@echo '----------------ok------------------'
	
.cpp.o:
	$(CC) -std=c++0x -Wall -g -o $@ -c $< -I../ -I./
	
clean:
	rm -f $(OBJS)
	rm -f core*