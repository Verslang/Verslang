#include "parser.h"
#include <sstream>

namespace verslang {

// ═══════════════════════════════════════════════════════════════
// Constructor
// ═══════════════════════════════════════════════════════════════

Parser::Parser(const std::vector<Token>& tokens, const std::string& filename)
    : tokens_(tokens), filename_(filename) {}

// ═══════════════════════════════════════════════════════════════
// Token access helpers
// ═══════════════════════════════════════════════════════════════

const Token& Parser::current() const {
    if (pos_ >= tokens_.size()) return tokens_.back();
    return tokens_[pos_];
}

const Token& Parser::peek(int offset) const {
    size_t idx = pos_ + offset;
    if (idx >= tokens_.size()) return tokens_.back();
    return tokens_[idx];
}

const Token& Parser::advance() {
    const Token& tok = current();
    if (pos_ < tokens_.size()) pos_++;
    return tok;
}

bool Parser::check(TokenType type) const {
    return current().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) { advance(); return true; }
    return false;
}

const Token& Parser::expect(TokenType type, const std::string& msg) {
    if (check(type)) return advance();
    error(msg + " (got '" + current().value + "')");
    return current(); // unreachable
}

bool Parser::isAtEnd() const {
    return current().type == TokenType::TOKEN_EOF;
}

bool Parser::expectCloseAngle() {
    if (pendingCloseAngle_) {
        pendingCloseAngle_ = false;
        return true;
    }
    if (match(TokenType::OP_GT)) return true;
    if (check(TokenType::OP_SHR)) {
        advance(); // consume >>
        pendingCloseAngle_ = true; // one > remains
        return true;
    }
    error("Expected '>'");
    return false;
}

void Parser::error(const std::string& msg) {
    throw ParseError(msg, current());
}

void Parser::optionalSemicolon() {
    match(TokenType::SEMICOLON);
}

// ═══════════════════════════════════════════════════════════════
// Top-level parsing
// ═══════════════════════════════════════════════════════════════

std::shared_ptr<ASTNode> Parser::parse() {
    return parseProgram();
}

std::shared_ptr<ASTNode> Parser::parseProgram() {
    auto program = ASTNode::makeProgram();
    while (!isAtEnd()) {
        program->children.push_back(parseStatement());
    }
    return program;
}

std::shared_ptr<ASTNode> Parser::parseBlock() {
    expect(TokenType::LBRACE, "Expected '{'");
    auto block = ASTNode::makeBlock();
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        block->children.push_back(parseStatement());
    }
    expect(TokenType::RBRACE, "Expected '}'");
    return block;
}

// ═══════════════════════════════════════════════════════════════
// Statement dispatch
// ═══════════════════════════════════════════════════════════════

