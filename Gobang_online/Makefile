mysql:mysql.cc
	g++ -o $@ $^ -std=c++11 -L/usr/lib64/mysql/ -lmysqlclient

json:json.cc
	g++ -o $@ $^ -std=c++11 -lpthread -lboost_system -ljsoncpp

wsServer:wsServer.cc
	g++ -o $@ $^ -std=c++11 -lpthread -lboost_system

.PHONY:clean
clean:
	rm -f wsServer json mysql