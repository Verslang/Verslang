#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include "lexer/token.h"

namespace verslang {

// ═══════════════════════════════════════════════════════════════
// Type System — Fixed-width types for low-level programming
// ═══════════════════════════════════════════════════════════════

enum class TypeKind {
    VOID, BOOL, CHAR,
    U8, U16, U32, U64,
    I8, I16, I32, I64,
    F32, F64,
    USIZE, ISIZE,       // Platform-dependent size (u64/i64 on 64-bit)
    PTR,                 // ptr<T>
    ARRAY,               // array<T, N>  or [T; N]
    STRUCT,              // User-defined structure
    ENUM,                // User-defined enumeration
    UNION,               // User-defined union
    FUNCTION,            // Function pointer type
    NAMED,               // Named type reference (resolved later)
    UNRESOLVED,          // Not yet resolved
};

struct TypeInfo;
using TypePtr = std::shared_ptr<TypeInfo>;

struct TypeInfo {
    TypeKind kind;
    std::string name;           // For STRUCT, ENUM, UNION, NAMED
    TypePtr pointeeType;        // For PTR: what it points to
    TypePtr elementType;        // For ARRAY: element type
    uint64_t arraySize = 0;     // For ARRAY: fixed size
    TypePtr returnType;         // For FUNCTION: return type
    std::vector<TypePtr> paramTypes; // For FUNCTION: parameter types

    // Size in bytes
    size_t sizeBytes() const {
        switch (kind) {
            case TypeKind::VOID: return 0;
            case TypeKind::BOOL: case TypeKind::U8: case TypeKind::I8: case TypeKind::CHAR: return 1;
            case TypeKind::U16: case TypeKind::I16: return 2;
            case TypeKind::U32: case TypeKind::I32: case TypeKind::F32: return 4;
            case TypeKind::U64: case TypeKind::I64: case TypeKind::F64:
            case TypeKind::USIZE: case TypeKind::ISIZE: case TypeKind::PTR:
            case TypeKind::FUNCTION: return 8;
            case TypeKind::ARRAY: return elementType ? elementType->sizeBytes() * arraySize : 0;
            default: return 0; // STRUCT, ENUM, etc. need layout computation
        }
    }

    size_t alignment() const {
        size_t s = sizeBytes();
        if (s == 0) return 1;
        if (s >= 8) return 8;
        if (s >= 4) return 4;
        if (s >= 2) return 2;
        return 1;
    }

    bool isInteger() const {
        return kind >= TypeKind::U8 && kind <= TypeKind::I64;
    }
    bool isUnsigned() const {
        return kind == TypeKind::U8 || kind == TypeKind::U16 ||
               kind == TypeKind::U32 || kind == TypeKind::U64 || kind == TypeKind::USIZE;
    }
    bool isSigned() const {
        return kind == TypeKind::I8 || kind == TypeKind::I16 ||
               kind == TypeKind::I32 || kind == TypeKind::I64 || kind == TypeKind::ISIZE;
    }
    bool isFloat() const {
        return kind == TypeKind::F32 || kind == TypeKind::F64;
    }
    bool isNumeric() const { return isInteger() || isFloat(); }
    bool isPointer() const { return kind == TypeKind::PTR; }
    bool isVoid() const { return kind == TypeKind::VOID; }
    bool isBool() const { return kind == TypeKind::BOOL; }