std::shared_ptr<ASTNode> Parser::parseStatement() {
    // Export modifier
    if (check(TokenType::KW_EXPORT)) {
        advance();
        if (check(TokenType::KW_FUNCTION)) {
            return parseFuncDecl(true, false, false);
        }
        error("Expected 'function' after 'export'");
    }

    // External modifier
    if (check(TokenType::KW_EXTERNAL)) {
        advance();
        bool isInline = false;
        if (match(TokenType::KW_INLINE)) isInline = true;
        if (check(TokenType::KW_FUNCTION)) {
            return parseFuncDecl(false, true, isInline);
        }
        error("Expected 'function' after 'external'");
    }

    // Inline modifier
    if (check(TokenType::KW_INLINE)) {
        advance();
        if (check(TokenType::KW_FUNCTION)) {
            return parseFuncDecl(false, false, true);
        }
        error("Expected 'function' after 'inline'");
    }

    switch (current().type) {
        // Declarations
        case TokenType::KW_DECLARE:
            advance(); return parseVarDecl(TokenType::KW_DECLARE);
        case TokenType::KW_CONSTANT:
            advance(); return parseVarDecl(TokenType::KW_CONSTANT);
        case TokenType::KW_MUTABLE:
            advance(); return parseVarDecl(TokenType::KW_MUTABLE);
        case TokenType::KW_FUNCTION:
            return parseFuncDecl(false, false, false);
        case TokenType::KW_STRUCTURE:
            return parseStructDecl();
        case TokenType::KW_ENUMERATION:
            return parseEnumDecl();
        case TokenType::KW_UNION:
            return parseUnionDecl();
        case TokenType::KW_MODULE:
            return parseModuleDecl();
        case TokenType::KW_IMPORT:
            return parseImport();
        case TokenType::KW_INTERRUPT_HANDLER:
            return parseInterruptHandler();

        // Control flow
        case TokenType::KW_IF:
            return parseIfStmt();
        case TokenType::KW_WHILE:
            return parseWhileStmt();
        case TokenType::KW_FOR:
            return parseForStmt();
        case TokenType::KW_LOOP:
            return parseLoopStmt();
        case TokenType::KW_RETURN:
            return parseReturnStmt();
        case TokenType::KW_SWITCH:
            return parseSwitchStmt();
        case TokenType::KW_BREAK: {
            advance();
            auto node = ASTNode::make(NodeType::BREAK_STMT);
            optionalSemicolon();
            return node;
        }
        case TokenType::KW_CONTINUE: {
            advance();
            auto node = ASTNode::make(NodeType::CONTINUE_STMT);
            optionalSemicolon();
            return node;
        }
        case TokenType::KW_GOTO:
            return parseGotoStmt();
        case TokenType::KW_LABEL:
            return parseLabelStmt();

        // Low-level
        case TokenType::KW_ASSEMBLY:
            return parseAssemblyBlock();
        case TokenType::KW_DIRECTIVE:
            return parseDirective();
        case TokenType::KW_WHEN:
            return parseWhenStmt();

        // Expression statement
        default: {
            auto expr = parseExpression();
            auto node = ASTNode::make(NodeType::EXPR_STMT);
            node->left = expr;
            optionalSemicolon();
            return node;
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// Variable declarations
//   declare x: u32 = 10
//   constant PI: f64 = 3.14
//   mutable counter: u32 = 0
// ═══════════════════════════════════════════════════════════════

std::shared_ptr<ASTNode> Parser::parseVarDecl(TokenType declType) {
    NodeType nodeType;
    switch (declType) {
        case TokenType::KW_CONSTANT: nodeType = NodeType::CONST_DECL; break;
        case TokenType::KW_MUTABLE:  nodeType = NodeType::MUT_DECL; break;
        default:                     nodeType = NodeType::VAR_DECL; break;
    }

    auto node = ASTNode::make(nodeType);
    node->name = expect(TokenType::IDENTIFIER, "Expected variable name").value;

    // Optional type annotation
    if (match(TokenType::COLON)) {
        node->typeAnnotation = parseType();
    }

    // Optional initializer
    if (match(TokenType::OP_ASSIGN)) {
        node->left = parseExpression();
    }

    optionalSemicolon();
    return node;
}

// ═══════════════════════════════════════════════════════════════
// Function declarations
//   function add(a: i32, b: i32) -> i32 { return a + b }
//   external function printf(fmt: ptr<char>, ...) -> i32
// ═══════════════════════════════════════════════════════════════

std::shared_ptr<ASTNode> Parser::parseFuncDecl(bool isExport, bool isExtern, bool isInline) {
    expect(TokenType::KW_FUNCTION, "Expected 'function'");

    auto node = ASTNode::make(isExtern ? NodeType::EXTERN_FUNC_DECL : NodeType::FUNC_DECL);
    node->name = expect(TokenType::IDENTIFIER, "Expected function name").value;
    node->isExport = isExport;
    node->isExtern = isExtern;
    node->isInline = isInline;

    // Parameters
    expect(TokenType::LPAREN, "Expected '('");
    node->params = parseParamList();
    expect(TokenType::RPAREN, "Expected ')'");

    // Check variadic
    for (auto& p : node->params) {
        if (p.isVariadic) { node->isVariadic = true; break; }
    }

    // Return type
    if (match(TokenType::ARROW) || match(TokenType::COLON)) {
        node->typeAnnotation = parseType();
    } else {
        node->typeAnnotation = TypeInfo::makeVoid();
    }

    // Body (no body for extern)
    if (!isExtern) {
        node->body = parseBlock();
    } else {
        optionalSemicolon();
    }

    return node;
}

// ═══════════════════════════════════════════════════════════════
// Structure declarations
//   structure Point { x: f64, y: f64 }
// ═══════════════════════════════════════════════════════════════

std::shared_ptr<ASTNode> Parser::parseStructDecl() {
    advance(); // skip 'structure'
    auto node = ASTNode::make(NodeType::STRUCT_DECL);
    node->name = expect(TokenType::IDENTIFIER, "Expected structure name").value;

    // Check for 'packed' attribute
    if (check(TokenType::KW_PACKED)) { advance(); node->isPacked = true; }

    expect(TokenType::LBRACE, "Expected '{'");
    node->fields = parseFieldList();
    expect(TokenType::RBRACE, "Expected '}'");
    return node;
}

// ═══════════════════════════════════════════════════════════════
// Enumeration declarations
//   enumeration Color { RED = 0, GREEN = 1, BLUE = 2 }
// ═══════════════════════════════════════════════════════════════

std::shared_ptr<ASTNode> Parser::parseEnumDecl() {
    advance(); // skip 'enumeration'
    auto node = ASTNode::make(NodeType::ENUM_DECL);
    node->name = expect(TokenType::IDENTIFIER, "Expected enum name").value;
    expect(TokenType::LBRACE, "Expected '{'");

    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        EnumVariant v;
        v.name = expect(TokenType::IDENTIFIER, "Expected variant name").value;
        if (match(TokenType::OP_ASSIGN)) {
            v.value = parseExpression();
        }
        node->variants.push_back(v);
        // Allow comma or newline separation
        match(TokenType::COMMA);
    }
    expect(TokenType::RBRACE, "Expected '}'");
    return node;
}

// ═══════════════════════════════════════════════════════════════
// Union declarations
// ═══════════════════════════════════════════════════════════════

std::shared_ptr<ASTNode> Parser::parseUnionDecl() {
    advance(); // skip 'union'
    auto node = ASTNode::make(NodeType::UNION_DECL);
    node->name = expect(TokenType::IDENTIFIER, "Expected union name").value;
    expect(TokenType::LBRACE, "Expected '{'");
    node->fields = parseFieldList();
    expect(TokenType::RBRACE, "Expected '}'");
    return node;
}

// ═══════════════════════════════════════════════════════════════
// Module declarations
//   module my_module { ... }
// ═══════════════════════════════════════════════════════════════

std::shared_ptr<ASTNode> Parser::parseModuleDecl() {
    advance(); // skip 'module'
    auto node = ASTNode::make(NodeType::MODULE_DECL);
    node->name = expect(TokenType::IDENTIFIER, "Expected module name").value;
    node->body = parseBlock();
    return node;
}

// ═══════════════════════════════════════════════════════════════
// Import statements
//   import "path/to/file.vlang"
//   import versmodule:name
// ═══════════════════════════════════════════════════════════════

std::shared_ptr<ASTNode> Parser::parseImport() {
    advance(); // skip 'import'
    auto node = ASTNode::make(NodeType::IMPORT_STMT);

    if (check(TokenType::STRING_LITERAL)) {
        node->importPath = advance().value;
    } else {
        // versmodule:name or identifier
        std::string path = expect(TokenType::IDENTIFIER, "Expected module path").value;
        while (match(TokenType::COLON)) {
            path += ":";
            if (match(TokenType::AT)) path += "@";
            path += expect(TokenType::IDENTIFIER, "Expected module name").value;
        }
        node->importPath = path;
    }

    optionalSemicolon();
    return node;
}

// ═══════════════════════════════════════════════════════════════
// Interrupt handler
//   interrupt_handler divzero() { ... }
// ═══════════════════════════════════════════════════════════════

std::shared_ptr<ASTNode> Parser::parseInterruptHandler() {
    advance(); // skip 'interrupt_handler'
    auto node = ASTNode::make(NodeType::INTERRUPT_HANDLER_DECL);
    node->name = expect(TokenType::IDENTIFIER, "Expected handler name").value;
    expect(TokenType::LPAREN, "Expected '('");
    expect(TokenType::RPAREN, "Expected ')'");
    node->body = parseBlock();
    return node;
}

// ═══════════════════════════════════════════════════════════════
// Control flow
// ═══════════════════════════════════════════════════════════════

std::shared_ptr<ASTNode> Parser::parseIfStmt() {
    advance(); // skip 'if'
    auto node = ASTNode::make(NodeType::IF_STMT);
    expect(TokenType::LPAREN, "Expected '('");
    node->condition = parseExpression();
    expect(TokenType::RPAREN, "Expected ')'");
    node->body = parseBlock();

    if (match(TokenType::KW_ELSE)) {
        if (check(TokenType::KW_IF)) {
            node->elseBody = parseIfStmt();
        } else {
            node->elseBody = parseBlock();
        }
    }
    return node;
}

std::shared_ptr<ASTNode> Parser::parseWhileStmt() {
    advance(); // skip 'while'
    auto node = ASTNode::make(NodeType::WHILE_STMT);
    expect(TokenType::LPAREN, "Expected '('");
    node->condition = parseExpression();
    expect(TokenType::RPAREN, "Expected ')'");
    node->body = parseBlock();
    return node;
}

std::shared_ptr<ASTNode> Parser::parseForStmt() {
    advance(); // skip 'for'
    auto node = ASTNode::make(NodeType::FOR_STMT);
    expect(TokenType::LPAREN, "Expected '('");

    // Init
    bool initConsumedSemicolon = false;
    if (check(TokenType::KW_DECLARE) || check(TokenType::KW_MUTABLE)) {
        TokenType dt = current().type;
        advance();
        node->init = parseVarDecl(dt);
        // parseVarDecl calls optionalSemicolon(), which already consumed ';'
        initConsumedSemicolon = true;
    } else if (!check(TokenType::SEMICOLON)) {
        node->init = parseExpression();
    }
    if (!initConsumedSemicolon) {
        expect(TokenType::SEMICOLON, "Expected ';' in for loop");
    }

    // Condition
    if (!check(TokenType::SEMICOLON)) {
        node->condition = parseExpression();
    }
    expect(TokenType::SEMICOLON, "Expected ';' in for loop");

    // Step
    if (!check(TokenType::RPAREN)) {
        node->step = parseExpression();
    }
    expect(TokenType::RPAREN, "Expected ')'");
    node->body = parseBlock();
    return node;
}

std::shared_ptr<ASTNode> Parser::parseLoopStmt() {
    advance(); // skip 'loop'
    auto node = ASTNode::make(NodeType::LOOP_STMT);
    node->body = parseBlock();
    return node;
}

std::shared_ptr<ASTNode> Parser::parseReturnStmt() {
    advance(); // skip 'return'
    auto node = ASTNode::make(NodeType::RETURN_STMT);
    if (!check(TokenType::SEMICOLON) && !check(TokenType::RBRACE)) {
        node->left = parseExpression();
    }
    optionalSemicolon();
    return node;
}

std::shared_ptr<ASTNode> Parser::parseSwitchStmt() {
    advance(); // skip 'switch'
    auto node = ASTNode::make(NodeType::SWITCH_STMT);
    expect(TokenType::LPAREN, "Expected '('");
    node->condition = parseExpression();
    expect(TokenType::RPAREN, "Expected ')'");
    expect(TokenType::LBRACE, "Expected '{'");

    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        auto caseNode = ASTNode::make(NodeType::CASE_CLAUSE);
        if (match(TokenType::KW_CASE)) {
            caseNode->left = parseExpression();
        } else if (match(TokenType::KW_DEFAULT)) {
            // default case: left is nullptr
        } else {
            error("Expected 'case' or 'default'");
        }
        expect(TokenType::COLON, "Expected ':' after case");

        // Parse case body
        caseNode->body = ASTNode::makeBlock();
        while (!check(TokenType::KW_CASE) && !check(TokenType::KW_DEFAULT) &&
               !check(TokenType::RBRACE) && !isAtEnd()) {
            caseNode->body->children.push_back(parseStatement());
        }
        node->cases.push_back(caseNode);
    }
    expect(TokenType::RBRACE, "Expected '}'");
    return node;
}

std::shared_ptr<ASTNode> Parser::parseLabelStmt() {
    advance(); // skip 'label'
    auto node = ASTNode::make(NodeType::LABEL_STMT);
    node->name = expect(TokenType::IDENTIFIER, "Expected label name").value;
    match(TokenType::COLON);
    return node;
}

std::shared_ptr<ASTNode> Parser::parseGotoStmt() {
    advance(); // skip 'goto'
    auto node = ASTNode::make(NodeType::GOTO_STMT);
    node->name = expect(TokenType::IDENTIFIER, "Expected label name").value;
    optionalSemicolon();
    return node;
}

// ═══════════════════════════════════════════════════════════════
// Assembly block
//   assembly { mov rax, 1; syscall }
// ═══════════════════════════════════════════════════════════════

std::shared_ptr<ASTNode> Parser::parseAssemblyBlock() {
    advance(); // skip 'assembly'
    auto node = ASTNode::make(NodeType::ASSEMBLY_BLOCK);
    expect(TokenType::LBRACE, "Expected '{'");

    // Parse assembly instructions as raw text or string literals
    // Format 1: mnemonic [operand [, operand [, operand]]]
    // Format 2: "full instruction as string"
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        AsmInstr instr;

        // String literal form: "mov rax, 9"
        if (check(TokenType::STRING_LITERAL)) {
            std::string raw = current().value;
            advance();
            // Parse string into mnemonic + operands
            size_t sp = raw.find(' ');
            if (sp != std::string::npos) {
                instr.mnemonic = raw.substr(0, sp);
                std::string rest = raw.substr(sp + 1);
                // Split on comma
                size_t comma = rest.find(',');
                if (comma != std::string::npos) {
                    instr.operand1 = rest.substr(0, comma);
                    // Trim leading space on second operand
                    std::string op2 = rest.substr(comma + 1);
                    while (!op2.empty() && op2[0] == ' ') op2.erase(0, 1);
                    size_t comma2 = op2.find(',');
                    if (comma2 != std::string::npos) {
                        instr.operand2 = op2.substr(0, comma2);
                        std::string op3 = op2.substr(comma2 + 1);
                        while (!op3.empty() && op3[0] == ' ') op3.erase(0, 1);
                        instr.operand3 = op3;
                    } else {
                        instr.operand2 = op2;
                    }
                } else {
                    instr.operand1 = rest;
                }
            } else {
                instr.mnemonic = raw;
            }
            node->asmInstructions.push_back(instr);
            match(TokenType::SEMICOLON);
            continue;
        }

        instr.mnemonic = expect(TokenType::IDENTIFIER, "Expected assembly mnemonic").value;

        // Parse operands (simplified: just collect tokens until semicolon/newline/})
        if (!check(TokenType::SEMICOLON) && !check(TokenType::RBRACE)) {
            // First operand
            std::string op1;
            while (!check(TokenType::COMMA) && !check(TokenType::SEMICOLON) &&
                   !check(TokenType::RBRACE) && !isAtEnd()) {
                op1 += current().value;
                advance();
                if (check(TokenType::COMMA) || check(TokenType::SEMICOLON) || check(TokenType::RBRACE)) break;
                op1 += " ";
            }
            instr.operand1 = op1;

            // Second operand
            if (match(TokenType::COMMA)) {
                std::string op2;
                while (!check(TokenType::COMMA) && !check(TokenType::SEMICOLON) &&
                       !check(TokenType::RBRACE) && !isAtEnd()) {
                    op2 += current().value;
                    advance();
                    if (check(TokenType::COMMA) || check(TokenType::SEMICOLON) || check(TokenType::RBRACE)) break;
                    op2 += " ";
                }
                instr.operand2 = op2;
            }

            // Third operand
            if (match(TokenType::COMMA)) {
                std::string op3;
                while (!check(TokenType::SEMICOLON) && !check(TokenType::RBRACE) && !isAtEnd()) {
                    op3 += current().value;
                    advance();
                    if (check(TokenType::SEMICOLON) || check(TokenType::RBRACE)) break;
                    op3 += " ";
                }
                instr.operand3 = op3;
            }
        }

        node->asmInstructions.push_back(instr);
        match(TokenType::SEMICOLON);
    }
    expect(TokenType::RBRACE, "Expected '}'");
    return node;
}

