#CXX=g++
#CC=gcc
INCLUDES=-I/usr/include\
		 -I/usr/local/include\
		 -I../simple/3rd/include\
		 -I../simple/3rd/include/lua-5.14\
		 -I/usr/local/mysql/include
CXXFLAGS=-c -Wall -g $(INCLUDES)

#LIBS=../simple/3rd/lib/liblua.a
	
SRCDIRS=.
SRCS=$(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.c))

OBJS=$(SRCS:.c=.o)
PROG=./lmpack.so

all: $(PROG) $(MODULE)

install:
clean:
	rm -rf $(OBJS) $(PROG) $(PROG).core

$(PROG): $(OBJS)
	gcc -g $(OBJS) -fPIC -shared -o $(PROG) $(LIBS) -ldl -lrt -lc -rdynamic -lm
.c.o:
	gcc $(CXXFLAGS) -fPIC $< -o $@