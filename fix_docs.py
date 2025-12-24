#!/usr/bin/env python3
"""
Script to clean up markdown documentation files that were extracted from PDFs.
Fixes line numbers, arrows, spacing, and other formatting issues.
"""

import re
import sys

def clean_line_numbers(content):
    """Remove line numbers and arrows from the start of lines."""
    # Pattern: optional spaces + number + → + content
    lines = content.split('\n')
    cleaned_lines = []

    for line in lines:
        # Remove line number prefix: spaces + digits + →
        cleaned = re.sub(r'^\s*\d+→', '', line)
        cleaned_lines.append(cleaned)

    return '\n'.join(cleaned_lines)

def fix_underscores(content):
    """Fix underscores that should be hyphens or spaces."""
    # Fix contract numbers and similar patterns
    content = re.sub(r'N00014_84-C_2445', 'N00014-84-C-2445', content)
    content = re.sub(r'ANSI/MIL_STO-1815A', 'ANSI/MIL-STD-1815A', content)
    content = re.sub(r'ANSI/MIL_STD-1815A', 'ANSI/MIL-STD-1815A', content)

    return content

def fix_dots_spacing(content):
    """Fix excessive dots used for spacing in table of contents."""
    # Replace sequences of dots and spaces with proper spacing
    # Pattern: word followed by many dots/spaces
    lines = content.split('\n')
    cleaned_lines = []

    for line in lines:
        # If line has excessive dots/spaces (likely TOC), clean it up
        if '........' in line or '. . . . .' in line:
            # Replace multiple dots and spaces with single space
            cleaned = re.sub(r'[\s\.]{3,}', ' ', line)
            cleaned_lines.append(cleaned)
        else:
            cleaned_lines.append(line)

    return '\n'.join(cleaned_lines)

def fix_special_chars(content):
    """Fix special characters that got mangled."""
    # Fix common OCR/PDF extraction errors
    content = content.replace('¢', 's')  # section marker
    content = content.replace(''', "'")  # smart quotes
    content = content.replace(''', "'")
    content = content.replace('"', '"')
    content = content.replace('"', '"')

    return content

def fix_code_blocks(content):
    """Fix improperly placed code blocks."""
    # Remove ```ada``` blocks that don't contain actual code
    lines = content.split('\n')
    result = []
    skip_until_closing = False

    i = 0
    while i < len(lines):
        line = lines[i]

        # Handle skip mode
        if skip_until_closing:
            if line.strip().startswith('```'):
                skip_until_closing = False
                i += 1
                continue
            else:
                # Keep the content, just skip the markers
                result.append(line)
                i += 1
                continue

        # Detect code block markers
        if line.strip().startswith('```'):
            # Look ahead to see if content looks like code
            content_lines = []
            j = i + 1
            while j < len(lines) and not lines[j].strip().startswith('```'):
                content_lines.append(lines[j])
                j += 1

            if len(content_lines) == 0:
                # Empty code block, skip both markers
                i = j + 1
                continue

            # Check if this is actual code
            is_actual_code = False
            ada_keywords = ['procedure', 'function', 'package', 'begin', 'end', 'type', 'with', 'use']
            code_indicators = [':=', '--', ';', 'return', 'declare', 'loop', 'if', 'then', 'else']

            for cl in content_lines:
                stripped = cl.strip().lower()
                # Check for Ada keywords at start of line
                for keyword in ada_keywords:
                    if stripped.startswith(keyword + ' ') or stripped == keyword:
                        is_actual_code = True
                        break
                # Check for code indicators
                for indicator in code_indicators:
                    if indicator in cl:
                        is_actual_code = True
                        break
                if is_actual_code:
                    break

            # Also check if it's just a simple list (single words/letters on lines)
            if not is_actual_code:
                # Check if content looks like a simple list
                list_like = all(len(cl.strip().split()) <= 5 for cl in content_lines if cl.strip())
                if list_like and len(content_lines) < 10:
                    # This is likely a list, not code - remove markers
                    skip_until_closing = True
                    i += 1
                    continue

            if is_actual_code:
                # Keep the code block
                result.append(line)
            else:
                # Skip the opening marker and set flag to skip closing
                skip_until_closing = True
                i += 1
                continue
        else:
            result.append(line)

        i += 1

    return '\n'.join(result)

def remove_excessive_blank_lines(content):
    """Remove more than 2 consecutive blank lines."""
    # Replace 3+ blank lines with 2 blank lines
    while '\n\n\n\n' in content:
        content = content.replace('\n\n\n\n', '\n\n\n')

    return content

def fix_spacing(content):
    """Fix various spacing issues."""
    lines = content.split('\n')
    cleaned_lines = []

    for line in lines:
        # Remove trailing whitespace
        line = line.rstrip()

        # Fix multiple spaces (but preserve indentation)
        if line.strip():
            # Get leading whitespace
            leading = len(line) - len(line.lstrip())
            rest = line.lstrip()
            # Fix multiple spaces in content (not in code blocks or special formatting)
            if not rest.startswith('#') and not rest.startswith('-'):
                rest = re.sub(r'  +', ' ', rest)
            line = ' ' * leading + rest

        cleaned_lines.append(line)

    return '\n'.join(cleaned_lines)

def clean_document(content):
    """Apply all cleaning operations to the document."""
    print("  Removing line numbers...")
    content = clean_line_numbers(content)

    print("  Fixing underscores...")
    content = fix_underscores(content)

    print("  Fixing dots and spacing...")
    content = fix_dots_spacing(content)

    print("  Fixing special characters...")
    content = fix_special_chars(content)

    print("  Fixing code blocks...")
    content = fix_code_blocks(content)

    print("  Fixing spacing...")
    content = fix_spacing(content)

    print("  Removing excessive blank lines...")
    content = remove_excessive_blank_lines(content)

    return content

def process_file(filepath):
    """Process a single markdown file."""
    print(f"\nProcessing {filepath}...")

    # Read the file
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        return False

    # Clean the content
    cleaned = clean_document(content)

    # Write back
    try:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(cleaned)
        print(f"  ✓ Successfully cleaned {filepath}")
        return True
    except Exception as e:
        print(f"Error writing {filepath}: {e}")
        return False

def main():
    """Main function to process all documentation files."""
    files = [
        '/home/user/Ada83/reference/gnat-design.md',
        '/home/user/Ada83/reference/DIANA.md',
        '/home/user/Ada83/reference/Ada83_LRM.md'
    ]

    print("Starting documentation cleanup...")
    success_count = 0

    for filepath in files:
        if process_file(filepath):
            success_count += 1

    print(f"\n{'='*60}")
    print(f"Completed: {success_count}/{len(files)} files processed successfully")
    print(f"{'='*60}")

if __name__ == '__main__':
    main()