// ═══════════════════════════════════════════════════════════════
// Directive statement
//   directive bootloader
//   directive bits 16
//   directive origin 0x7C00
//   directive section ".text"
// ═══════════════════════════════════════════════════════════════

std::shared_ptr<ASTNode> Parser::parseDirective() {
    advance(); // skip 'directive'
    auto node = ASTNode::make(NodeType::DIRECTIVE_STMT);

    if (match(TokenType::KW_BOOTLOADER)) {
        node->directive = "bootloader";
    } else {
        node->directive = expect(TokenType::IDENTIFIER, "Expected directive name").value;
        if (!check(TokenType::SEMICOLON) && !check(TokenType::RBRACE) && !isAtEnd()) {
            node->left = parseExpression();
        }
    }

    optionalSemicolon();
    return node;
}

// ═══════════════════════════════════════════════════════════════
// When (conditional compilation)
//   when platform == "windows" { ... }
// ═══════════════════════════════════════════════════════════════

std::shared_ptr<ASTNode> Parser::parseWhenStmt() {
    advance(); // skip 'when'
    auto node = ASTNode::make(NodeType::WHEN_STMT);
    node->condition = parseExpression();
    node->body = parseBlock();
    return node;
}

// ═══════════════════════════════════════════════════════════════
// Type parsing
// ═══════════════════════════════════════════════════════════════

