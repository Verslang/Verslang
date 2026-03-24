# Verslang

**Low-Level Systems Programming Language**

Verslang is a low-level systems programming language designed for kernel development, bootloaders, OS construction, and bare-metal programming. It compiles directly to x86-64 machine code and produces native executables (ELF64, PE32+), raw binaries, and boot sectors.

> Written like Versscript — but kernel-level.

## Features

- **Direct machine code emission** — No assembler or linker needed. Verslang compiles straight to binary.
- **Multiple output formats** — ELF64 (Linux), PE32+ (Windows .exe), flat binary, 512-byte boot sectors
- **Inline assembly** — Full x86-64 assembly blocks with `assembly { }`
- **System calls** — Direct `syscall()` with register-level control
- **Port I/O** — `port.input_byte()` / `port.output_byte()` for hardware access
- **Memory operations** — `memory.read_u8()` / `memory.write_u8()` / `memory.allocate()` / `memory.free()`
- **Register access** — `register.rax`, `register.rbx`, etc.
- **Interrupt handlers** — `interrupt_handler keyboard() { }`
- **Boot sector support** — `directive bootloader` produces 512-byte images with 0xAA55 signature
- **Fixed-width types** — `u8`, `u16`, `u32`, `u64`, `i8`–`i64`, `f32`, `f64`, `ptr<T>`, `array<T, N>`
- **Structures, enums, unions** — `structure`, `enumeration`, `union`
- **Explicit everything** — `declare`, `constant`, `mutable`, `address_of()`, `dereference()`, `cast<T>()`
- **Calling conventions** — System V AMD64 (Linux) and Win64 (Windows)
- **Conditional compilation** — `when platform == "windows" { }`

## File Extensions

`.vlang` `.verslang` `.lang` `.language` `.kerlang`

## Quick Start

### Build the Compiler

```batch
build.bat
```

Requires Visual Studio 2022 with C++20 support. Produces `build/verslang.exe`.

### Compile a Program

```bash
# Linux ELF executable
verslang examples/hello.vlang --format elf -o hello

# Windows PE executable
verslang examples/fibonacci.vlang --format pe -o fibonacci.exe

# Boot sector image (test with QEMU)
verslang examples/bootloader.vlang --format boot -o boot.img

# Dump AST for debugging
verslang program.vlang --format ast

# Dump generated machine code
verslang program.vlang --format hex -v
```

### CLI Options

| Option | Description |
|--------|-------------|
| `-o <file>` | Output file path |
| `--format <fmt>` | `elf`, `pe`, `bin`, `boot`, `ast`, `hex` |
| `--conv <cc>` | `sysv` (Linux) or `win64` (Windows) |
| `-v, --verbose` | Verbose compilation output |
| `--dump-ast` | Print AST tree |
| `--dump-code` | Print generated machine code |
| `-h, --help` | Show help |
| `--version` | Show version |

## Language Reference

### Variables

```verslang
declare x: u32 = 42          // Typed variable
constant PI: f64 = 3.14159   // Immutable constant
mutable counter: u32 = 0     // Explicitly mutable
```

### Types

| Type | Size | Description |
|------|------|-------------|
| `u8`, `u16`, `u32`, `u64` | 1–8 bytes | Unsigned integers |
| `i8`, `i16`, `i32`, `i64` | 1–8 bytes | Signed integers |
| `f32`, `f64` | 4–8 bytes | Floating point |
| `bool` | 1 byte | Boolean |
| `char` | 1 byte | Character |
| `void` | 0 | No value |
| `usize`, `isize` | 8 bytes | Platform-sized integers |
| `ptr<T>` | 8 bytes | Pointer to T |
| `array<T, N>` | N×sizeof(T) | Fixed-size array |

### Functions

```verslang
function add(a: i32, b: i32) -> i32 {
    return a + b
}

export function public_api() -> void { }
external function printf(fmt: ptr<char>, ...) -> i32
inline function fast_op(x: u64) -> u64 { return x * 2 }
```

### Structures & Enums

```verslang
structure Point {
    x: f64
    y: f64
}

enumeration Color {
    RED = 0
    GREEN = 1
    BLUE = 2
}

union Value {
    integer: i64
    floating: f64
    pointer: ptr<void>
}
```

### Control Flow