    // Factory methods
    static TypePtr makeVoid()  { auto t = std::make_shared<TypeInfo>(); t->kind = TypeKind::VOID; return t; }
    static TypePtr makeBool()  { auto t = std::make_shared<TypeInfo>(); t->kind = TypeKind::BOOL; return t; }
    static TypePtr makeChar()  { auto t = std::make_shared<TypeInfo>(); t->kind = TypeKind::CHAR; return t; }
    static TypePtr makeU8()    { auto t = std::make_shared<TypeInfo>(); t->kind = TypeKind::U8; return t; }
    static TypePtr makeU16()   { auto t = std::make_shared<TypeInfo>(); t->kind = TypeKind::U16; return t; }
    static TypePtr makeU32()   { auto t = std::make_shared<TypeInfo>(); t->kind = TypeKind::U32; return t; }
    static TypePtr makeU64()   { auto t = std::make_shared<TypeInfo>(); t->kind = TypeKind::U64; return t; }
    static TypePtr makeI8()    { auto t = std::make_shared<TypeInfo>(); t->kind = TypeKind::I8; return t; }
    static TypePtr makeI16()   { auto t = std::make_shared<TypeInfo>(); t->kind = TypeKind::I16; return t; }
    static TypePtr makeI32()   { auto t = std::make_shared<TypeInfo>(); t->kind = TypeKind::I32; return t; }
    static TypePtr makeI64()   { auto t = std::make_shared<TypeInfo>(); t->kind = TypeKind::I64; return t; }
    static TypePtr makeF32()   { auto t = std::make_shared<TypeInfo>(); t->kind = TypeKind::F32; return t; }
    static TypePtr makeF64()   { auto t = std::make_shared<TypeInfo>(); t->kind = TypeKind::F64; return t; }
    static TypePtr makeUsize() { auto t = std::make_shared<TypeInfo>(); t->kind = TypeKind::USIZE; return t; }
    static TypePtr makeIsize() { auto t = std::make_shared<TypeInfo>(); t->kind = TypeKind::ISIZE; return t; }

    static TypePtr makePtr(TypePtr pointee) {
        auto t = std::make_shared<TypeInfo>();
        t->kind = TypeKind::PTR;
        t->pointeeType = pointee;
        return t;
    }
    static TypePtr makeArray(TypePtr elem, uint64_t size) {
        auto t = std::make_shared<TypeInfo>();
        t->kind = TypeKind::ARRAY;
        t->elementType = elem;
        t->arraySize = size;
        return t;
    }
    static TypePtr makeNamed(const std::string& name) {
        auto t = std::make_shared<TypeInfo>();
        t->kind = TypeKind::NAMED;
        t->name = name;
        return t;
    }
    static TypePtr makeFunction(TypePtr ret, std::vector<TypePtr> params) {
        auto t = std::make_shared<TypeInfo>();
        t->kind = TypeKind::FUNCTION;
        t->returnType = ret;
        t->paramTypes = std::move(params);
        return t;
    }

    std::string toString() const {
        switch (kind) {
            case TypeKind::VOID: return "void";
            case TypeKind::BOOL: return "bool";
            case TypeKind::CHAR: return "char";
            case TypeKind::U8: return "u8"; case TypeKind::U16: return "u16";
            case TypeKind::U32: return "u32"; case TypeKind::U64: return "u64";
            case TypeKind::I8: return "i8"; case TypeKind::I16: return "i16";
            case TypeKind::I32: return "i32"; case TypeKind::I64: return "i64";
            case TypeKind::F32: return "f32"; case TypeKind::F64: return "f64";
            case TypeKind::USIZE: return "usize"; case TypeKind::ISIZE: return "isize";
            case TypeKind::PTR: return "ptr<" + (pointeeType ? pointeeType->toString() : "?") + ">";
            case TypeKind::ARRAY: return "array<" + (elementType ? elementType->toString() : "?") + ", " + std::to_string(arraySize) + ">";
            case TypeKind::STRUCT: return "structure " + name;
            case TypeKind::ENUM: return "enumeration " + name;
            case TypeKind::NAMED: return name;
            default: return "unknown";
        }
    }
};

// ═══════════════════════════════════════════════════════════════
// AST Node Types
// ═══════════════════════════════════════════════════════════════

enum class NodeType {
    // Top-level
    PROGRAM,
    BLOCK,

    // Declarations
    VAR_DECL,               // declare x: u32 = 10
    CONST_DECL,             // constant PI: f64 = 3.14
    MUT_DECL,               // mutable counter: u32 = 0
    FUNC_DECL,              // function name(params) -> type { body }
    EXTERN_FUNC_DECL,       // external function name(params) -> type
    STRUCT_DECL,            // structure Name { fields }
    ENUM_DECL,              // enumeration Name { variants }
    UNION_DECL,             // union Name { fields }
    BITFIELD_DECL,          // bitfield Name { fields }
    MODULE_DECL,            // module name { body }
    IMPORT_STMT,            // import "file" / import module