TypePtr Parser::parseType() {
    // Pointer type: ptr<T>
    if (check(TokenType::KW_PTR)) return parsePtrType();

    // Array type: array<T, N>
    if (check(TokenType::KW_ARRAY)) return parseArrayType();

    // Function type: function(T1, T2) -> T
    if (check(TokenType::KW_FUNCTION)) {
        advance();
        expect(TokenType::LPAREN, "Expected '('");
        std::vector<TypePtr> paramTypes;
        while (!check(TokenType::RPAREN) && !isAtEnd()) {
            paramTypes.push_back(parseType());
            if (!match(TokenType::COMMA)) break;
        }
        expect(TokenType::RPAREN, "Expected ')'");
        TypePtr retType = TypeInfo::makeVoid();
        if (match(TokenType::ARROW)) {
            retType = parseType();
        }
        return TypeInfo::makeFunction(retType, paramTypes);
    }

    // Primitive types
    switch (current().type) {
        case TokenType::KW_U8:    advance(); return TypeInfo::makeU8();
        case TokenType::KW_U16:   advance(); return TypeInfo::makeU16();
        case TokenType::KW_U32:   advance(); return TypeInfo::makeU32();
        case TokenType::KW_U64:   advance(); return TypeInfo::makeU64();
        case TokenType::KW_I8:    advance(); return TypeInfo::makeI8();
        case TokenType::KW_I16:   advance(); return TypeInfo::makeI16();
        case TokenType::KW_I32:   advance(); return TypeInfo::makeI32();
        case TokenType::KW_I64:   advance(); return TypeInfo::makeI64();
        case TokenType::KW_F32:   advance(); return TypeInfo::makeF32();
        case TokenType::KW_F64:   advance(); return TypeInfo::makeF64();
        case TokenType::KW_BOOL:  advance(); return TypeInfo::makeBool();
        case TokenType::KW_VOID:  advance(); return TypeInfo::makeVoid();
        case TokenType::KW_CHAR:  advance(); return TypeInfo::makeChar();
        case TokenType::KW_USIZE: advance(); return TypeInfo::makeUsize();
        case TokenType::KW_ISIZE: advance(); return TypeInfo::makeIsize();
        default: break;
    }

    // Named type (struct, enum, etc.)
    if (check(TokenType::IDENTIFIER)) {
        std::string name = advance().value;
        return TypeInfo::makeNamed(name);
    }

    error("Expected type");
    return TypeInfo::makeVoid();
}

