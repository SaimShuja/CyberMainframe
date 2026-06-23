#!/usr/bin/env python3
"""
html_to_header.py – Convert index.html to an Arduino C++ header.

This script is intended to be placed in the repository's 'tools/' folder.
It reads ../web/index.html and writes ../firmware/esp32/include/index_html.h

Usage (from the tools/ folder):
    python html_to_header.py
"""

import os
import sys

# =============================================================================
# HARDCODED RELATIVE PATHS (assuming script is in <repo_root>/tools/)
# =============================================================================

# This script's own directory (tools/)
script_dir = os.path.dirname(os.path.abspath(__file__))

# The repository root is one level up
repo_root = os.path.abspath(os.path.join(script_dir, ".."))

# Define input and output paths relative to the repo root
input_file = os.path.join(repo_root, "web", "index.html")
output_file = os.path.join(repo_root, "firmware", "esp32", "include", "index_html.h")

# =============================================================================
# GENERATE HEADER
# =============================================================================

def generate_header(html_content, var_name="INDEX_HTML"):
    """
    Generate the header file content using a raw string literal.
    """
    header = f"""// Auto-generated from index.html – DO NOT EDIT.
// This file is included in the ESP32 firmware to serve the web interface.

#ifndef INDEX_HTML_H
#define INDEX_HTML_H

#include <Arduino.h>

// Use a raw string literal to preserve formatting without escaping.
static const char {var_name}[] PROGMEM = R"=====(
{html_content}
)=====";

#endif // INDEX_HTML_H
"""
    return header

# =============================================================================
# MAIN
# =============================================================================

def main():
    # Read the HTML file
    try:
        with open(input_file, 'r', encoding='utf-8') as f:
            html = f.read()
        print(f"Read HTML from: {input_file}")
    except FileNotFoundError:
        print(f"Error: HTML file not found at: {input_file}")
        sys.exit(1)
    except Exception as e:
        print(f"Error reading input file: {e}")
        sys.exit(1)

    # Ensure the output directory exists
    out_dir = os.path.dirname(output_file)
    if out_dir and not os.path.exists(out_dir):
        os.makedirs(out_dir, exist_ok=True)

    # Generate and write the header
    header_content = generate_header(html)

    try:
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(header_content)
        print(f"Header successfully written to: {output_file}")
    except Exception as e:
        print(f"Error writing output file: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()