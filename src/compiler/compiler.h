#pragma once
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../codegen/x86_64.h"
#include "../codegen/elf.h"
#include "../codegen/pe.h"
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace verslang {

// ═══════════════════════════════════════════════════════════════
// Output format
// ═══════════════════════════════════════════════════════════════

enum class OutputFormat {
    ELF64,          // Linux executable
    PE64,           // Windows executable (.exe)
    FLAT_BINARY,    // Raw binary
    BOOT_SECTOR,    // 512-byte boot sector
    AST_DUMP,       // Debug: dump AST
    CODE_DUMP,      // Debug: dump generated machine code
};

// ═══════════════════════════════════════════════════════════════
// Compiler options
// ═══════════════════════════════════════════════════════════════

struct CompilerOptions {
    std::string inputFile;
    std::string outputFile;
    OutputFormat format = OutputFormat::PE64;
    CallingConv callingConv = CallingConv::WIN64;
    bool verbose = false;
    bool dumpAst = false;
    bool dumpCode = false;
    bool guiSubsystem = false;  // --gui flag: PE subsystem 2 (GUI) instead of 3 (CUI)
};

// ═══════════════════════════════════════════════════════════════
// Compiler — ties lexer → parser → codegen → output together
// ═══════════════════════════════════════════════════════════════

class Compiler {
public:
    explicit Compiler(const CompilerOptions& opts) : opts_(opts) {}

    bool compile() {
        try {
            // 1. Read source file
            if (opts_.verbose) std::cout << "[1/5] Reading source: " << opts_.inputFile << std::endl;
            std::string source = readFile(opts_.inputFile);
            if (source.empty()) {
                std::cerr << "Error: Could not read file '" << opts_.inputFile << "'" << std::endl;
                return false;
            }

            // 2. Lexer
            if (opts_.verbose) std::cout << "[2/5] Tokenizing..." << std::endl;
            Lexer lexer(source, opts_.inputFile);
            auto tokens = lexer.tokenize();
            if (opts_.verbose) std::cout << "  " << tokens.size() << " tokens produced" << std::endl;

            // 3. Parser
            if (opts_.verbose) std::cout << "[3/5] Parsing..." << std::endl;
            Parser parser(tokens, opts_.inputFile);
            auto ast = parser.parse();

            if (opts_.dumpAst || opts_.format == OutputFormat::AST_DUMP) {
                dumpAST(ast, 0);
                if (opts_.format == OutputFormat::AST_DUMP) return true;
            }

            // 4. Code generation
            if (opts_.verbose) std::cout << "[4/5] Generating x86-64 code..." << std::endl;
            X86_64 codegen;
            codegen.setCallingConv(opts_.callingConv);

            // Check for bootloader directive
            for (auto& child : ast->children) {
                if (child->type == NodeType::DIRECTIVE_STMT && child->directive == "bootloader") {
                    codegen.setBootloaderMode(true);
                    if (opts_.format != OutputFormat::FLAT_BINARY) {
                        opts_.format = OutputFormat::BOOT_SECTOR;
                    }
                }
            }

            codegen.generate(ast);

            if (opts_.dumpCode || opts_.format == OutputFormat::CODE_DUMP) {
                codegen.dumpCode();
                if (opts_.format == OutputFormat::CODE_DUMP) return true;
            }

            // 5. Output
            if (opts_.verbose) std::cout << "[5/5] Writing output: " << opts_.outputFile << std::endl;

            std::vector<uint8_t> output;
            switch (opts_.format) {
                case OutputFormat::ELF64:
                    output = ELF64Writer::createExecutable(
                        codegen.code(), codegen.data(), codegen.rodata(), codegen.entryPoint());
                    break;
                case OutputFormat::PE64: {
                    uint16_t subsys = opts_.guiSubsystem ? 2 : 3;
                    output = PE64Writer::createExecutable(
                        codegen.code(), codegen.data(), codegen.rodata(), codegen.entryPoint(),
                        codegen.importRelocations(), codegen.relocations(), subsys);
                    break;
                }
                case OutputFormat::FLAT_BINARY:
                    output = BinaryWriter::createFlatBinary(codegen.code(), codegen.data());
                    break;
                case OutputFormat::BOOT_SECTOR:
                    output = BinaryWriter::createBootSector(codegen.code());
                    break;
                default:
                    break;
            }

            if (!writeFile(opts_.outputFile, output)) {
                std::cerr << "Error: Could not write output file '" << opts_.outputFile << "'" << std::endl;
                return false;
            }

            if (opts_.verbose) {
                std::cout << "  Code:   " << codegen.code().size() << " bytes" << std::endl;
                std::cout << "  Data:   " << codegen.data().size() << " bytes" << std::endl;
                std::cout << "  Rodata: " << codegen.rodata().size() << " bytes" << std::endl;
                std::cout << "  Output: " << output.size() << " bytes" << std::endl;
            }

            std::cout << "Compiled successfully: " << opts_.outputFile
                      << " (" << output.size() << " bytes)" << std::endl;
            return true;

        } catch (const ParseError& e) {
            std::cerr << e.filename << ":" << e.line << ":" << e.column
                      << ": Parse error: " << e.what() << std::endl;
            return false;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return false;
        }
    }

private:
    CompilerOptions opts_;

    static std::string readFile(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return "";
        std::ostringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    static bool writeFile(const std::string& path, const std::vector<uint8_t>& data) {
        // Create parent directory if needed
        std::filesystem::path p(path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        return file.good();
    }

    void dumpAST(const std::shared_ptr<ASTNode>& node, int depth) {
        if (!node) return;
        std::string indent(depth * 2, ' ');

        // Node type name
        static const char* nodeNames[] = {
            "PROGRAM","BLOCK",
            "VAR_DECL","CONST_DECL","MUT_DECL",
            "FUNC_DECL","EXTERN_FUNC_DECL","STRUCT_DECL","ENUM_DECL","UNION_DECL",
            "BITFIELD_DECL","MODULE_DECL","IMPORT_STMT",
            "EXPR_STMT","RETURN_STMT","IF_STMT","WHILE_STMT","FOR_STMT",
            "LOOP_STMT","BREAK_STMT","CONTINUE_STMT","SWITCH_STMT","CASE_CLAUSE",
            "DEFAULT_CLAUSE","GOTO_STMT","LABEL_STMT",
            "ASSEMBLY_BLOCK","ASM_INSTRUCTION","SYSCALL_EXPR",
            "INTERRUPT_DECL","INTERRUPT_HANDLER_DECL","DIRECTIVE_STMT",
            "SECTION_STMT","GLOBAL_STMT","ALIGN_STMT",
            "BINARY_EXPR","UNARY_EXPR","CALL_EXPR","MEMBER_EXPR","INDEX_EXPR",
            "CAST_EXPR","BITCAST_EXPR","SIZEOF_EXPR","ALIGNOF_EXPR",
            "ADDRESS_OF_EXPR","DEREF_EXPR","OFFSET_OF_EXPR",
            "ASSIGNMENT_EXPR","COMPOUND_ASSIGNMENT_EXPR","TERNARY_EXPR",
            "POSTFIX_EXPR","SCOPE_RESOLUTION_EXPR","TYPEOF_EXPR",
            "MEMORY_EXPR","PORT_EXPR","REGISTER_EXPR",
            "INT_LITERAL","FLOAT_LITERAL","STRING_LITERAL","CHAR_LITERAL",
            "BOOL_LITERAL","NULL_LITERAL","ARRAY_LITERAL","STRUCT_LITERAL",
            "IDENTIFIER_EXPR",
            "TYPE_ANNOTATION","PTR_TYPE","ARRAY_TYPE",
            "WHEN_STMT",
        };

        int idx = static_cast<int>(node->type);
        const char* name = (idx >= 0 && idx < (int)(sizeof(nodeNames)/sizeof(nodeNames[0])))
            ? nodeNames[idx] : "UNKNOWN";

        std::cout << indent << name;
        if (!node->name.empty()) std::cout << " name=\"" << node->name << "\"";
        if (!node->op.empty()) std::cout << " op=\"" << node->op << "\"";
        if (node->type == NodeType::INT_LITERAL) std::cout << " value=" << node->intValue;
        if (node->type == NodeType::FLOAT_LITERAL) std::cout << " value=" << node->floatValue;
        if (node->type == NodeType::STRING_LITERAL) std::cout << " value=\"" << node->stringValue << "\"";
        if (node->type == NodeType::BOOL_LITERAL) std::cout << " value=" << (node->boolValue ? "true" : "false");
        std::cout << std::endl;

        // Recurse
        if (node->left) dumpAST(node->left, depth + 1);
        if (node->right) dumpAST(node->right, depth + 1);
        if (node->condition) dumpAST(node->condition, depth + 1);
        if (node->body) dumpAST(node->body, depth + 1);
        if (node->elseBody) dumpAST(node->elseBody, depth + 1);
        if (node->init) dumpAST(node->init, depth + 1);
        if (node->step) dumpAST(node->step, depth + 1);
        if (node->object) dumpAST(node->object, depth + 1);
        for (auto& child : node->children) dumpAST(child, depth + 1);
        for (auto& arg : node->arguments) dumpAST(arg, depth + 1);
    }
};

} // namespace verslang
