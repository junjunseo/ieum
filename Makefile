CXX = g++
CXXFLAGS = -std=c++17 -Wall

TARGET = ieum
SRC = src/main.cpp

$(TARGET): $(SRC) src/lexer.h src/token.h
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: run clean
