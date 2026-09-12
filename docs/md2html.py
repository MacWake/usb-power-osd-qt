#!/usr/bin/env python3
"""Minimal Markdown-to-HTML converter for the user manual.

Only handles the constructs used in docs/USER_MANUAL.md:
- ATX-style headers (#, ##, ###)
- Paragraphs
- Bold / italic inline markup (**text**, *text*)
- Unordered lists (- and *)
- Ordered lists (1.)
- Links [text](url)
- Code / inline code (`text`)
- Tables (GitHub-flavored)
"""

import html
import re
import sys
from pathlib import Path


def escape(text: str) -> str:
    return html.escape(text)


def inline_markup(text: str) -> str:
    # code spans
    text = re.sub(r'`([^`]+)`', r'<code>\1</code>', text)
    # bold
    text = re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', text)
    text = re.sub(r'__(.+?)__', r'<strong>\1</strong>', text)
    # italic
    text = re.sub(r'\*(.+?)\*', r'<em>\1</em>', text)
    text = re.sub(r'_(.+?)_', r'<em>\1</em>', text)
    # links
    text = re.sub(r'\[([^\]]+)\]\(([^)]+)\)', r'<a href="\2">\1</a>', text)
    return text


def convert(md: str) -> str:
    lines = md.splitlines()
    out = []
    out.append('<!DOCTYPE html>')
    out.append('<html lang="en">')
    out.append('<head>')
    out.append('  <meta charset="UTF-8">')
    out.append('  <title>USB Power OSD — User Manual</title>')
    out.append('  <style>')
    out.append('    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; line-height: 1.6; max-width: 800px; margin: 2em auto; padding: 0 1em; color: #222; }')
    out.append('    h1 { border-bottom: 2px solid #ddd; padding-bottom: 0.3em; }')
    out.append('    h2 { border-bottom: 1px solid #eee; padding-bottom: 0.2em; margin-top: 1.5em; }')
    out.append('    h3 { margin-top: 1.2em; }')
    out.append('    table { border-collapse: collapse; width: 100%; margin: 1em 0; }')
    out.append('    th, td { border: 1px solid #ddd; padding: 0.5em; text-align: left; }')
    out.append('    th { background-color: #f6f6f6; }')
    out.append('    code { background-color: #f4f4f4; padding: 0.1em 0.3em; border-radius: 3px; }')
    out.append('    pre { background-color: #f4f4f4; padding: 1em; overflow-x: auto; border-radius: 4px; }')
    out.append('    a { color: #0366d6; text-decoration: none; }')
    out.append('    a:hover { text-decoration: underline; }')
    out.append('  </style>')
    out.append('</head>')
    out.append('<body>')

    i = 0
    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        # Headers
        m = re.match(r'^(#{1,6})\s+(.*)$', stripped)
        if m:
            level = len(m.group(1))
            content = inline_markup(escape(m.group(2).strip()))
            out.append(f'<h{level}>{content}</h{level}>')
            i += 1
            continue

        # Horizontal rule
        if stripped == '---' or stripped == '***':
            out.append('<hr>')
            i += 1
            continue

        # Empty lines
        if not stripped:
            i += 1
            continue

        # Tables
        if '|' in stripped:
            # collect table block
            table_lines = []
            while i < len(lines) and '|' in lines[i].strip():
                table_lines.append(lines[i].strip())
                i += 1
            if len(table_lines) >= 2 and re.match(r'^\|?[-:|\s]+\|?[-:|\s|\|]*$', table_lines[1]):
                # header
                header_cells = [c.strip() for c in table_lines[0].split('|')]
                header_cells = [c for c in header_cells if c or len(header_cells) > 2]
                out.append('<table>')
                out.append('  <thead>')
                out.append('    <tr>')
                for cell in header_cells:
                    out.append(f'      <th>{inline_markup(escape(cell))}</th>')
                out.append('    </tr>')
                out.append('  </thead>')
                out.append('  <tbody>')
                for row in table_lines[2:]:
                    cells = [c.strip() for c in row.split('|')]
                    cells = [c for c in cells if c or len(cells) > 2]
                    if not cells:
                        continue
                    out.append('    <tr>')
                    for cell in cells:
                        out.append(f'      <td>{inline_markup(escape(cell))}</td>')
                    out.append('    </tr>')
                out.append('  </tbody>')
                out.append('</table>')
                continue
            else:
                # not a table, treat first line as paragraph and continue processing rest
                out.append(f'<p>{inline_markup(escape(stripped))}</p>')
                # put remaining collected lines back by adjusting index? We already consumed them.
                # For simplicity, if it's not a table we just paragraphize each line.
                for tl in table_lines[1:]:
                    if tl.strip():
                        out.append(f'<p>{inline_markup(escape(tl.strip()))}</p>')
                continue

        # Unordered list
        if re.match(r'^[-*]\s+', stripped):
            out.append('<ul>')
            while i < len(lines) and re.match(r'^[-*]\s+', lines[i].strip()):
                content = re.sub(r'^[-*]\s+', '', lines[i].strip())
                out.append(f'  <li>{inline_markup(escape(content))}</li>')
                i += 1
            out.append('</ul>')
            continue

        # Ordered list
        if re.match(r'^\d+\.\s+', stripped):
            out.append('<ol>')
            while i < len(lines) and re.match(r'^\d+\.\s+', lines[i].strip()):
                content = re.sub(r'^\d+\.\s+', '', lines[i].strip())
                out.append(f'  <li>{inline_markup(escape(content))}</li>')
                i += 1
            out.append('</ol>')
            continue

        # Blockquote
        if stripped.startswith('>'):
            out.append('<blockquote>')
            while i < len(lines) and lines[i].strip().startswith('>'):
                content = re.sub(r'^>\s?', '', lines[i].strip())
                out.append(f'  <p>{inline_markup(escape(content))}</p>')
                i += 1
            out.append('</blockquote>')
            continue

        # Regular paragraph
        out.append(f'<p>{inline_markup(escape(stripped))}</p>')
        i += 1

    out.append('</body>')
    out.append('</html>')
    return '\n'.join(out) + '\n'


def main():
    if len(sys.argv) != 3:
        print(f'Usage: {sys.argv[0]} <input.md> <output.html>', file=sys.stderr)
        sys.exit(1)
    in_path = Path(sys.argv[1])
    out_path = Path(sys.argv[2])
    out_path.write_text(convert(in_path.read_text(encoding='utf-8')), encoding='utf-8')
    print(f'Wrote {out_path}')


if __name__ == '__main__':
    main()
