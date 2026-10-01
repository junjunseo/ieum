CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
VERSION := $(strip $(file <VERSION))
CPPFLAGS = -Isrc -DIEUM_VERSION=\"$(VERSION)\"

ifeq ($(OS),Windows_NT)
	EXE = .exe
endif

BUILD_DIR = build
TARGET = $(BUILD_DIR)/ieum$(EXE)
TEST_PARSER = $(BUILD_DIR)/testParser$(EXE)
TEST_PIPELINE = $(BUILD_DIR)/testPipeline$(EXE)
TEST_CHECKER = $(BUILD_DIR)/testChecker$(EXE)
TEST_GRAPH = $(BUILD_DIR)/testGraph$(EXE)
TEST_SEMANTIC = $(BUILD_DIR)/testSemantic$(EXE)
TEST_INTERPRETER = $(BUILD_DIR)/testInterpreter$(EXE)
TEST_VALUES = $(BUILD_DIR)/testValues$(EXE)
TEST_CONTROL_FLOW = $(BUILD_DIR)/testControlFlow$(EXE)
BENCHMARK_CHECKER = $(BUILD_DIR)/benchmarkChecker$(EXE)
BENCHMARK_MODULES ?= 200
BENCHMARK_ITERATIONS ?= 7
HEADERS = src/token.h src/source.h src/value.h src/evaluator.h src/ast.h src/lexer.h src/parser.h src/checker.h src/graph.h src/semantic.h src/interpreter.h src/version.h
INVALID_EXAMPLES = \
	examples/implicit_dependency.ieum \
	examples/cyclic_dependency.ieum \
	examples/layer_violation.ieum \
	examples/transitive_layer_violation.ieum \
	examples/invalid_declarations.ieum \
	examples/semantic_undefined_function.ieum \
	examples/semantic_arity_mismatch.ieum \
	examples/semantic_missing_dependency.ieum \
	examples/semantic_undefined_variable.ieum
VALID_EXAMPLES = \
	examples/valid.ieum \
	examples/module_body.ieum

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): src/main.cpp $(HEADERS) VERSION | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(TEST_PARSER): test/testParser.cpp $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(TEST_PIPELINE): test/testPipeline.cpp $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(TEST_CHECKER): test/testChecker.cpp $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(TEST_GRAPH): test/testGraph.cpp $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(TEST_SEMANTIC): test/testSemantic.cpp $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(TEST_INTERPRETER): test/testInterpreter.cpp $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(TEST_VALUES): test/testValues.cpp $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(TEST_CONTROL_FLOW): test/testControlFlow.cpp $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BENCHMARK_CHECKER): benchmark/benchmarkChecker.cpp $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -O2 -DNDEBUG $< -o $@

run: $(TARGET)
	./$(TARGET) examples/valid.ieum

test: $(TEST_PARSER) $(TEST_PIPELINE) $(TEST_CHECKER) $(TEST_GRAPH) $(TEST_SEMANTIC) $(TEST_INTERPRETER) $(TEST_VALUES) $(TEST_CONTROL_FLOW) $(TARGET) $(BENCHMARK_CHECKER)
	./$(TEST_PARSER)
	./$(TEST_PIPELINE)
	./$(TEST_CHECKER)
	./$(TEST_GRAPH)
	./$(TEST_SEMANTIC)
	./$(TEST_INTERPRETER)
	./$(TEST_VALUES)
	./$(TEST_CONTROL_FLOW)
	./$(BENCHMARK_CHECKER) 2 1
	test "$$(./$(TARGET) --version)" = "ieum $(VERSION)"
	@for example in $(VALID_EXAMPLES); do \
		./$(TARGET) $$example || exit 1; \
	done
	./$(TARGET) examples/execution.ieum --run service.main
	./$(TARGET) examples/values.ieum --run app.main
	./$(TARGET) examples/control_flow.ieum --run app.main
	@./$(TARGET) examples/execution.ieum --run service.missing >/dev/null 2>&1; \
		status=$$?; \
		if [ $$status -ne 1 ]; then \
			echo "Expected missing entry function to exit with 1, got $$status"; \
			exit 1; \
		fi
	@./$(TARGET) examples/execution.ieum --run service.prepare >/dev/null 2>&1; \
		status=$$?; \
		if [ $$status -ne 1 ]; then \
			echo "Expected parameterized entry function to exit with 1, got $$status"; \
			exit 1; \
		fi
	@./$(TARGET) examples/execution.ieum --run service >/dev/null 2>&1; \
		status=$$?; \
		if [ $$status -ne 2 ]; then \
			echo "Expected invalid entry format to exit with 2, got $$status"; \
			exit 1; \
		fi
	@for example in $(INVALID_EXAMPLES); do \
		./$(TARGET) $$example >/dev/null 2>&1; \
		status=$$?; \
		if [ $$status -eq 0 ]; then \
			echo "Expected validation violation for $$example"; \
			exit 1; \
		fi; \
		if [ $$status -ne 1 ]; then \
			echo "Expected exit code 1 for $$example, got $$status"; \
			exit 1; \
		fi; \
	done

benchmark: $(BENCHMARK_CHECKER)
	./$(BENCHMARK_CHECKER) $(BENCHMARK_MODULES) $(BENCHMARK_ITERATIONS)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all run test benchmark clean
