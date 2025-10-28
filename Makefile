# Makefile for Flic Custom Client
# Based on the simpleclient Makefile from fliclib-linux-hci

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
TARGET_LINUX = flic_custom_client_linux
SOURCE_LINUX = flic_custom_client_linux.cpp
PROTOCOL_HEADER = fliclib-linux-hci/simpleclient/client_protocol_packets.h

# Default target
all: $(TARGET_LINUX)

# Build the Linux executable (recommended for Raspberry Pi)
linux: $(TARGET_LINUX)

# Build the Windows-compatible executable
windows: $(TARGET)

# Build the Linux executable
$(TARGET_LINUX): $(SOURCE_LINUX) $(PROTOCOL_HEADER)
	$(CXX) $(CXXFLAGS) -o $(TARGET_LINUX) $(SOURCE_LINUX)

# Build the Windows-compatible executable
$(TARGET): $(SOURCE) $(PROTOCOL_HEADER)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE)

# Clean build artifacts
clean:
	rm -f $(TARGET) $(TARGET_LINUX)

# Install target (optional)
install: $(TARGET_LINUX)
	cp $(TARGET_LINUX) /usr/local/bin/

# Uninstall target (optional)
uninstall:
	rm -f /usr/local/bin/$(TARGET_LINUX)

# Debug build
debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET_LINUX)

# Release build
release: CXXFLAGS += -DNDEBUG
release: $(TARGET_LINUX)

# Help target
help:
	@echo "Available targets:"
	@echo "  all      - Build the Linux flic_custom_client executable (default)"
	@echo "  linux    - Build the Linux version (recommended for Raspberry Pi)"
	@echo "  windows  - Build the Windows-compatible version"
	@echo "  clean    - Remove build artifacts"
	@echo "  debug    - Build with debug symbols"
	@echo "  release  - Build optimized release version"
	@echo "  install  - Install to /usr/local/bin (requires sudo)"
	@echo "  uninstall- Remove from /usr/local/bin"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "For Raspberry Pi deployment, use: make linux"

.PHONY: all linux windows clean install uninstall debug release help
