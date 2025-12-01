# --------------------------
# Compiler & Tools
# --------------------------
CC = gcc
CFLAGS = -Wall -Wextra -I./src -g -fPIC
AR = ar
ARFLAGS = rcs
RM = rm -rf

# --------------------------
# Source Files
# --------------------------
SRC = src/casky.c src/utils.c
OBJ = $(patsubst src/%.c,build/%.o,$(SRC))

# --------------------------
# Output Files
# --------------------------
BUILD_DIR = build
STATIC_LIB = $(BUILD_DIR)/libcasky.a
DYNAMIC_LIB = $(BUILD_DIR)/libcasky.so
TEST_SRC = tests/test_casky.c
TEST_BIN = $(BUILD_DIR)/test_casky

# --------------------------
# Targets
# --------------------------
all: $(STATIC_LIB) $(DYNAMIC_LIB) $(TEST_BIN)

# Ensure build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile object files
$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Build static library
$(STATIC_LIB): $(OBJ)
	$(AR) $(ARFLAGS) $@ $^

# Build dynamic library
$(DYNAMIC_LIB): $(OBJ)
	$(CC) -shared -o $@ $^

# Build test executable
$(TEST_BIN): $(TEST_SRC) $(STATIC_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(TEST_SRC) $(STATIC_LIB)

# Run tests
test: $(TEST_BIN)
	./$(TEST_BIN)

# Clean all build artifacts
clean:
	$(RM) $(BUILD_DIR)

.PHONY: all clean test

# --------------------------
# Installation paths
# --------------------------
PREFIX = /usr/local
INCLUDEDIR = $(PREFIX)/include
LIBDIR = $(PREFIX)/lib

# --------------------------
# Install target
# --------------------------
install: all
	@echo "Installing Casky..."
	install -d $(INCLUDEDIR)
	install -d $(LIBDIR)
	cp src/casky.h $(INCLUDEDIR)/
	cp src/utils.h $(INCLUDEDIR)/
	cp $(STATIC_LIB) $(LIBDIR)/
	cp $(DYNAMIC_LIB) $(LIBDIR)/
	@echo "Casky installed to $(PREFIX)"

# --------------------------
# Uninstall target
# --------------------------
uninstall:
	@echo "Removing Casky..."
	rm -f $(INCLUDEDIR)/casky.h
	rm -f $(INCLUDEDIR)/utils.h
	rm -f $(LIBDIR)/libcasky.a
	rm -f $(LIBDIR)/libcasky.so
	@echo "Casky uninstalled"
