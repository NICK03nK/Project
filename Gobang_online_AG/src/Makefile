# test:test.cpp
# 	g++ -std=c++11 $^ -o $@ -L/usr/lib64/mysql/ -lmysqlclient -ljsoncpp -lboost_system -lpthread

online_gobang:online_gobang.cpp
	g++ -std=c++11 $^ -o $@ -L/usr/lib64/mysql/ -lmysqlclient -ljsoncpp -lboost_system -lpthread

.PHONY:clean
clean:
	rm -f online_gobang