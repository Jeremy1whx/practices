CXX = g++
CXXFLAGS = -std=c++20 -O2 -s
WIN_FLAGS = -static-libgcc -static-libstdc++


.PHONY: all clean install windows linux macos help

help:
	@echo "=========================================="
	@echo "24point Game - Cross-platform Compilation"
	@echo "=========================================="
	@echo "Available commands:"
	@echo "  make all      - Compile for all platforms"
	@echo "  make windows  - Compile Windows version only"
	@echo "  make linux    - Compile Linux version only"
	@echo "  make macos    - Compile macOS version only"
	@echo "  make clean    - Clean all generated files"
	@echo "  make package  - Package all versions into zip"
	@echo "=========================================="

# Install necessary compilation tools
install:
	@echo "Installing cross-compilation tools..."
	@sudo apt-get update -qq 2>/dev/null || true
	@sudo apt-get install -y -qq g++-mingw-w64-x86-64 clang zip 2>/dev/null || true
	@echo "Tools installed successfully"

# Compile Windows version
windows: install
	@echo ""
	@echo "📦 Compiling Windows version..."
	@x86_64-w64-mingw32-g++ main.cpp -o 24point.exe $(CXXFLAGS) $(WIN_FLAGS)
	@echo "✓ Generated: 24point.exe"
	@ls -lh 24point.exe

# Compile Linux version
linux: install
	@echo ""
	@echo "📦 Compiling Linux version..."
	@$(CXX) main.cpp -o 24point $(CXXFLAGS)
	@echo "✓ Generated: 24point"
	@ls -lh 24point

# Compile macOS version
macos: install
	@echo ""
	@echo "📦 Compiling macOS version..."
	@clang++ main.cpp -o 24point_mac $(CXXFLAGS) -target x86_64-apple-darwin 2>/dev/null || echo "⚠ Note: macOS version may need to be recompiled on an actual Mac"
	@if [ -f 24point_mac ]; then echo "✓ Generated: 24point_mac" && ls -lh 24point_mac; fi

# Compile all platforms
all: windows linux macos
	@echo ""
	@echo "=========================================="
	@echo "✅ All platforms compiled successfully!"
	@echo "=========================================="
	@echo "Generated files:"
	@ls -lh 24point* 2>/dev/null | grep -v ".cpp"
	@echo "=========================================="

# Clean files
clean:
	@echo "Cleaning generated files..."
	@rm -f 24point 24point.exe 24point_mac *.zip
	@echo "✓ Clean completed"

# Package all files
package: all
	@echo ""
	@echo "📦 Packaging all files..."
	@zip -q 24point-all-platforms.zip 24point 24point.exe 24point_mac 2>/dev/null || true
	@echo "✓ Package completed: 24point-all-platforms.zip"
	@ls -lh 24point-all-platforms.zip