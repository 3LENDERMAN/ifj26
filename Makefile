# IFJ26 compiler - Makefile
# Author: Jakub Holub (xholubj00)
# Date: 4.10.2026
# 
# Description: script for the automated translation of project binary files.


# Compiler and flags variables
CC=gcc
CFLAGS=-std=c11 -Wall -Wextra -Werror -g
TARGET=ifj26-compiler

# Find all .c source files in the current directory
SOURCES=$(wildcard src/*.c)
HEADERS=$(wildcard src/*.h)
OBJECTS=$(SOURCES:.c=.o)

# Default target when running 'make' 
.PHONY: all build build-check format clean valgrind

all: build

# Compile the final executable program
build: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

# Compiling .c files into .o object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Extension for code formatting
format:
	clang-format -i --style=LLVM $(SOURCES) $(HEADERS)

# Target for static code analysis using cppcheck, followed by build
build-check: format
	cppcheck --enable=all --suppress=missingIncludeSystem $(SOURCES)
	$(MAKE) build

# Help extension for quick memory check
valgrind: build
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TARGET)

# Clean up artifacts and binary files
clean:
	rm -f $(OBJECTS) $(TARGET)
