bday: bday.cpp Makefile
	g++ -std=c++20 -g -O3 -Wall -march=native -I ./external/hoytech-cpp bday.cpp -lcrypto -lpthread -ltbb -o bday