TypePtr Parser::parsePtrType() {
    advance(); // skip 'ptr'
    expect(TokenType::OP_LT, "Expected '<' after 'ptr'");
    TypePtr inner = parseType();
    expectCloseAngle();
    return TypeInfo::makePtr(inner);
}

TypePtr Parser::parseArrayType() {
    advance(); // skip 'array'
    expect(TokenType::OP_LT, "Expected '<' after 'array'");
    TypePtr inner = parseType();
    expect(TokenType::COMMA, "Expected ',' in array type");
    size_t size = (size_t)expect(TokenType::INTEGER_LITERAL, "Expected array size").intValue;
    expectCloseAngle();
    return TypeInfo::makeArray(inner, size);
}

// ═══════════════════════════════════════════════════════════════
// Parameter and field lists
// ═══════════════════════════════════════════════════════════════

std::vector<ParamDef> Parser::parseParamList() {
    std::vector<ParamDef> params;
    while (!check(TokenType::RPAREN) && !isAtEnd()) {
        // Check for variadic: ...
        if (check(TokenType::ELLIPSIS)) {
            advance();
            ParamDef p;
            p.name = "...";
            p.isVariadic = true;
            params.push_back(p);
            break;
        }

        ParamDef p;
        p.name = expect(TokenType::IDENTIFIER, "Expected parameter name").value;
        expect(TokenType::COLON, "Expected ':' after parameter name");
        p.type = parseType();

        // Default value
        if (match(TokenType::OP_ASSIGN)) {
            p.defaultValue = parseExpression();
        }

        params.push_back(p);
        if (!match(TokenType::COMMA)) break;
    }
    return params;
}

std::vector<FieldDef> Parser::parseFieldList() {
    std::vector<FieldDef> fields;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        FieldDef f;
        // Allow keywords as field names (e.g., 'section')
        if (check(TokenType::IDENTIFIER) || current().value.size() > 0) {
            f.name = current().value;
            advance();
        } else {
            f.name = expect(TokenType::IDENTIFIER, "Expected field name").value;
        }
        expect(TokenType::COLON, "Expected ':' after field name");
        f.type = parseType();

        // Bit width for bitfields: name: u32 : 5
        if (match(TokenType::COLON)) {
            f.bitWidth = (int)expect(TokenType::INTEGER_LITERAL, "Expected bit width").intValue;
        }

        // Default value
        if (match(TokenType::OP_ASSIGN)) {
            f.defaultValue = parseExpression();
        }

        fields.push_back(f);
        // Allow comma or newline separation
        match(TokenType::COMMA);
    }
    return fields;
}

