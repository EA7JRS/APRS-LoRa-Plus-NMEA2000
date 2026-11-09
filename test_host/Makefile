# Copyright (C) 2026 Manuel - EA7JRS - GPL-3.0-or-later
CXXFLAGS = -std=gnu++17 -O1 -g -Wall -Wextra -Werror -Wno-unused-parameter -fsanitize=address,undefined -I../lib/GateCore/src
SRC = ../lib/GateCore/src/nmea0183.cpp ../lib/GateCore/src/ais_vessel.cpp

test_core: test_core.cpp $(SRC) ../lib/GateCore/src/*.h
	$(CXX) $(CXXFLAGS) -o $@ test_core.cpp $(SRC)

run: test_core
	./test_core

clean:
	rm -f test_core