```verslang
if (x > 0) { } else { }

while (running) { }

for (declare i: u32 = 0; i < 10; i = i + 1) { }

loop { if (done) { break } }

switch (value) {
    case 1: { }
    case 2: { }
    default: { }
}

goto error_handler
label error_handler
```

### Pointers & Memory

```verslang
declare p: ptr<u32> = address_of(x)
declare val: u32 = dereference(p)
declare wide: u64 = cast<u64>(narrow)
declare size: u64 = sizeof(Point)

// Raw memory operations
declare buf: ptr<u8> = memory.allocate(1024)
memory.write_u8(buf, 0, 0x41)
declare byte: u8 = memory.read_u8(buf, 0)
memory.free(buf)
```

### Inline Assembly

```verslang
assembly {
    mov rax, 60       // syscall number (exit)
    xor rdi, rdi      // exit code 0
    syscall
}
```

### System Calls (Linux)

```verslang
// syscall(number, arg1, arg2, ...)
// Returns result in RAX

// sys_write(stdout, message, length)
syscall(1, 1, msg, 17)

// sys_exit(0)
syscall(60, 0)
```

### Port I/O (Kernel Mode)

```verslang
// Read from hardware port
declare scancode: u8 = port.input_byte(0x60)

// Write to hardware port
port.output_byte(0x20, 0x20)    // ACK PIC
```

### Register Access

```verslang
declare cr0: u64 = register.cr0
register.rax = 0x1234
```

### Boot Sector

```verslang
directive bootloader

function _start() -> void {
    assembly {
        cli
        mov ah, 0x0E
        mov al, 0x56    // 'V'
        int 0x10
        hlt
    }
}
```

Compiles to exactly 512 bytes with boot signature `0xAA55`.

### Modules

```verslang
module math {
    export function abs(x: i64) -> i64 {
        if (x < 0) { return 0 - x }
        return x
    }
}

import "other_file.vlang"
```

### Conditional Compilation

```verslang
when platform == "windows" {
    // Windows-specific code
}
when platform == "linux" {
    // Linux-specific code
}
```

## Architecture

```
Source (.vlang)
    │
    ▼
┌─────────┐    ┌──────────┐    ┌──────────────┐    ┌──────────────┐
│  Lexer   │───▶│  Parser  │───▶│ x86-64 Code  │───▶│ Format Writer │
│ token.h  │    │  ast.h   │    │  Generator   │    │ (ELF/PE/bin) │
│ lexer.cpp│    │parser.cpp│    │ x86_64.cpp   │    │ elf.h, pe.h  │
└─────────┘    └──────────┘    └──────────────┘    └──────────────┘
                                                          │
                                                          ▼
                                                    Native Binary
                                                   (.exe / ELF / .bin / .img)
```

### Source Files

| File | Lines | Description |
|------|-------|-------------|
| `src/lexer/token.h` | ~250 | Token types and Token struct |
| `src/lexer/lexer.h/cpp` | ~350 | Lexer with keyword table |
| `src/parser/ast.h` | ~450 | Type system + AST node definitions |
| `src/parser/parser.h/cpp` | ~850 | Recursive descent parser |
| `src/codegen/x86_64.h/cpp` | ~1400 | x86-64 machine code generator |
| `src/codegen/elf.h` | ~180 | ELF64 format writer |
| `src/codegen/pe.h` | ~210 | PE32+ format writer |
| `src/compiler/compiler.h` | ~220 | Compiler driver |
| `src/main.cpp` | ~160 | CLI entry point |
| **Total** | **~4000+** | |

## Output Formats

| Format | Flag | Description |
|--------|------|-------------|
| PE32+ | `--format pe` | Windows 64-bit executable |
| ELF64 | `--format elf` | Linux 64-bit executable |
| Flat Binary | `--format bin` | Raw machine code |
| Boot Sector | `--format boot` | 512 bytes with 0xAA55 |
| AST Dump | `--format ast` | Debug: print syntax tree |
| Code Dump | `--format hex` | Debug: print machine code |

## Design Goals

1. **Lower than C** — Explicit types, manual memory, direct hardware access
2. **Readable like Versscript** — Familiar syntax with clear keywords
3. **Self-contained** — No external assembler, linker, or runtime needed
4. **OS-capable** — Build bootloaders, kernels, and bare-metal programs
5. **Self-hosting** — Eventually compile Verslang in Verslang

## License

MIT