// ═══════════════════════════════════════════════════════════════
// Expression parsing - Precedence Climbing
// ═══════════════════════════════════════════════════════════════

std::shared_ptr<ASTNode> Parser::parseExpression() {
    return parseAssignment();
}

std::shared_ptr<ASTNode> Parser::parseAssignment() {
    auto left = parseTernary();

    if (check(TokenType::OP_ASSIGN) || current().isAssignOp()) {
        std::string op = advance().value;
        auto right = parseAssignment();

        if (op == "=") {
            auto node = ASTNode::make(NodeType::ASSIGNMENT_EXPR);
            node->left = left;
            node->right = right;
            return node;
        } else {
            auto node = ASTNode::make(NodeType::COMPOUND_ASSIGNMENT_EXPR);
            node->op = op;
            node->left = left;
            node->right = right;
            return node;
        }
    }
    return left;
}

std::shared_ptr<ASTNode> Parser::parseTernary() {
    auto expr = parseLogicalOr();
    if (match(TokenType::QUESTION)) {
        auto node = ASTNode::make(NodeType::TERNARY_EXPR);
        node->condition = expr;
        node->left = parseExpression();
        expect(TokenType::COLON, "Expected ':' in ternary expression");
        node->right = parseExpression();
        return node;
    }
    return expr;
}

std::shared_ptr<ASTNode> Parser::parseLogicalOr() {
    auto left = parseLogicalAnd();
    while (match(TokenType::OP_OR)) {
        auto right = parseLogicalAnd();
        left = ASTNode::makeBinaryExpr("||", left, right);
    }
    return left;
}

std::shared_ptr<ASTNode> Parser::parseLogicalAnd() {
    auto left = parseBitwiseOr();
    while (match(TokenType::OP_AND)) {
        auto right = parseBitwiseOr();
        left = ASTNode::makeBinaryExpr("&&", left, right);
    }
    return left;
}

std::shared_ptr<ASTNode> Parser::parseBitwiseOr() {
    auto left = parseBitwiseXor();
    while (match(TokenType::OP_BIT_OR)) {
        auto right = parseBitwiseXor();
        left = ASTNode::makeBinaryExpr("|", left, right);
    }
    return left;
}

std::shared_ptr<ASTNode> Parser::parseBitwiseXor() {
    auto left = parseBitwiseAnd();
    while (match(TokenType::OP_BIT_XOR)) {
        auto right = parseBitwiseAnd();
        left = ASTNode::makeBinaryExpr("^", left, right);
    }
    return left;
}

std::shared_ptr<ASTNode> Parser::parseBitwiseAnd() {
    auto left = parseEquality();
    while (match(TokenType::OP_BIT_AND)) {
        auto right = parseEquality();
        left = ASTNode::makeBinaryExpr("&", left, right);
    }
    return left;
}

std::shared_ptr<ASTNode> Parser::parseEquality() {
    auto left = parseComparison();
    while (check(TokenType::OP_EQ) || check(TokenType::OP_NEQ)) {
        std::string op = advance().value;
        auto right = parseComparison();
        left = ASTNode::makeBinaryExpr(op, left, right);
    }
    return left;
}

std::shared_ptr<ASTNode> Parser::parseComparison() {
    auto left = parseShift();
    while (check(TokenType::OP_LT) || check(TokenType::OP_GT) ||
           check(TokenType::OP_LTE) || check(TokenType::OP_GTE)) {
        std::string op = advance().value;
        auto right = parseShift();
        left = ASTNode::makeBinaryExpr(op, left, right);
    }
    return left;
}

std::shared_ptr<ASTNode> Parser::parseShift() {
    auto left = parseAddition();
    while (check(TokenType::OP_SHL) || check(TokenType::OP_SHR)) {
        std::string op = advance().value;
        auto right = parseAddition();
        left = ASTNode::makeBinaryExpr(op, left, right);
    }
    return left;
}

std::shared_ptr<ASTNode> Parser::parseAddition() {
    auto left = parseMultiplication();
    while (check(TokenType::OP_PLUS) || check(TokenType::OP_MINUS)) {
        std::string op = advance().value;
        auto right = parseMultiplication();
        left = ASTNode::makeBinaryExpr(op, left, right);
    }
    return left;
}

std::shared_ptr<ASTNode> Parser::parseMultiplication() {
    auto left = parseUnary();
    while (check(TokenType::OP_STAR) || check(TokenType::OP_SLASH) || check(TokenType::OP_PERCENT)) {
        std::string op = advance().value;
        auto right = parseUnary();
        left = ASTNode::makeBinaryExpr(op, left, right);
    }
    return left;
}

std::shared_ptr<ASTNode> Parser::parseUnary() {
    if (check(TokenType::OP_NOT) || check(TokenType::OP_MINUS) ||
        check(TokenType::OP_BIT_NOT) || check(TokenType::OP_BIT_AND) ||
        check(TokenType::OP_STAR)) {
        std::string op = advance().value;
        auto operand = parseUnary();
        return ASTNode::makeUnaryExpr(op, operand);
    }

    // Pre-increment/decrement
    if (check(TokenType::OP_INCREMENT) || check(TokenType::OP_DECREMENT)) {
        std::string op = advance().value;
        auto operand = parseUnary();
        auto node = ASTNode::makeUnaryExpr(op, operand);
        return node;
    }

    return parsePostfix();
}

