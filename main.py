#!/usr/bin/env python3
"""
requirementGenerator — Entry point.

Usage:
    python main.py                     # parse current directory
    python main.py path/to/dir         # parse all .h/.c/.s in directory
    python main.py sample.h sample.c   # parse specific files
    python main.py -o out/ sample.h    # custom output directory
"""
import argparse
import sys
from pathlib import Path

from reqgen.header_parser import parse_header
from reqgen.source_parser import parse_source
from reqgen.asm_parser import parse_asm
from reqgen.writer import write_dd_header, write_srs_llr, write_srs_llr_asm


def process_file(path: Path, output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)

    if path.suffix == '.h':
        parsed = parse_header(path)
        out = output_dir / f'DD_{path.stem.upper()}.txt'
        write_dd_header(parsed, out)
        _report('DD ', path.name, out.name, parsed)

    elif path.suffix == '.c':
        parsed = parse_source(path)
        out = output_dir / f'SRS_LLR_{path.stem.upper()}.txt'
        write_srs_llr(parsed, out)
        _report('SRS', path.name, out.name, parsed)

    elif path.suffix == '.s':
        parsed = parse_asm(path)
        out = output_dir / f'SRS_LLR_{path.stem.upper()}.txt'
        write_srs_llr_asm(parsed, out)
        _report('ASM', path.name, out.name, parsed)

    else:
        print(f'  [skip] {path.name} — unsupported extension')


def _report(tag: str, src: str, dst: str, parsed: dict) -> None:
    funcs = len(parsed.get('functions', []))
    defines = len(parsed.get('defines', []))
    structs = (len(parsed.get('structs', [])) + len(parsed.get('enums', [])) +
               len(parsed.get('unions', [])))
    detail = []
    if funcs:
        detail.append(f'{funcs} functions')
    if defines:
        detail.append(f'{defines} defines')
    if structs:
        detail.append(f'{structs} types')
    info = ', '.join(detail) if detail else 'no items'
    print(f'  [{tag}] {src} -> {dst}  ({info})')


def main() -> None:
    ap = argparse.ArgumentParser(
        description='Generate DOORS DD_HEADER and SRS_LLR txt files from C/ASM source.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    ap.add_argument(
        'paths', nargs='*', default=['.'],
        help='C source files (.h/.c/.s) or directories to parse (default: current dir)',
    )
    ap.add_argument(
        '-o', '--output', default='output',
        help='Output directory for generated txt files (default: output/)',
    )
    args = ap.parse_args()

    output_dir = Path(args.output)
    processed = 0

    for path_str in args.paths:
        path = Path(path_str)

        if path.is_dir():
            files = sorted(
                [f for f in path.iterdir() if f.suffix in ('.h', '.c', '.s')],
                key=lambda f: (f.suffix, f.name),
            )
            if not files:
                print(f'No .h/.c/.s files found in {path}')
                continue
            print(f'Processing directory: {path}')
            for f in files:
                process_file(f, output_dir)
                processed += 1

        elif path.is_file():
            process_file(path, output_dir)
            processed += 1

        else:
            print(f'Warning: path not found: {path}', file=sys.stderr)

    print(f'\nDone. {processed} file(s) processed -> {output_dir}/')


if __name__ == '__main__':
    main()
