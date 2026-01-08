###############################################################################
##                                                                           ##
##                        A D A 8 3   I N T E R P R E T E R                  ##
##                                                                           ##
##                              M a k e f i l e                              ##
##                                                                           ##
##  This Makefile builds the refactored Ada83 interpreter from the          ##
##  literate-style source modules.                                          ##
##                                                                           ##
###############################################################################

#------------------------------------------------------------------------------
# Compiler Configuration
#------------------------------------------------------------------------------

CC       := gcc
CFLAGS   := -std=gnu11 -Wall -Wextra -O2
CFLAGS   += -D_POSIX_C_SOURCE=200809L
LDFLAGS  := -lm -lpthread

# Debug build
ifdef DEBUG
CFLAGS   += -g -O0 -DDEBUG
endif

#------------------------------------------------------------------------------
# Source Files
#------------------------------------------------------------------------------

HEADERS := \
    ada83_common.h \
    ada83_bignum.h \
    ada83_arena.h \
    ada83_string.h \
    ada83_lexer.h \
    ada83_ast.h \
    ada83_types.h \
    ada83_symbols.h \
    ada83_parser.h \
    ada83_eval.h

SOURCES := \
    ada83_bignum.c \
    ada83_arena.c \
    ada83_string.c \
    ada83_lexer.c \
    ada83_ast.c \
    ada83_types.c \
    ada83_symbols.c \
    ada83_parser.c \
    ada83_eval.c

OBJECTS := $(SOURCES:.c=.o)

#------------------------------------------------------------------------------
# Targets
#------------------------------------------------------------------------------

.PHONY: all clean test

all: libada83.a ada83

libada83.a: $(OBJECTS)
	ar rcs $@ $^

ada83: ada83_main.o libada83.a
	$(CC) $(CFLAGS) -o $@ $< -L. -lada83 $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) ada83_main.o libada83.a ada83

#------------------------------------------------------------------------------
# Testing
#------------------------------------------------------------------------------

test: all
	@echo "Running tests..."
	@echo "Tests not yet implemented for refactored modules"

#------------------------------------------------------------------------------
# Documentation
#------------------------------------------------------------------------------

.PHONY: docs

docs:
	@echo "Generating documentation..."
	@echo "Use doxygen or similar tool to generate from literate comments"

#------------------------------------------------------------------------------
# Module Descriptions
#------------------------------------------------------------------------------
#
# ada83_common.h   - Common definitions, includes, forward declarations
# ada83_bignum     - Multiprecision integer arithmetic (universal_integer)
# ada83_arena      - Memory arena allocator for AST and symbols
# ada83_string     - String handling with case-insensitive comparison
# ada83_lexer      - Lexical analysis (tokenization)
# ada83_ast        - Abstract Syntax Tree node definitions
# ada83_types      - Ada83 type system implementation
# ada83_symbols    - Symbol table and scope management
# ada83_parser     - Recursive descent parser per LRM Annex E
# ada83_eval       - Semantic analysis and evaluation/interpretation engine
# ada83_main       - Main program entry point
#
###############################################################################