    // Statements
    EXPR_STMT,              // expression;
    RETURN_STMT,            // return expr
    IF_STMT,                // if (cond) { } else { }
    WHILE_STMT,             // while (cond) { }
    FOR_STMT,               // for (init; cond; step) { }
    LOOP_STMT,              // loop { }
    BREAK_STMT,             // break
    CONTINUE_STMT,          // continue
    SWITCH_STMT,            // switch (val) { cases }
    CASE_CLAUSE,            // case value: statements
    DEFAULT_CLAUSE,         // default: statements
    GOTO_STMT,              // goto label
    LABEL_STMT,             // label:

    // Low-level statements
    ASSEMBLY_BLOCK,         // assembly { instructions }
    ASM_INSTRUCTION,        // Single assembly instruction
    SYSCALL_EXPR,           // syscall(number, args...)
    INTERRUPT_DECL,         // interrupt 0x80 { handler }
    INTERRUPT_HANDLER_DECL, // interrupt_handler name() { }
    DIRECTIVE_STMT,         // directive bootloader / directive bits 16
    SECTION_STMT,           // section .text
    GLOBAL_STMT,            // global _start
    ALIGN_STMT,             // align 16

    // Expressions
    BINARY_EXPR,            // a + b, a == b, etc.
    UNARY_EXPR,             // -x, !x, ~x, &x, *x
    CALL_EXPR,              // func(args)
    MEMBER_EXPR,            // obj.field
    INDEX_EXPR,             // arr[idx]
    CAST_EXPR,              // cast<T>(expr)
    BITCAST_EXPR,           // bitcast<T>(expr)
    SIZEOF_EXPR,            // sizeof(T)
    ALIGNOF_EXPR,           // alignof(T)
    ADDRESS_OF_EXPR,        // address_of(expr)
    DEREF_EXPR,             // dereference(expr)
    OFFSET_OF_EXPR,         // offset_of(Struct, field)
    ASSIGNMENT_EXPR,        // x = value
    COMPOUND_ASSIGNMENT_EXPR, // x += value
    TERNARY_EXPR,           // cond ? a : b
    POSTFIX_EXPR,           // x++ / x--
    SCOPE_RESOLUTION_EXPR,  // Module::member
    TYPEOF_EXPR,            // typeof(expr)

    // Memory expressions
    MEMORY_EXPR,            // memory.allocate / memory.free etc.
    PORT_EXPR,              // port.input_byte / port.output_byte
    REGISTER_EXPR,          // register.rax

    // Literals
    INT_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    CHAR_LITERAL,
    BOOL_LITERAL,
    NULL_LITERAL,
    ARRAY_LITERAL,          // [1, 2, 3]
    STRUCT_LITERAL,         // Point { x: 1.0, y: 2.0 }

    // Identifiers
    IDENTIFIER_EXPR,

    // Type annotations
    TYPE_ANNOTATION,
    PTR_TYPE,
    ARRAY_TYPE,

    // Conditional compilation
    WHEN_STMT,              // when platform == "windows" { }
};

// ═══════════════════════════════════════════════════════════════
// AST Node — The tree structure for parsed Verslang code
// ═══════════════════════════════════════════════════════════════

struct ASTNode;
using ASTNodePtr = std::shared_ptr<ASTNode>;

// Struct field definition
struct FieldDef {
    std::string name;
    TypePtr type;
    ASTNodePtr defaultValue;
    int bitWidth = -1;      // For bitfields: number of bits
};

// Function parameter
struct ParamDef {
    std::string name;
    TypePtr type;
    bool isVariadic = false; // ... parameter
    ASTNodePtr defaultValue;
};

// Enum variant
struct EnumVariant {
    std::string name;
    ASTNodePtr value;       // Optional explicit value
};

// Assembly instruction representation
struct AsmInstr {
    std::string mnemonic;   // mov, add, sub, etc.
    std::string operand1;   // First operand
    std::string operand2;   // Second operand (optional)
    std::string operand3;   // Third operand (optional)
    std::string comment;    // Inline comment
};

struct ASTNode {
    NodeType type;
    Token token;            // Source location

    // ── Name / identifiers ──
    std::string name;
    std::string op;         // Operator for binary/unary expressions
    std::string importPath; // For imports

    // ── Type information ──
    TypePtr typeAnnotation;
    TypePtr castType;       // For cast<T> / bitcast<T>

    // ── Numeric values ──
    int64_t intValue = 0;
    double floatValue = 0.0;
    bool boolValue = false;
    std::string stringValue;
    char charValue = 0;

