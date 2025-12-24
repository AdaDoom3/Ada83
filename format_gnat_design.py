#!/usr/bin/env python3
"""
Comprehensive formatter for gnat-design.md to convert it to proper markdown.
"""

import re
import sys

def is_page_number_line(line):
    """Check if line is just a page number."""
    return re.match(r'^\s*\d+\s*$', line) is not None

def is_roman_numeral_line(line):
    """Check if line is just a roman numeral page marker."""
    return re.match(r'^\s*[ivxlcdm]+\s*$', line, re.IGNORECASE) is not None

def strip_page_number(text):
    """Remove trailing page numbers from text."""
    # Remove numbers at the end, but be careful about section numbers
    return re.sub(r'\s+\d+\s*$', '', text)

def detect_toc_end(lines, current_index):
    """Detect when TOC ends by looking for content start patterns."""
    # Look ahead for Part I marker or "Acknowledgements" or similar
    for j in range(current_index, min(current_index + 20, len(lines))):
        line = lines[j].strip()
        if line.startswith('Part I') or line == 'Acknowledgements' or line == 'Preface':
            return j
        if line.startswith('Chapter 1') and 'GNAT Project' in line:
            return j
    return None

def format_document(content):
    """Apply comprehensive markdown formatting."""
    lines = content.split('\n')
    result = []
    i = 0
    in_toc = False
    toc_end_index = None

    # First pass: find TOC boundaries
    for idx, line in enumerate(lines):
        if line.strip() == 'Contents':
            in_toc_temp = True
            # Look for where TOC ends
            for j in range(idx + 1, min(idx + 300, len(lines))):
                if lines[j].strip() in ['Acknowledgements', 'Preface']:
                    toc_end_index = j
                    break
                if re.match(r'^Part [IVX]+$', lines[j].strip()):
                    toc_end_index = j
                    break
            break

    in_toc = False
    while i < len(lines):
        line = lines[i]

        # Skip orphaned page markers
        if is_page_number_line(line) or is_roman_numeral_line(line):
            i += 1
            continue

        # Skip "CONTENTS" markers (including with page numbers)
        if re.match(r'^CONTENTS\s+[ivxlc]+\s*$', line.strip(), re.IGNORECASE):
            i += 1
            continue
        if line.strip() == 'CONTENTS':
            i += 1
            continue

        # Main title
        if i < 5 and 'GNAT: The GNU Ada Compiler' in line:
            result.append('# GNAT: The GNU Ada Compiler')
            result.append('')
            i += 1
            continue

        # Date
        if i < 10 and re.match(r'^[A-Z][a-z]+,\s+\d{4}$', line.strip()):
            result.append(f'**{line.strip()}**')
            result.append('')
            i += 1
            continue

        # Copyright and organization info
        if i < 30 and ('Copyright' in line or '@' in line or 'Permission is granted' in line):
            result.append(line)
            i += 1
            continue

        # Table of Contents
        if line.strip() == 'Contents':
            result.append('## Table of Contents')
            result.append('')
            in_toc = True
            i += 1
            continue

        # Check if we've reached end of TOC
        if toc_end_index and i >= toc_end_index:
            in_toc = False

        # In TOC mode
        if in_toc:
            stripped = line.strip()

            # Part markers in TOC
            if re.match(r'^[IVX]+\s+.*Part', stripped):
                result.append(f'**{strip_page_number(stripped)}**')
                result.append('')
                i += 1
                continue

            # Chapter entries (just number and title)
            chapter_toc = re.match(r'^(\d+)\s+(.+)$', stripped)
            if chapter_toc and not '.' in chapter_toc.group(1):
                chapter_num = chapter_toc.group(1)
                chapter_title = strip_page_number(chapter_toc.group(2))
                result.append(f'{chapter_num}. {chapter_title}')
                i += 1
                continue

            # Section/subsection entries (X.Y or X.Y.Z)
            section_toc = re.match(r'^(\d+(?:\.\d+)+)\s+(.+)$', stripped)
            if section_toc:
                section_num = section_toc.group(1)
                section_title = strip_page_number(section_toc.group(2))
                indent_level = section_num.count('.')
                indent = '   ' * indent_level
                result.append(f'{indent}{section_num} {section_title}')
                i += 1
                continue

        # Not in TOC - format actual content

        # Special sections (with possible page numbers)
        special_section_match = re.match(r'^(Acknowledgements|Preface|Short Biography)(?:\s+\d+)?$', line.strip())
        if special_section_match:
            result.append('')
            result.append(f'## {special_section_match.group(1)}')
            result.append('')
            i += 1
            continue

        # Part headers
        part_match = re.match(r'^Part ([IVX]+)$', line.strip())
        if part_match:
            # Look ahead for title
            title = ''
            if i + 1 < len(lines):
                next_line = lines[i + 1].strip()
                # Clean up the title (remove "Part:" prefix if it exists)
                title = re.sub(r'^(First|Second|Third|Fourth|Fifth)\s+Part:\s*', '', next_line)
                title = strip_page_number(title)
                i += 1
            result.append('')
            result.append(f'# Part {part_match.group(1)}: {title}')
            result.append('')
            i += 1
            continue

        # Chapter headers (various formats)
        chapter_match = re.match(r'^Chapter\s+(\d+)$', line.strip())
        if chapter_match:
            # Look ahead for title
            title = ''
            if i + 1 < len(lines):
                title = strip_page_number(lines[i + 1].strip())
                i += 1
            result.append('')
            result.append(f'## Chapter {chapter_match.group(1)}: {title}')
            result.append('')
            i += 1
            continue

        # CHAPTER X. TITLE format
        chapter_caps = re.match(r'^CHAPTER\s+(\d+)\.\s+(.+)$', line.strip())
        if chapter_caps:
            result.append('')
            result.append(f'## Chapter {chapter_caps.group(1)}: {chapter_caps.group(2).title()}')
            result.append('')
            i += 1
            continue

        # Section headers X.Y
        section_match = re.match(r'^(\d+\.\d+)\s+(.+)$', line.strip())
        if section_match and not section_match.group(1).count('.') > 1:
            result.append('')
            result.append(f'### {section_match.group(1)} {section_match.group(2)}')
            result.append('')
            i += 1
            continue

        # Subsection headers X.Y.Z
        subsection_match = re.match(r'^(\d+\.\d+\.\d+)\s+(.+)$', line.strip())
        if subsection_match and subsection_match.group(1).count('.') == 2:
            result.append('')
            result.append(f'#### {subsection_match.group(1)} {subsection_match.group(2)}')
            result.append('')
            i += 1
            continue

        # Deeper subsections
        subsubsection_match = re.match(r'^(\d+\.\d+\.\d+\.\d+)\s+(.+)$', line.strip())
        if subsubsection_match:
            result.append('')
            result.append(f'##### {subsubsection_match.group(1)} {subsubsection_match.group(2)}')
            result.append('')
            i += 1
            continue

        # Figure captions
        if re.match(r'^Figure\s+\d+\.\d+:', line.strip()):
            result.append('')
            result.append(f'**{line.strip()}**')
            result.append('')
            i += 1
            continue

        # Regular content line
        result.append(line)
        i += 1

    return '\n'.join(result)

def clean_extra_blanks(content):
    """Clean up excessive blank lines."""
    # Remove more than 2 consecutive blank lines
    while '\n\n\n\n' in content:
        content = content.replace('\n\n\n\n', '\n\n\n')
    return content

def main():
    filepath = '/home/user/Ada83/reference/gnat-design.md'

    print(f"Reading {filepath}...")
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    print("Formatting document structure...")
    formatted = format_document(content)

    print("Cleaning up blank lines...")
    formatted = clean_extra_blanks(formatted)

    print(f"Writing formatted content...")
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(formatted)

    print("✓ Done!")

if __name__ == '__main__':
    main()
