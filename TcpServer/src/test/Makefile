all: client6
# server:tcp_svr.cc
# 	g++ -o $@ $^ -std=c++11 -lpthread

server:server.cc
	g++ -g -o $@ $^ -std=c++11 -lpthread

client:tcp_cli.cc
	g++ -g -o $@ $^ -std=c++11 -lpthread

client1:client1.cc
	g++ -g -o $@ $^ -std=c++11 -lpthread

client2:client2.cc
	g++ -g -o $@ $^ -std=c++11 -lpthread

client3:client3.cc
	g++ -g -o $@ $^ -std=c++11 -lpthread

client4:client4.cc
	g++ -g -o $@ $^ -std=c++11 -lpthread

client5:client5.cc
	g++ -g -o $@ $^ -std=c++11 -lpthread

client6:client6.cc
	g++ -g -o $@ $^ -std=c++11 -lpthread

.PHONY:clean
clean:
	rm -f client6