    // ── Tree structure ──
    ASTNodePtr left;        // Left operand / condition / init
    ASTNodePtr right;       // Right operand / step
    ASTNodePtr body;        // Body block / then-branch
    ASTNodePtr elseBody;    // Else-branch
    ASTNodePtr condition;   // For if/while/for
    ASTNodePtr init;        // For-loop init
    ASTNodePtr step;        // For-loop step
    ASTNodePtr object;      // For member/index expressions

    // ── Collections ──
    std::vector<ASTNodePtr> children;       // Block children / arguments
    std::vector<ASTNodePtr> arguments;      // Call arguments
    std::vector<ParamDef> params;           // Function parameters
    std::vector<FieldDef> fields;           // Struct/union fields
    std::vector<EnumVariant> variants;      // Enum variants
    std::vector<ASTNodePtr> cases;          // Switch cases
    std::vector<AsmInstr> asmInstructions;  // Assembly block

    // ── Flags ──
    bool isExport = false;
    bool isExtern = false;
    bool isInline = false;
    bool isVolatile = false;
    bool isPacked = false;
    bool isStatic = false;
    bool isVariadic = false;

    // ── Directives ──
    std::string directive;
    std::string directiveValue;

    // ── Import details ──
    std::string moduleName;
    std::string alias;
    std::string author;    // For versmodule:@author:module

    // ── Section name ──
    std::string sectionName;

    // ── Alignment ──
    int alignValue = 0;

    // Factory methods
    static ASTNodePtr make(NodeType t) {
        auto n = std::make_shared<ASTNode>();
        n->type = t;
        return n;
    }

    static ASTNodePtr makeProgram(std::vector<ASTNodePtr> stmts = {}) {
        auto n = make(NodeType::PROGRAM);
        n->children = std::move(stmts);
        return n;
    }

    static ASTNodePtr makeBlock(std::vector<ASTNodePtr> stmts = {}) {
        auto n = make(NodeType::BLOCK);
        n->children = std::move(stmts);
        return n;
    }

    static ASTNodePtr makeIntLiteral(int64_t val, Token tok = Token()) {
        auto n = make(NodeType::INT_LITERAL);
        n->intValue = val;
        n->token = tok;
        return n;
    }

    static ASTNodePtr makeFloatLiteral(double val, Token tok = Token()) {
        auto n = make(NodeType::FLOAT_LITERAL);
        n->floatValue = val;
        n->token = tok;
        return n;
    }

    static ASTNodePtr makeStringLiteral(const std::string& val, Token tok = Token()) {
        auto n = make(NodeType::STRING_LITERAL);
        n->stringValue = val;
        n->token = tok;
        return n;
    }

    static ASTNodePtr makeCharLiteral(char val, Token tok = Token()) {
        auto n = make(NodeType::CHAR_LITERAL);
        n->charValue = val;
        n->token = tok;
        return n;
    }

    static ASTNodePtr makeBoolLiteral(bool val, Token tok = Token()) {
        auto n = make(NodeType::BOOL_LITERAL);
        n->boolValue = val;
        n->token = tok;
        return n;
    }

    static ASTNodePtr makeIdentifier(const std::string& name, Token tok = Token()) {
        auto n = make(NodeType::IDENTIFIER_EXPR);
        n->name = name;
        n->token = tok;
        return n;
    }

    static ASTNodePtr makeBinaryExpr(const std::string& op, ASTNodePtr left, ASTNodePtr right, Token tok = Token()) {
        auto n = make(NodeType::BINARY_EXPR);
        n->op = op;
        n->left = left;
        n->right = right;
        n->token = tok;
        return n;
    }

    static ASTNodePtr makeUnaryExpr(const std::string& op, ASTNodePtr operand, Token tok = Token()) {
        auto n = make(NodeType::UNARY_EXPR);
        n->op = op;
        n->left = operand;
        n->token = tok;
        return n;
    }

    static ASTNodePtr makeCallExpr(ASTNodePtr func, std::vector<ASTNodePtr> args, Token tok = Token()) {
        auto n = make(NodeType::CALL_EXPR);
        n->left = func;
        n->arguments = std::move(args);
        n->token = tok;
        return n;
    }
};

} // namespace verslang
