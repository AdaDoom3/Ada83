#!/usr/bin/env python3
"""
Comprehensive formatter for gnat-design.md - handles all the PDF artifacts properly.
"""

import re

def clean_gnat_design():
    filepath = '/home/user/Ada83/reference/gnat-design.md'

    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    output = []
    i = 0
    in_toc = False
    last_chapter = None

    while i < len(lines):
        line = lines[i].rstrip()

        # Skip standalone page numbers and roman numerals
        if re.match(r'^\s*[ivxlcdm]+\s*$', line, re.IGNORECASE) or re.match(r'^\s*\d+\s*$', line):
            i += 1
            continue

        # Skip "CONTENTS" markers
        if re.match(r'^CONTENTS(\s+[ivxlc]+)?$', line.strip(), re.IGNORECASE):
            i += 1
            continue

        # Title
        if i < 5 and 'GNAT: The GNU Ada Compiler' in line:
            output.append('# GNAT: The GNU Ada Compiler\n\n')
            i += 1
            continue

        # Date
        if i < 10 and re.match(r'^[A-Z][a-z]+,\s+\d{4}$', line.strip()):
            output.append(f'**{line.strip()}**\n\n')
            i += 1
            continue

        # Copyright/organizational info at start
        if i < 30 and (('Copyright' in line) or ('@' in line) or ('Permission is granted' in line)):
            output.append(line + '\n')
            i += 1
            continue

        # Table of Contents header
        if line.strip() == 'Contents':
            output.append('## Table of Contents\n\n')
            in_toc = True
            i += 1
            continue

        # Detect end of TOC
        if in_toc and (line.strip() in ['Acknowledgements', 'Preface'] or re.match(r'^Part [IVX]+$', line.strip())):
            in_toc = False

        # In TOC
        if in_toc:
            stripped = line.strip()
            # Part markers
            if re.match(r'^[IVX]+\s+.*Part', stripped):
                cleaned = re.sub(r'\s+\d+$', '', stripped)
                output.append(f'**{cleaned}**\n\n')
            # Chapters (no decimal)
            elif re.match(r'^(\d+)\s+(.+)$', stripped) and '.' not in stripped.split()[0]:
                m = re.match(r'^(\d+)\s+(.+)$', stripped)
                title = re.sub(r'\s+\d+$', '', m.group(2))
                output.append(f'{m.group(1)}. {title}\n')
            # Sections
            elif re.match(r'^(\d+\.\d+(?:\.\d+)*)\s+(.+)$', stripped):
                m = re.match(r'^(\d+\.\d+(?:\.\d+)*)\s+(.+)$', stripped)
                indent = '   ' * m.group(1).count('.')
                title = re.sub(r'\s+\d+$', '', m.group(2))
                output.append(f'{indent}{m.group(1)} {title}\n')
            i += 1
            continue

        # Special sections
        if re.match(r'^(Acknowledgements|Preface|Short Biography)(\s+\d+)?$', line.strip()):
            section_name = re.match(r'^(Acknowledgements|Preface|Short Biography)', line.strip()).group(1)
            output.append(f'\n## {section_name}\n\n')
            i += 1
            continue

        # Part headers
        if re.match(r'^Part ([IVX]+)$', line.strip()):
            m = re.match(r'^Part ([IVX]+)$', line.strip())
            part_num = m.group(1)
            # Look ahead for title (skip page numbers)
            title = ''
            j = i + 1
            while j < len(lines) and j < i + 5:
                next_line = lines[j].strip()
                if next_line and not re.match(r'^\d+$', next_line):
                    # Clean up title
                    title = re.sub(r'^(First|Second|Third|Fourth|Fifth)\s+Part:\s*', '', next_line)
                    title = re.sub(r'\s+\d+$', '', title)
                    break
                j += 1
            output.append(f'\n# Part {part_num}: {title}\n\n')
            i = j + 1 if title else i + 1
            continue

        # Chapter headers - "Chapter N"
        if re.match(r'^Chapter\s+(\d+)$', line.strip()):
            m = re.match(r'^Chapter\s+(\d+)$', line.strip())
            chapter_num = m.group(1)
            last_chapter = chapter_num
            # Look ahead for title
            title = ''
            j = i + 1
            while j < len(lines) and j < i + 5:
                next_line = lines[j].strip()
                if next_line and not re.match(r'^\d+$', next_line):
                    title = re.sub(r'\s+\d+$', '', next_line)
                    break
                j += 1
            output.append(f'\n## Chapter {chapter_num}: {title}\n\n')
            i = j + 1 if title else i + 1
            continue

        # CHAPTER N. TITLE format - skip if we just saw Chapter N
        if re.match(r'^CHAPTER\s+(\d+)\.\s+(.+)$', line.strip()):
            m = re.match(r'^CHAPTER\s+(\d+)\.\s+(.+)$', line.strip())
            chapter_num = m.group(1)
            # Skip if we just processed this chapter
            if chapter_num == last_chapter:
                i += 1
                continue
            # Otherwise process it
            title = m.group(2).title()
            last_chapter = chapter_num
            output.append(f'\n## Chapter {chapter_num}: {title}\n\n')
            i += 1
            continue

        # Section headers X.Y
        if re.match(r'^(\d+\.\d+)\s+(.+)$', line.strip()):
            m = re.match(r'^(\d+\.\d+)\s+(.+)$', line.strip())
            if m.group(1).count('.') == 1:  # Only X.Y, not X.Y.Z
                output.append(f'\n### {m.group(1)} {m.group(2)}\n\n')
                i += 1
                continue

        # Subsections X.Y.Z
        if re.match(r'^(\d+\.\d+\.\d+)\s+(.+)$', line.strip()):
            m = re.match(r'^(\d+\.\d+\.\d+)\s+(.+)$', line.strip())
            if m.group(1).count('.') == 2:
                output.append(f'\n#### {m.group(1)} {m.group(2)}\n\n')
                i += 1
                continue

        # Sub-subsections X.Y.Z.W
        if re.match(r'^(\d+\.\d+\.\d+\.\d+)\s+(.+)$', line.strip()):
            m = re.match(r'^(\d+\.\d+\.\d+\.\d+)\s+(.+)$', line.strip())
            output.append(f'\n##### {m.group(1)} {m.group(2)}\n\n')
            i += 1
            continue

        # Figure captions
        if re.match(r'^Figure\s+\d+\.\d+:', line.strip()):
            output.append(f'\n**{line.strip()}**\n\n')
            i += 1
            continue

        # Regular line
        output.append(line + '\n')
        i += 1

    # Clean up excessive blank lines
    content = ''.join(output)
    while '\n\n\n\n' in content:
        content = content.replace('\n\n\n\n', '\n\n\n')

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)

    print("✓ Formatting complete!")

if __name__ == '__main__':
    clean_gnat_design()
