CC=gcc
CFLAGS=-std=c99 -O2 -w
LDFLAGS=-lm
SRC=ada83.c
BIN=ada83
RTS_DIR=rts
TEST_DIR=acats
TEST_RESULTS=test_results
ACATS_LOGS=acats_logs

.PHONY: all clean test test-a test-b test-c test-full rts help

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $(BIN) $(SRC) $(LDFLAGS)

rts: $(BIN)
	./$(BIN) $(RTS_DIR)/report.adb > $(RTS_DIR)/report.ll 2>&1 || true

clean:
	rm -f $(BIN)
	rm -rf $(TEST_RESULTS) $(ACATS_LOGS)
	rm -f $(RTS_DIR)/report.ll
	rm -f test_summary.txt

test-dirs:
	mkdir -p $(TEST_RESULTS) $(ACATS_LOGS)

test-a: $(BIN) rts test-dirs
	./test.sh g a

test-b: $(BIN) rts test-dirs
	./test.sh g b

test-c: $(BIN) rts test-dirs
	./test.sh g c

test-full: $(BIN) rts test-dirs
	./test.sh f

test: test-a

quick: CFLAGS=-std=c99 -O0 -w
quick: clean all

debug: CFLAGS=-std=c99 -g -O0
debug: clean all

help:
	@echo "Ada83 Compiler Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build ada83 compiler (default)"
	@echo "  rts        - Build runtime system (report.ll)"
	@echo "  clean      - Remove build artifacts and test results"
	@echo "  test       - Run A-series ACATS tests (same as test-a)"
	@echo "  test-a     - Run A-series ACATS tests"
	@echo "  test-b     - Run B-series ACATS tests"
	@echo "  test-c     - Run C-series ACATS tests"
	@echo "  test-full  - Run all ACATS tests"
	@echo "  quick      - Quick build without optimization"
	@echo "  debug      - Build with debug symbols"
	@echo ""
	@echo "Examples:"
	@echo "  make              # Build compiler"
	@echo "  make test         # Run A-series tests"
	@echo "  make clean all    # Rebuild from scratch"
