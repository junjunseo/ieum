CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
CPPFLAGS = -Isrc

ifeq ($(OS),Windows_NT)
	EXE = .exe
endif

BUILD_DIR = build
TARGET = $(BUILD_DIR)/ieum$(EXE)
TEST_PARSER = $(BUILD_DIR)/testParser$(EXE)
TEST_PIPELINE = $(BUILD_DIR)/testPipeline$(EXE)
TEST_CHECKER = $(BUILD_DIR)/testChecker$(EXE)
HEADERS = src/token.h src/ast.h src/lexer.h src/parser.h src/checker.h

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): src/main.cpp $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(TEST_PARSER): test/testParser.cpp src/token.h src/ast.h src/parser.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(TEST_PIPELINE): test/testPipeline.cpp $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(TEST_CHECKER): test/testChecker.cpp $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

run: $(TARGET)
	./$(TARGET) examples/valid.ieum

test: $(TEST_PARSER) $(TEST_PIPELINE) $(TEST_CHECKER)
	./$(TEST_PARSER)
	./$(TEST_PIPELINE)
	./$(TEST_CHECKER)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all run test clean