std::shared_ptr<ASTNode> Parser::parsePostfix() {
    auto expr = parsePrimary();

    while (true) {
        // Member access: expr.field
        if (match(TokenType::DOT)) {
            // Check for special member: memory.xxx, port.xxx, register.xxx
            auto node = ASTNode::make(NodeType::MEMBER_EXPR);
            node->object = expr;
            // Allow keywords as member names (e.g., obj.section)
            node->name = current().value;
            advance();

            // Check if it's a method call: obj.method(args)
            if (check(TokenType::LPAREN)) {
                advance();
                auto call = ASTNode::make(NodeType::CALL_EXPR);
                call->object = expr;
                call->name = node->name;
                while (!check(TokenType::RPAREN) && !isAtEnd()) {
                    call->arguments.push_back(parseExpression());
                    if (!match(TokenType::COMMA)) break;
                }
                expect(TokenType::RPAREN, "Expected ')'");
                expr = call;
            } else {
                expr = node;
            }
            continue;
        }

        // Scope resolution: module::item
        if (match(TokenType::DOUBLE_COLON)) {
            auto node = ASTNode::make(NodeType::SCOPE_RESOLUTION_EXPR);
            node->object = expr;
            node->name = expect(TokenType::IDENTIFIER, "Expected name after '::'").value;
            expr = node;
            continue;
        }

        // Index: expr[index]
        if (match(TokenType::LBRACKET)) {
            auto node = ASTNode::make(NodeType::INDEX_EXPR);
            node->object = expr;
            node->left = parseExpression();
            expect(TokenType::RBRACKET, "Expected ']'");
            expr = node;
            continue;
        }

        // Function call: expr(args)
        if (check(TokenType::LPAREN) && expr->type == NodeType::IDENTIFIER_EXPR) {
            advance();
            auto call = ASTNode::makeCallExpr(expr, {});
            while (!check(TokenType::RPAREN) && !isAtEnd()) {
                call->arguments.push_back(parseExpression());
                if (!match(TokenType::COMMA)) break;
            }
            expect(TokenType::RPAREN, "Expected ')'");
            expr = call;
            continue;
        }

        // Post-increment/decrement
        if (check(TokenType::OP_INCREMENT) || check(TokenType::OP_DECREMENT)) {
            std::string op = advance().value;
            auto node = ASTNode::make(NodeType::POSTFIX_EXPR);
            node->op = op;
            node->left = expr;
            expr = node;
            continue;
        }

        break;
    }

    return expr;
}

// ═══════════════════════════════════════════════════════════════
// Primary expressions
// ═══════════════════════════════════════════════════════════════

