all: Simple

Simple: Simple.o
	g++ -std=c++17 Simple.o -o Simple

Simple.o: ./../src/Buffer/Buffer.hpp ./../src/Client/Connector.hpp Reader.hpp Simple.cpp
	g++ -std=c++17 -c Simple.cpp
