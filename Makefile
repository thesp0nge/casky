# --------------------------
# Compiler & Tools
# --------------------------
CC = gcc
CFLAGS = -Wall -Wextra -I./src -fPIC
AR = ar
ARFLAGS = rcs
RM = rm -rf

# Optional debug code
# Uncomment the next line to compile the code with debug information
CFLAGS += -g 

# Optional thread-safety flag
# Uncomment the next line to enable thread-safe API
CFLAGS += -DTHREAD_SAFE -pthread

# --------------------------
# Source Files
# --------------------------
SRC = src/casky.c src/utils.c src/crc.c
OBJ = $(patsubst src/%.c,build/%.o,$(SRC))

SERVER_SRC = src/caskyd.c
SERVER_OBJ = $(BUILD_DIR)/caskyd.o
SERVER_BIN = $(BUILD_DIR)/caskyd

# --------------------------
# Output Files
# --------------------------
BUILD_DIR = build
STATIC_LIB = $(BUILD_DIR)/libcasky.a
DYNAMIC_LIB = $(BUILD_DIR)/libcasky.so
TEST_SRC = tests/test_casky.c
TEST_BIN = $(BUILD_DIR)/test_casky
TEST_DAEMON_SRC = tests/test_caskyd.c
TEST_DAEMON_BIN = $(BUILD_DIR)/test_caskyd

TEST_STRESS_DAEMON_SRC = tests/test_stress_caskyd.c
TEST_STRESS_DAEMON_BIN = $(BUILD_DIR)/test_stress_caskyd

LOGDUMP_SRC = src/casky_logdump.c
LOGDUMP_BIN = $(BUILD_DIR)/casky_logdump

# --------------------------
# Targets
# --------------------------
all: $(STATIC_LIB) $(DYNAMIC_LIB) $(TEST_BIN) $(SERVER_BIN) $(LOGDUMP_BIN)

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

# -----------------------------
# Server binary
# -----------------------------
$(SERVER_BIN): $(SERVER_SRC) $(STATIC_LIB)
	$(CC) $(CFLAGS) $(SERVER_SRC) $(STATIC_LIB) -o $(SERVER_BIN)

# Build test executable
$(TEST_BIN): $(TEST_SRC) $(STATIC_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(TEST_SRC) $(STATIC_LIB)

$(TEST_DAEMON_BIN): $(TEST_DAEMON_SRC) $(STATIC_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(TEST_DAEMON_SRC) $(STATIC_LIB)

$(TEST_STRESS_DAEMON_BIN): $(TEST_STRESS_DAEMON_SRC) $(STATIC_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(TEST_STRESS_DAEMON_SRC) $(STATIC_LIB)

$(LOGDUMP_BIN): $(LOGDUMP_SRC) $(STATIC_LIB) | $(BUILD)
	$(CC) $(CFLAGS) $(LOGDUMP_SRC) $(STATIC_LIB) -o $(LOGDUMP_BIN)

# Run tests
test: $(TEST_BIN) $(TEST_DAEMON_BIN) $(TEST_STRESS_DAEMON_BIN)
	./$(TEST_BIN)
	./$(TEST_DAEMON_BIN)
	./$(TEST_STRESS_DAEMON_BIN)

# Clean all build artifacts
clean:
	$(RM) $(BUILD_DIR)
	$(RM) *.log
	$(RM) caskyd.db

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
