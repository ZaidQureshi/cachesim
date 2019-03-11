all: compile

compile:
	export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/zaid/cache/libbf/lib
	g++ -std=c++11 -I/home/zaid/cache/libbf/include/ cachesim.cpp -o cachesim   -L/home/zaid/cache/libbf/lib/ -lbf  
run: 
	export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/zaid/cache/libbf/lib
