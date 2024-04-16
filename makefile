sdfer: src/main.cpp src/tests.cpp ../ok_sdf/src/ok_sdf.cpp ../ok_sdf/src/ok_sdf.h
	g++ -o sdfer src/main.cpp src/tests.cpp ../ok_sdf/src/ok_sdf.cpp -I./src -O3 -std=c++11