std::shared_ptr<ASTNode> Parser::parsePrimary() {
    // Integer literal
    if (check(TokenType::INTEGER_LITERAL) || check(TokenType::HEX_LITERAL) ||
        check(TokenType::BINARY_LITERAL) || check(TokenType::OCTAL_LITERAL)) {
        const Token& tok = advance();
        return ASTNode::makeIntLiteral(tok.intValue);
    }

    // Float literal
    if (check(TokenType::FLOAT_LITERAL)) {
        const Token& tok = advance();
        return ASTNode::makeFloatLiteral(tok.floatValue);
    }

    // String literal
    if (check(TokenType::STRING_LITERAL)) {
        return ASTNode::makeStringLiteral(advance().value);
    }

    // Char literal
    if (check(TokenType::CHAR_LITERAL)) {
        const Token& tok = advance();
        return ASTNode::makeCharLiteral(tok.value.empty() ? '\0' : tok.value[0]);
    }

    // Boolean literals
    if (check(TokenType::KW_TRUE)) { advance(); return ASTNode::makeBoolLiteral(true); }
    if (check(TokenType::KW_FALSE)) { advance(); return ASTNode::makeBoolLiteral(false); }

    // Null literals
    if (check(TokenType::KW_NULL) || check(TokenType::KW_NULLPTR)) {
        advance();
        auto node = ASTNode::make(NodeType::NULL_LITERAL);
        return node;
    }

    // Parenthesized expression
    if (match(TokenType::LPAREN)) {
        auto expr = parseExpression();
        expect(TokenType::RPAREN, "Expected ')'");
        return expr;
    }

    // Special expressions
    if (check(TokenType::KW_CAST)) return parseCastExpr();
    if (check(TokenType::KW_SIZEOF)) return parseSizeofExpr();
    if (check(TokenType::KW_ADDRESS_OF)) return parseAddressOfExpr();
    if (check(TokenType::KW_DEREFERENCE)) return parseDerefExpr();
    if (check(TokenType::KW_SYSCALL)) return parseSyscallExpr();

    // Bitcast: bitcast<Type>(expr)
    if (check(TokenType::KW_BITCAST)) {
        advance();
        expect(TokenType::OP_LT, "Expected '<'");
        auto castType = parseType();
        expectCloseAngle();
        expect(TokenType::LPAREN, "Expected '('");
        auto expr = parseExpression();
        expect(TokenType::RPAREN, "Expected ')'");
        auto node = ASTNode::make(NodeType::BITCAST_EXPR);
        node->castType = castType;
        node->left = expr;
        return node;
    }

    // Alignof
    if (check(TokenType::KW_ALIGNOF)) {
        advance();
        expect(TokenType::LPAREN, "Expected '('");
        auto type = parseType();
        expect(TokenType::RPAREN, "Expected ')'");
        auto node = ASTNode::make(NodeType::ALIGNOF_EXPR);
        node->typeAnnotation = type;
        return node;
    }

    // Typeof
    if (check(TokenType::KW_TYPEOF)) {
        advance();
        expect(TokenType::LPAREN, "Expected '('");
        auto expr = parseExpression();
        expect(TokenType::RPAREN, "Expected ')'");
        auto node = ASTNode::make(NodeType::TYPEOF_EXPR);
        node->left = expr;
        return node;
    }

    // Memory operations: memory.xxx(...)
    if (check(TokenType::KW_MEMORY)) {
        advance();
        expect(TokenType::DOT, "Expected '.' after 'memory'");
        auto node = ASTNode::make(NodeType::MEMORY_EXPR);
        node->name = expect(TokenType::IDENTIFIER, "Expected memory operation").value;
        if (match(TokenType::LPAREN)) {
            while (!check(TokenType::RPAREN) && !isAtEnd()) {
                node->arguments.push_back(parseExpression());
                if (!match(TokenType::COMMA)) break;
            }
            expect(TokenType::RPAREN, "Expected ')'");
        }
        return node;
    }

    // Port operations: port.xxx(...)
    if (check(TokenType::KW_PORT)) {
        advance();
        expect(TokenType::DOT, "Expected '.' after 'port'");
        auto node = ASTNode::make(NodeType::PORT_EXPR);
        node->name = expect(TokenType::IDENTIFIER, "Expected port operation").value;
        if (match(TokenType::LPAREN)) {
            while (!check(TokenType::RPAREN) && !isAtEnd()) {
                node->arguments.push_back(parseExpression());
                if (!match(TokenType::COMMA)) break;
            }
            expect(TokenType::RPAREN, "Expected ')'");
        }
        return node;
    }

    // Register access: register.rax
    if (check(TokenType::KW_REGISTER)) {
        advance();
        expect(TokenType::DOT, "Expected '.' after 'register'");
        auto node = ASTNode::make(NodeType::REGISTER_EXPR);
        node->name = expect(TokenType::IDENTIFIER, "Expected register name").value;
        return node;
    }

    // Platform keyword (used in when conditions)
    if (check(TokenType::KW_PLATFORM)) {
        advance();
        return ASTNode::makeIdentifier("platform");
    }

    // Identifier
    if (check(TokenType::IDENTIFIER)) {
        std::string name = advance().value;
        return ASTNode::makeIdentifier(name);
    }

    error("Unexpected token: '" + current().value + "'");
    return nullptr;
}

// ═══════════════════════════════════════════════════════════════
// Special expressions
// ═══════════════════════════════════════════════════════════════

std::shared_ptr<ASTNode> Parser::parseCastExpr() {
    advance(); // skip 'cast'
    expect(TokenType::OP_LT, "Expected '<' after 'cast'");
    auto castType = parseType();
    expectCloseAngle();
    expect(TokenType::LPAREN, "Expected '('");
    auto expr = parseExpression();
    expect(TokenType::RPAREN, "Expected ')'");
    auto node = ASTNode::make(NodeType::CAST_EXPR);
    node->castType = castType;
    node->left = expr;
    return node;
}

std::shared_ptr<ASTNode> Parser::parseSizeofExpr() {
    advance(); // skip 'sizeof'
    expect(TokenType::LPAREN, "Expected '('");
    auto node = ASTNode::make(NodeType::SIZEOF_EXPR);

    // Could be a type or an expression
    // Try type first (if it starts with a type keyword)
    if (current().isTypeKeyword()) {
        node->typeAnnotation = parseType();
    } else {
        node->left = parseExpression();
    }
    expect(TokenType::RPAREN, "Expected ')'");
    return node;
}

std::shared_ptr<ASTNode> Parser::parseAddressOfExpr() {
    advance(); // skip 'address_of'
    expect(TokenType::LPAREN, "Expected '('");
    auto expr = parseExpression();
    expect(TokenType::RPAREN, "Expected ')'");
    auto node = ASTNode::make(NodeType::ADDRESS_OF_EXPR);
    node->left = expr;
    return node;
}

std::shared_ptr<ASTNode> Parser::parseDerefExpr() {
    advance(); // skip 'dereference'
    expect(TokenType::LPAREN, "Expected '('");
    auto expr = parseExpression();
    expect(TokenType::RPAREN, "Expected ')'");
    auto node = ASTNode::make(NodeType::DEREF_EXPR);
    node->left = expr;
    return node;
}

std::shared_ptr<ASTNode> Parser::parseSyscallExpr() {
    advance(); // skip 'syscall'
    auto node = ASTNode::make(NodeType::SYSCALL_EXPR);
    expect(TokenType::LPAREN, "Expected '('");
    while (!check(TokenType::RPAREN) && !isAtEnd()) {
        node->arguments.push_back(parseExpression());
        if (!match(TokenType::COMMA)) break;
    }
    expect(TokenType::RPAREN, "Expected ')'");
    return node;
}

} // namespace verslang
