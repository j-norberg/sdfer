sdfer: src/main.cpp src/sdfer.cpp src/sdfer.h
	g++ -o sdfer src/main.cpp src/tests.cpp src/sdfer.cpp -I./src -O3
