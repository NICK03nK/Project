test:test.cpp
	g++ -std=c++11 $^ -o $@ -L/usr/lib64/mysql/ -lmysqlclient -ljsoncpp

.PHONY:clean
clean:
	rm -f test