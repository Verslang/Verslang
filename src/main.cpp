#include "compiler/compiler.h"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

using namespace verslang;

// ═══════════════════════════════════════════════════════════════
// Verslang Compiler — CLI Entry Point
// ═══════════════════════════════════════════════════════════════
//
//   verslang <input.vlang> [options]
//
//   Options:
//     -o <file>          Output file path
//     --format <fmt>     Output format: elf, pe, bin, boot, ast, hex
//     --target <arch>    Target architecture (x86_64)
//     --conv <cc>        Calling convention: sysv, win64
//     -v, --verbose      Verbose output
//     --dump-ast         Dump AST to stdout
//     --dump-code        Dump generated machine code hex
//     -h, --help         Show help
//     --version          Show version
//
// ═══════════════════════════════════════════════════════════════

static const char* VERSION = "0.1.0";

void printHelp() {
    std::cout << R"(
╔══════════════════════════════════════════════════════════════════╗
║  Verslang — Low-Level Systems Programming Language Compiler     ║
║  Version )" << VERSION << R"(                                                  ║
╚══════════════════════════════════════════════════════════════════╝

Usage: verslang <input> [options]

Options:
  -o <file>          Output file (default: based on format)
  --format <fmt>     Output format:
                       pe    - Windows executable (default on Windows)
                       elf   - Linux ELF64 executable
                       bin   - Flat binary
                       boot  - 512-byte boot sector
                       ast   - Dump AST (debug)
                       hex   - Dump machine code hex (debug)
  --conv <cc>        Calling convention:
                       win64 - Windows x64 (default on Windows)
                       sysv  - System V AMD64 (default on Linux)
  -v, --verbose      Verbose compilation output
  --dump-ast         Print AST tree
  --dump-code        Print generated machine code
  -h, --help         Show this help
  --version          Show version

Supported file extensions: .vlang .verslang .lang .language .kerlang

Examples:
  verslang hello.vlang                      Compile to hello.exe (Windows)
  verslang hello.vlang --format elf -o hello Compile to ELF (Linux)
  verslang boot.vlang --format boot         Create boot sector
  verslang program.vlang --format ast       Dump AST tree
  verslang program.vlang -v --dump-code     Verbose + hex dump

)" << std::endl;
}

void printVersion() {
    std::cout << "verslang " << VERSION << std::endl;
    std::cout << "Target: x86-64" << std::endl;
    std::cout << "Formats: PE64 (Windows), ELF64 (Linux), binary, boot sector" << std::endl;
}

std::string inferOutputFile(const std::string& input, OutputFormat format) {
    std::filesystem::path p(input);
    std::string stem = p.stem().string();

    switch (format) {
        case OutputFormat::PE64:         return stem + ".exe";
        case OutputFormat::ELF64:        return stem;
        case OutputFormat::FLAT_BINARY:  return stem + ".bin";
        case OutputFormat::BOOT_SECTOR:  return stem + ".img";
        default:                         return stem + ".out";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printHelp();
        return 1;
    }

    CompilerOptions opts;
    std::vector<std::string> args;
    for (int i = 1; i < argc; i++) args.push_back(argv[i]);

    // Parse arguments
    for (size_t i = 0; i < args.size(); i++) {
        const std::string& arg = args[i];

        if (arg == "-h" || arg == "--help") {
            printHelp();
            return 0;
        }
        if (arg == "--version") {
            printVersion();
            return 0;
        }
        if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
            continue;
        }
        if (arg == "--dump-ast") {
            opts.dumpAst = true;
            continue;
        }
        if (arg == "--dump-code") {
            opts.dumpCode = true;
            continue;
        }
        if (arg == "-o" && i + 1 < args.size()) {
            opts.outputFile = args[++i];
            continue;
        }
        if (arg == "--format" && i + 1 < args.size()) {
            std::string fmt = args[++i];
            if (fmt == "elf" || fmt == "elf64") opts.format = OutputFormat::ELF64;
            else if (fmt == "pe" || fmt == "exe" || fmt == "pe64") opts.format = OutputFormat::PE64;
            else if (fmt == "bin" || fmt == "binary" || fmt == "flat") opts.format = OutputFormat::FLAT_BINARY;
            else if (fmt == "boot" || fmt == "bootsector") opts.format = OutputFormat::BOOT_SECTOR;
            else if (fmt == "ast") opts.format = OutputFormat::AST_DUMP;
            else if (fmt == "hex" || fmt == "code") opts.format = OutputFormat::CODE_DUMP;
            else {
                std::cerr << "Unknown format: " << fmt << std::endl;
                return 1;
            }
            continue;
        }
        if (arg == "--gui") {
            opts.guiSubsystem = true;
            continue;
        }
        if (arg == "--conv" && i + 1 < args.size()) {
            std::string cc = args[++i];
            if (cc == "sysv" || cc == "linux") opts.callingConv = CallingConv::SYSV_AMD64;
            else if (cc == "win64" || cc == "windows") opts.callingConv = CallingConv::WIN64;
            else {
                std::cerr << "Unknown calling convention: " << cc << std::endl;
                return 1;
            }
            continue;
        }

        // Input file
        if (opts.inputFile.empty()) {
            opts.inputFile = arg;
        } else {
            std::cerr << "Unexpected argument: " << arg << std::endl;
            return 1;
        }
    }

    // Validate
    if (opts.inputFile.empty()) {
        std::cerr << "Error: No input file specified" << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(opts.inputFile)) {
        std::cerr << "Error: File not found: " << opts.inputFile << std::endl;
        return 1;
    }

    // Auto-detect format based on output file extension if not specified
    if (!opts.outputFile.empty()) {
        std::filesystem::path p(opts.outputFile);
        std::string ext = p.extension().string();
        if (ext == ".exe" && opts.format != OutputFormat::PE64) {
            // Don't override explicit --format
        }
    }

    // Infer output file if not given
    if (opts.outputFile.empty()) {
        opts.outputFile = inferOutputFile(opts.inputFile, opts.format);
    }

    // Compile
    Compiler compiler(opts);
    return compiler.compile() ? 0 : 1;
}
