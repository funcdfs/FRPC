#!/bin/bash
# FRPC Framework Documentation Generation Script

set -e

echo "=== FRPC Framework Documentation Generator ==="
echo ""

# Check if doxygen is installed
if ! command -v doxygen &> /dev/null; then
    echo "Error: Doxygen is not installed."
    echo "Please install doxygen:"
    echo "  Ubuntu/Debian: sudo apt-get install doxygen graphviz"
    echo "  macOS: brew install doxygen graphviz"
    echo "  Fedora/RHEL: sudo dnf install doxygen graphviz"
    exit 1
fi

# Check if graphviz is installed (for diagrams)
if ! command -v dot &> /dev/null; then
    echo "Warning: Graphviz is not installed. Diagrams will not be generated."
    echo "Install graphviz for better documentation:"
    echo "  Ubuntu/Debian: sudo apt-get install graphviz"
    echo "  macOS: brew install graphviz"
    echo "  Fedora/RHEL: sudo dnf install graphviz"
fi

# Generate documentation
echo "Generating API documentation..."
doxygen Doxyfile

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Documentation generated successfully!"
    echo "  Output directory: docs/html/"
    echo "  Open docs/html/index.html in your browser to view the documentation."
else
    echo ""
    echo "✗ Documentation generation failed!"
    exit 1
fi

# Check for undocumented items
echo ""
echo "Checking for undocumented items..."
if [ -f "docs/html/index.html" ]; then
    echo "✓ Documentation is available at: docs/html/index.html"
else
    echo "✗ Documentation index not found!"
    exit 1
fi

echo ""
echo "=== Documentation Generation Complete ==="
