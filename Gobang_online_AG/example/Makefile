MySQL_API:MySQL_API.cpp
	g++ -std=c++11 $^ -o $@ -L/usr/lib64/mysql/ -lmysqlclient

Json:Json.cpp
	g++ -std=c++11 $^ -o $@ -lpthread -lboost_system -ljsoncpp

websocketServer:websocketServer.cpp
	g++ -std=c++11 $^ -o $@ -lpthread -lboost_system

.PHONY:clean
clean:
	rm -f websocketServer
