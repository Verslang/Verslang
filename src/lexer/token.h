#pragma once
#include <string>
#include <cstdint>

namespace verslang {

// ═══════════════════════════════════════════════════════════════
// Token Types — Every lexical element of the Verslang language
// ═══════════════════════════════════════════════════════════════
enum class TokenType {
    // ── Literals ──
    INTEGER_LITERAL,        // 42, 0xFF, 0b1010, 0o77
    FLOAT_LITERAL,          // 3.14, 1.0e10
    STRING_LITERAL,         // "hello"
    CHAR_LITERAL,           // 'A'
    HEX_LITERAL,            // 0xFF
    BINARY_LITERAL,         // 0b1010
    OCTAL_LITERAL,          // 0o77

    // ── Identifier ──
    IDENTIFIER,             // variable/function/type names

    // ── Declaration Keywords ──
    KW_DECLARE,             // declare x: u32 = 10
    KW_CONSTANT,            // constant PI: f64 = 3.14
    KW_MUTABLE,             // mutable counter: u32 = 0
    KW_FUNCTION,            // function name() -> type { }
    KW_EXTERNAL,            // external function printf(...)
    KW_EXPORT,              // export function public_fn()
    KW_RETURN,              // return value
    KW_INLINE,              // inline function hint

    // ── Composite Type Keywords ──
    KW_STRUCTURE,           // structure Point { x: f64, y: f64 }
    KW_ENUMERATION,         // enumeration Color { RED, GREEN, BLUE }
    KW_UNION,               // union Value { i: i64, f: f64 }
    KW_BITFIELD,            // bitfield Flags { a: 1, b: 3, c: 4 }

    // ── Module Keywords ──
    KW_MODULE,              // module name { }
    KW_IMPORT,              // import "file.vlang"

    // ── Control Flow Keywords ──
    KW_IF,                  // if (cond) { }
    KW_ELSE,                // else { }
    KW_WHILE,               // while (cond) { }
    KW_FOR,                 // for (init; cond; step) { }
    KW_LOOP,                // loop { } (infinite)
    KW_BREAK,               // break
    KW_CONTINUE,            // continue
    KW_SWITCH,              // switch (val) { }
    KW_CASE,                // case value:
    KW_DEFAULT,             // default:
    KW_GOTO,                // goto label (low-level jump)
    KW_LABEL,               // label: (jump target)

    // ── Low-Level Keywords ──
    KW_ASSEMBLY,            // assembly { mov rax, 1 }
    KW_SYSCALL,             // syscall(number, args...)
    KW_INTERRUPT,           // interrupt 0x80 { }
    KW_INTERRUPT_HANDLER,   // interrupt_handler name() { }
    KW_DIRECTIVE,           // directive bootloader / directive bits 16
    KW_BOOTLOADER,          // bootloader { }
    KW_SECTION,             // section .text / .data / .bss / .rodata
    KW_GLOBAL,              // global _start (ELF symbol)
    KW_ALIGN,               // align 16

    // ── Memory & Hardware Keywords ──
    KW_MEMORY,              // memory.allocate / memory.free
    KW_PORT,                // port.input_byte / port.output_byte
    KW_REGISTER,            // register.rax / register.rbx
    KW_VOLATILE,            // volatile declare (no optimization)
    KW_PACKED,              // packed structure (no padding)
    KW_STATIC,              // static declare (file scope)

    // ── Type Operation Keywords ──
    KW_CAST,                // cast<u64>(value)
    KW_SIZEOF,              // sizeof(Type)
    KW_ALIGNOF,             // alignof(Type)
    KW_TYPEOF,              // typeof(expr)
    KW_ADDRESS_OF,          // address_of(variable)
    KW_DEREFERENCE,         // dereference(pointer)
    KW_OFFSET_OF,           // offset_of(Structure, field)
    KW_BITCAST,             // bitcast<f64>(int_val)

    // ── Conditional Compilation ──
    KW_WHEN,                // when platform == "windows" { }
    KW_PLATFORM,            // platform identifier

    // ── Primitive Type Keywords ──
    KW_U8,    KW_U16,   KW_U32,   KW_U64,
    KW_I8,    KW_I16,   KW_I32,   KW_I64,
    KW_F32,   KW_F64,
    KW_BOOL,  KW_VOID,  KW_CHAR,
    KW_USIZE, KW_ISIZE,
    KW_PTR,   KW_ARRAY,

    // ── Value Keywords ──
    KW_TRUE,   KW_FALSE,  KW_NULL,  KW_NULLPTR,

    // ── Arithmetic Operators ──
    OP_PLUS,          // +
    OP_MINUS,         // -
    OP_STAR,          // * (multiply or pointer deref)
    OP_SLASH,         // /
    OP_PERCENT,       // %

    // ── Comparison Operators ──
    OP_ASSIGN,        // =
    OP_EQ,            // ==
    OP_NEQ,           // !=
    OP_LT,            // <
    OP_GT,            // >
    OP_LTE,           // <=
    OP_GTE,           // >=

