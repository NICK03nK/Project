all: server client
# server:tcp_svr.cc
# 	g++ -o $@ $^ -std=c++11 -lpthread

server:server.cc
	g++ -g -o $@ $^ -std=c++11 -lpthread

client:tcp_cli.cc
	g++ -g -o $@ $^ -std=c++11 -lpthread

.PHONY:clean
clean:
	rm -f server client