# Host build of the editor core.
#
# The editor core is plain C++ and does not need an ESP32 to be exercised, so
# these tests run on the development machine in about a second. That is the
# difference between iterating on word wrap in a REPL-like loop and iterating on
# it one three-minute flash cycle at a time.
#
#   make          build and run
#   make clean

CXX ?= c++
SRC_DIR := ../arduino-alphasmart/src
CXXFLAGS := -std=c++17 -Wall -Wextra -Wno-unused-parameter -g -O0 \
            -I shim -I $(SRC_DIR)

CORE_SOURCES := \
	$(SRC_DIR)/gap_buffer.cpp \
	$(SRC_DIR)/text_buffer.cpp \
	$(SRC_DIR)/text_window.cpp \
	$(SRC_DIR)/text_document.cpp \
	$(SRC_DIR)/editor.cpp \
	$(SRC_DIR)/text_output.cpp \
	$(SRC_DIR)/text_store.cpp \
	$(SRC_DIR)/keyboard_usb.cpp \
	$(SRC_DIR)/keyboard_buffer_usb.cpp

TEST_SOURCES := test_main.cpp

BUILD_DIR := build
BINARY := $(BUILD_DIR)/denada_tests
FUZZ_BINARY := $(BUILD_DIR)/denada_fuzz

.PHONY: all run fuzz clean

all: run

run: $(BINARY)
	@./$(BINARY)

# Randomised key sequences checked against the window invariants after every
# render. Deliberately NOT part of `make`: it reports known-failing invariants
# (see ../docs/editor-architecture.md) and would make a clean run look broken.
# Run it when working on the window, and when the mark layer lands it should
# reach zero failures.
fuzz: $(FUZZ_BINARY)
	@./$(FUZZ_BINARY)

$(FUZZ_BINARY): $(CORE_SOURCES) fuzz_main.cpp $(wildcard $(SRC_DIR)/*.h) $(wildcard shim/*.h)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CORE_SOURCES) fuzz_main.cpp -o $(FUZZ_BINARY)

$(BINARY): $(CORE_SOURCES) $(TEST_SOURCES) $(wildcard $(SRC_DIR)/*.h) $(wildcard shim/*.h) test_util.h
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CORE_SOURCES) $(TEST_SOURCES) -o $(BINARY)

clean:
	rm -rf $(BUILD_DIR)
