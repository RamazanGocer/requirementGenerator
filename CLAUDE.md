# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Purpose

This repo is a **DOORS requirement generator** for embedded C projects. It parses C header (`.h`), source (`.c`), and assembly (`.s`) files and produces pipe-delimited `.txt` files that are then imported into IBM DOORS via DXL scripts to populate `DD_HEADER` and `SRS_LLR` modules.

## Running the Parser

```bash
# Parse all .h / .c / .s files in the current directory
python main.py .

# Parse a specific directory
python main.py path/to/source/

# Parse individual files
python main.py module.h module.c

# Custom output directory (default: output/)
python main.py -o out/ .
```

Python 3.8+ required. No third-party dependencies.

## Output Files

For each source file, `main.py` produces one txt in `output/`:

| Source | Output | DOORS module type |
|--------|--------|------------------|
| `foo.h` | `output/foo_DD_HEADER.txt` | DD_HEADER |
| `foo.c` | `output/foo_SRS_LLR.txt` | SRS_LLR |
| `foo.s` | `output/foo_SRS_LLR.txt` | SRS_LLR |

## Architecture

```
.h / .c / .s files
      |
   main.py
      |
  reqgen/
  ├── header_parser.py   → DD_HEADER data (defines, typedefs, enums, structs, unions, funcptrs, inlines)
  ├── source_parser.py   → SRS_LLR data (functions + inline __asm__ blocks)
  ├── asm_parser.py      → SRS_LLR data (standalone .s functions + data section symbols)
  ├── doxygen_parser.py  → comment extraction (doxygen @brief/@param/@return + plain /* */ )
  ├── size_calculator.py → C type → byte size (x86-64 / LP64)
  └── writer.py          → structured dicts → pipe-delimited txt files
      |
  output/foo_DD_HEADER.txt
  output/foo_SRS_LLR.txt
      |
  dxl/
  ├── dd_header_import.dxl   → reads DD_HEADER.txt, creates DOORS objects
  └── srs_llr_import.dxl     → reads SRS_LLR.txt, creates DOORS objects
```

## Txt File Format

Both files use `|`-delimited records. Lines starting with `#` are comments.

**DD_HEADER columns by record type:**

| Record | Fields |
|--------|--------|
| `SECTION` | Title |
| `DEFINE` / `DEFINE_FUNC` | Name \| Value \| DataType \| Size(B) \| ObjectText \| Description |
| `TYPEDEF` | Name \| AliasOf \| DataType \| Size(B) \| ObjectText \| Description |
| `ENUM` | Name \| DataType \| Size(B) \| ObjectText \| Description |
| `ENUM_VAL` | EnumName \| ValName \| Value |
| `STRUCT` / `UNION` | Name \| DataType \| Size(B) \| ObjectText \| Description |
| `STRUCT_FIELD` / `UNION_FIELD` | ParentName \| FieldName \| DataType \| Size(B) \| BitWidth |
| `FUNCPTR` | Name \| ReturnType \| Params \| Size(B) \| ObjectText \| Description |
| `INLINE` | Name \| ReturnType \| Params \| Size(B) \| ObjectText \| Description |

**SRS_LLR columns:**

| Record | Fields |
|--------|--------|
| `FUNC_HEADING` / `ASM_HEADING` / `DATA_HEADING` | FunctionName \| HeadingText \| false |
| `FUNC_REQ` / `FUNC_PARAM` / `FUNC_RETURN` / `ASM_REQ` / `DATA_REQ` | FunctionName \| RequirementText \| isReq(true/false) |

## DOORS Attributes Used

**DD_HEADER module:** `Object Heading`, `Object Text`, `Data Type`, `Size (Bytes)`, `Description`

**SRS_LLR module:** `Object Heading`, `Object Text`, `isReq`

## DXL Import Scripts

Both scripts in `dxl/` share the same structure:
1. Edit the `FILE_PATH` constant at the top of the script.
2. Open the target DOORS module.
3. Run via **Tools → Edit DXL...** (paste & run) or **Tools → Run DXL File...**.

The helper function `getField(line, n)` splits on `|` (ASCII 124) without relying on a built-in split, for maximum DXL compatibility.

## Doxygen Comment Support

The parser extracts descriptions from comments immediately preceding each C construct:
- **Doxygen format** (`/** @brief ... @param ... @return ... */`) — parsed into structured fields; `@param` entries become individual `FUNC_PARAM` requirement lines.
- **Plain block comment** (`/* ... */`) — first non-separator line used as description.
- **No comment** — description is derived from the identifier name (word-split on `_` and camelCase).

Adding doxygen comments to source files will automatically improve the generated requirement text on the next run.

## Sample C Files

The `sample.h` / `sample.c` / `sample_asm.s` files in the repo root serve as a **reference and test input** — they deliberately exercise every C11 and x86-64 construct so the parser can be validated against a known-good input.

```bash
# Compile the sample files (requires GCC / MinGW, -lm for math.h)
gcc -std=c11 -Wall -Wextra -o sample sample.c -lm
gcc -c sample_asm.s -o sample_asm.o

# Run
./sample
```

`sample.h` uses `C_VERSION 11`; the `HAS_C11` macro gates `_Noreturn`, `_Static_assert`, and the anonymous struct inside `Register`. Use `-std=gnu11` if GCC rejects inline asm under strict `-std=c11`.

### Key type aliases (`sample.h`)

`f32` = `float`, `f64` = `double`, `uint` = `unsigned int`, `u8`/`s8`/`u16`/`s32` map to `stdint.h` equivalents.

### Inline vs standalone assembly (`sample.c` vs `sample_asm.s`)

`sample.c` uses **GCC extended inline asm** (`__asm__ __volatile__("..." : out : in : clobbers)`) in AT&T syntax. `sample_asm.s` is a separate translation unit written in **Intel syntax** (`.intel_syntax noprefix`), following the System V AMD64 ABI. The parser detects inline `__asm__` blocks per-function and emits individual `ASM_REQ` lines for each distinct instruction mnemonic found.
