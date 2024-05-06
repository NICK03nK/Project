test:test.cpp
	g++ -std=c++11 $^ -o $@ -L/usr/lib64/mysql/ -lmysqlclient -ljsoncpp -lboost_system -lpthread

.PHONY:clean
clean:
	rm -f test