    // ── Logical Operators ──
    OP_AND,           // &&
    OP_OR,            // ||
    OP_NOT,           // !

    // ── Bitwise Operators ──
    OP_BIT_AND,       // &
    OP_BIT_OR,        // |
    OP_BIT_XOR,       // ^
    OP_BIT_NOT,       // ~
    OP_SHL,           // <<
    OP_SHR,           // >>

    // ── Compound Assignment ──
    OP_PLUS_ASSIGN,   // +=
    OP_MINUS_ASSIGN,  // -=
    OP_STAR_ASSIGN,   // *=
    OP_SLASH_ASSIGN,  // /=
    OP_PERCENT_ASSIGN,// %=
    OP_AND_ASSIGN,    // &=
    OP_OR_ASSIGN,     // |=
    OP_XOR_ASSIGN,    // ^=
    OP_SHL_ASSIGN,    // <<=
    OP_SHR_ASSIGN,    // >>=

    // ── Increment/Decrement ──
    OP_INCREMENT,     // ++
    OP_DECREMENT,     // --

    // ── Delimiters ──
    LPAREN,           // (
    RPAREN,           // )
    LBRACE,           // {
    RBRACE,           // }
    LBRACKET,         // [
    RBRACKET,         // ]
    LANGLE,           // < (generic context)
    RANGLE,           // > (generic context)

    // ── Punctuation ──
    SEMICOLON,        // ;
    COMMA,            // ,
    COLON,            // :
    DOT,              // .
    ARROW,            // ->
    DOUBLE_COLON,     // ::
    ELLIPSIS,         // ...
    AT,               // @
    HASH,             // # (preprocessor/attribute)
    QUESTION,         // ?

    // ── Special ──
    TOKEN_EOF,
    TOKEN_NEWLINE,
    TOKEN_ERROR,

    // ── Assembly Mode Tokens ──
    ASM_INSTRUCTION,  // mov, add, sub, etc.
    ASM_REGISTER,     // rax, rbx, etc.
    ASM_LABEL,        // label:
};

// ═══════════════════════════════════════════════════════════════
// Token — A single lexical unit with source location
// ═══════════════════════════════════════════════════════════════
struct Token {
    TokenType type;
    std::string value;
    std::string filename;
    int line;
    int column;

    // Parsed numeric values
    int64_t intValue = 0;
    double floatValue = 0.0;

    Token() : type(TokenType::TOKEN_EOF), line(0), column(0) {}
    Token(TokenType t, const std::string& v, int l, int c)
        : type(t), value(v), line(l), column(c) {}
    Token(TokenType t, const std::string& v, const std::string& f, int l, int c)
        : type(t), value(v), filename(f), line(l), column(c) {}

    bool is(TokenType t) const { return type == t; }
    bool isOneOf(TokenType t1, TokenType t2) const { return type == t1 || type == t2; }

    // Check if token is a type keyword
    bool isTypeKeyword() const {
        return type >= TokenType::KW_U8 && type <= TokenType::KW_ARRAY;
    }

    // Check if token is an assignment operator
    bool isAssignOp() const {
        return type == TokenType::OP_ASSIGN ||
               (type >= TokenType::OP_PLUS_ASSIGN && type <= TokenType::OP_SHR_ASSIGN);
    }
};

// Token type to string for debugging
inline const char* tokenTypeStr(TokenType t) {
    switch (t) {
        case TokenType::INTEGER_LITERAL: return "INTEGER";
        case TokenType::FLOAT_LITERAL: return "FLOAT";
        case TokenType::STRING_LITERAL: return "STRING";
        case TokenType::CHAR_LITERAL: return "CHAR";
        case TokenType::IDENTIFIER: return "IDENT";
        case TokenType::KW_DECLARE: return "declare";
        case TokenType::KW_CONSTANT: return "constant";
        case TokenType::KW_MUTABLE: return "mutable";
        case TokenType::KW_FUNCTION: return "function";
        case TokenType::KW_EXTERNAL: return "external";
        case TokenType::KW_RETURN: return "return";
        case TokenType::KW_STRUCTURE: return "structure";
        case TokenType::KW_ENUMERATION: return "enumeration";
        case TokenType::KW_MODULE: return "module";
        case TokenType::KW_IMPORT: return "import";
        case TokenType::KW_IF: return "if";
        case TokenType::KW_ELSE: return "else";
        case TokenType::KW_WHILE: return "while";
        case TokenType::KW_FOR: return "for";
        case TokenType::KW_LOOP: return "loop";
        case TokenType::KW_BREAK: return "break";
        case TokenType::KW_CONTINUE: return "continue";
        case TokenType::KW_ASSEMBLY: return "assembly";
        case TokenType::KW_SYSCALL: return "syscall";
        case TokenType::TOKEN_EOF: return "EOF";
        default: return "?";
    }
}

} // namespace verslang
