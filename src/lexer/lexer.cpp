#include "lexer.h"
#include <stdexcept>
#include <unordered_map>

namespace verslang {

// ═══════════════════════════════════════════════════════════════
// Keyword lookup table
// ═══════════════════════════════════════════════════════════════
static const std::unordered_map<std::string, TokenType> keywords = {
    // Declaration keywords
    {"declare",           TokenType::KW_DECLARE},
    {"constant",          TokenType::KW_CONSTANT},
    {"mutable",           TokenType::KW_MUTABLE},
    {"function",          TokenType::KW_FUNCTION},
    {"external",          TokenType::KW_EXTERNAL},
    {"export",            TokenType::KW_EXPORT},
    {"return",            TokenType::KW_RETURN},
    {"inline",            TokenType::KW_INLINE},

    // Composite types
    {"structure",         TokenType::KW_STRUCTURE},
    {"enumeration",       TokenType::KW_ENUMERATION},
    {"union",             TokenType::KW_UNION},
    {"bitfield",          TokenType::KW_BITFIELD},

    // Module keywords
    {"module",            TokenType::KW_MODULE},
    {"import",            TokenType::KW_IMPORT},

    // Control flow
    {"if",                TokenType::KW_IF},
    {"else",              TokenType::KW_ELSE},
    {"while",             TokenType::KW_WHILE},
    {"for",               TokenType::KW_FOR},
    {"loop",              TokenType::KW_LOOP},
    {"break",             TokenType::KW_BREAK},
    {"continue",          TokenType::KW_CONTINUE},
    {"switch",            TokenType::KW_SWITCH},
    {"case",              TokenType::KW_CASE},
    {"default",           TokenType::KW_DEFAULT},
    {"goto",              TokenType::KW_GOTO},
    {"label",             TokenType::KW_LABEL},

    // Low-level keywords
    {"assembly",          TokenType::KW_ASSEMBLY},
    {"syscall",           TokenType::KW_SYSCALL},
    {"interrupt",         TokenType::KW_INTERRUPT},
    {"interrupt_handler", TokenType::KW_INTERRUPT_HANDLER},
    {"directive",         TokenType::KW_DIRECTIVE},
    {"bootloader",        TokenType::KW_BOOTLOADER},
    {"section",           TokenType::KW_SECTION},
    {"global",            TokenType::KW_GLOBAL},
    {"align",             TokenType::KW_ALIGN},

    // Memory & hardware
    {"memory",            TokenType::KW_MEMORY},
    {"port",              TokenType::KW_PORT},
    {"register",          TokenType::KW_REGISTER},
    {"volatile",          TokenType::KW_VOLATILE},
    {"packed",            TokenType::KW_PACKED},
    {"static",            TokenType::KW_STATIC},

    // Type operations
    {"cast",              TokenType::KW_CAST},
    {"sizeof",            TokenType::KW_SIZEOF},
    {"alignof",           TokenType::KW_ALIGNOF},
    {"typeof",            TokenType::KW_TYPEOF},
    {"address_of",        TokenType::KW_ADDRESS_OF},
    {"dereference",       TokenType::KW_DEREFERENCE},
    {"offset_of",         TokenType::KW_OFFSET_OF},
    {"bitcast",           TokenType::KW_BITCAST},

    // Conditional compilation
    {"when",              TokenType::KW_WHEN},
    {"platform",          TokenType::KW_PLATFORM},

    // Primitive types
    {"u8",                TokenType::KW_U8},
    {"u16",               TokenType::KW_U16},
    {"u32",               TokenType::KW_U32},
    {"u64",               TokenType::KW_U64},
    {"i8",                TokenType::KW_I8},
    {"i16",               TokenType::KW_I16},
    {"i32",               TokenType::KW_I32},
    {"i64",               TokenType::KW_I64},
    {"f32",               TokenType::KW_F32},
    {"f64",               TokenType::KW_F64},
    {"bool",              TokenType::KW_BOOL},
    {"void",              TokenType::KW_VOID},
    {"char",              TokenType::KW_CHAR},
    {"usize",             TokenType::KW_USIZE},
    {"isize",             TokenType::KW_ISIZE},
    {"ptr",               TokenType::KW_PTR},
    {"array",             TokenType::KW_ARRAY},

    // Values
    {"true",              TokenType::KW_TRUE},
    {"false",             TokenType::KW_FALSE},
    {"null",              TokenType::KW_NULL},
    {"nullptr",           TokenType::KW_NULLPTR},
};

// ═══════════════════════════════════════════════════════════════
// Lexer Implementation
// ═══════════════════════════════════════════════════════════════

Lexer::Lexer(const std::string& source, const std::string& filename)
    : source_(source), filename_(filename) {}

char Lexer::current() const {
    if (pos_ >= source_.size()) return '\0';
    return source_[pos_];
}

char Lexer::peek(int offset) const {
    size_t idx = pos_ + offset;
    if (idx >= source_.size()) return '\0';
    return source_[idx];
}

char Lexer::advance() {
    char c = current();
    pos_++;
    if (c == '\n') { line_++; column_ = 1; }
    else { column_++; }
    return c;
}

bool Lexer::isAtEnd() const {
    return pos_ >= source_.size();
}

void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = current();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peek() == '/') {
            // Single-line comment
            while (!isAtEnd() && current() != '\n') advance();
        } else if (c == '/' && peek() == '*') {
            // Multi-line comment
            advance(); advance();
            while (!isAtEnd()) {
                if (current() == '*' && peek() == '/') {
                    advance(); advance();
                    break;
                }
                advance();
            }
        } else if (c == '\xC2' && peek() == '\xA7') {
            // § (section sign) single-line comment
            advance(); advance();
            while (!isAtEnd() && current() != '\n') advance();
        } else {
            break;
        }
    }
}

Token Lexer::makeToken(TokenType type, const std::string& value) {
    return Token(type, value, filename_, line_, column_);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        Token tok = nextToken();
        tokens.push_back(tok);
        if (tok.type == TokenType::TOKEN_EOF) break;
    }
    return tokens;
}

Token Lexer::nextToken() {
    skipWhitespace();
    if (isAtEnd()) return makeToken(TokenType::TOKEN_EOF, "");

    char c = current();

    // String literal
    if (c == '"') return scanString();

    // Char literal
    if (c == '\'') return scanChar();

    // Number literal
    if (std::isdigit(c)) return scanNumber();

    // Identifier or keyword
    if (std::isalpha(c) || c == '_') return scanIdentifierOrKeyword();

    // Operators and punctuation
    return scanOperator();
}

Token Lexer::scanString() {
    int startLine = line_, startCol = column_;
    advance(); // skip opening "
    std::string value;
    while (!isAtEnd() && current() != '"') {
        if (current() == '\\') {
            advance();
            switch (current()) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '0': value += '\0'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                case 'x': {
                    // Hex escape: \x41
                    advance();
                    std::string hex;
                    for (int i = 0; i < 2 && !isAtEnd() && std::isxdigit(current()); i++) {
                        hex += current(); advance();
                    }
                    value += (char)std::stoi(hex, nullptr, 16);
                    continue;
                }
                default: value += '\\'; value += current(); break;
            }
        } else {
            value += current();
        }
        advance();
    }
    if (!isAtEnd()) advance(); // skip closing "
    Token tok(TokenType::STRING_LITERAL, value, filename_, startLine, startCol);
    return tok;
}

Token Lexer::scanChar() {
    int startLine = line_, startCol = column_;
    advance(); // skip opening '
    char val = 0;
    if (current() == '\\') {
        advance();
        switch (current()) {
            case 'n': val = '\n'; break;
            case 't': val = '\t'; break;
            case 'r': val = '\r'; break;
            case '0': val = '\0'; break;
            case '\\': val = '\\'; break;
            case '\'': val = '\''; break;
            default: val = current(); break;
        }
    } else {
        val = current();
    }
    advance();
    if (current() == '\'') advance();
    Token tok(TokenType::CHAR_LITERAL, std::string(1, val), filename_, startLine, startCol);
    tok.intValue = val;
    return tok;
}

Token Lexer::scanNumber() {
    int startLine = line_, startCol = column_;
    std::string value;
    TokenType type = TokenType::INTEGER_LITERAL;

    // Check for hex, binary, octal prefixes
    if (current() == '0' && !isAtEnd()) {
        char next = peek();
        if (next == 'x' || next == 'X') {
            // Hexadecimal
            value += advance(); value += advance();
            while (!isAtEnd() && (std::isxdigit(current()) || current() == '_')) {
                if (current() != '_') value += current();
                advance();
            }
            Token tok(TokenType::HEX_LITERAL, value, filename_, startLine, startCol);
            tok.intValue = static_cast<int64_t>(std::stoull(value, nullptr, 16));
            return tok;
        }
        if (next == 'b' || next == 'B') {
            // Binary
            value += advance(); value += advance();
            while (!isAtEnd() && (current() == '0' || current() == '1' || current() == '_')) {
                if (current() != '_') value += current();
                advance();
            }
            Token tok(TokenType::BINARY_LITERAL, value, filename_, startLine, startCol);
            tok.intValue = static_cast<int64_t>(std::stoull(value.substr(2), nullptr, 2));
            return tok;
        }
        if (next == 'o' || next == 'O') {
            // Octal
            value += advance(); value += advance();
            while (!isAtEnd() && current() >= '0' && current() <= '7') {
                value += current(); advance();
            }
            Token tok(TokenType::OCTAL_LITERAL, value, filename_, startLine, startCol);
            tok.intValue = static_cast<int64_t>(std::stoull(value.substr(2), nullptr, 8));
            return tok;
        }
    }

    // Decimal integer or float
    while (!isAtEnd() && (std::isdigit(current()) || current() == '_')) {
        if (current() != '_') value += current();
        advance();
    }

    // Check for float
    if (current() == '.' && std::isdigit(peek())) {
        type = TokenType::FLOAT_LITERAL;
        value += advance(); // .
        while (!isAtEnd() && (std::isdigit(current()) || current() == '_')) {
            if (current() != '_') value += current();
            advance();
        }
    }

    // Check for exponent
    if (current() == 'e' || current() == 'E') {
        type = TokenType::FLOAT_LITERAL;
        value += advance();
        if (current() == '+' || current() == '-') value += advance();
        while (!isAtEnd() && std::isdigit(current())) {
            value += advance();
        }
    }

    Token tok(type, value, filename_, startLine, startCol);
    if (type == TokenType::FLOAT_LITERAL) {
        tok.floatValue = std::stod(value);
    } else {
        tok.intValue = static_cast<int64_t>(std::stoull(value));
    }
    return tok;
}

Token Lexer::scanIdentifierOrKeyword() {
    int startLine = line_, startCol = column_;
    std::string value;
    while (!isAtEnd() && (std::isalnum(current()) || current() == '_')) {
        value += current();
        advance();
    }

    TokenType type = lookupKeyword(value);
    Token tok(type, value, filename_, startLine, startCol);

    // Set boolean values for true/false
    if (type == TokenType::KW_TRUE) tok.intValue = 1;
    if (type == TokenType::KW_FALSE) tok.intValue = 0;

    return tok;
}

TokenType Lexer::lookupKeyword(const std::string& word) const {
    auto it = keywords.find(word);
    if (it != keywords.end()) return it->second;
    return TokenType::IDENTIFIER;
}

Token Lexer::scanOperator() {
    int startLine = line_, startCol = column_;
    char c = advance();

    switch (c) {
        case '(': return Token(TokenType::LPAREN, "(", filename_, startLine, startCol);
        case ')': return Token(TokenType::RPAREN, ")", filename_, startLine, startCol);
        case '{': return Token(TokenType::LBRACE, "{", filename_, startLine, startCol);
        case '}': return Token(TokenType::RBRACE, "}", filename_, startLine, startCol);
        case '[': return Token(TokenType::LBRACKET, "[", filename_, startLine, startCol);
        case ']': return Token(TokenType::RBRACKET, "]", filename_, startLine, startCol);
        case ';': return Token(TokenType::SEMICOLON, ";", filename_, startLine, startCol);
        case ',': return Token(TokenType::COMMA, ",", filename_, startLine, startCol);
        case '~': return Token(TokenType::OP_BIT_NOT, "~", filename_, startLine, startCol);
        case '?': return Token(TokenType::QUESTION, "?", filename_, startLine, startCol);
        case '@': return Token(TokenType::AT, "@", filename_, startLine, startCol);
        case '#': return Token(TokenType::HASH, "#", filename_, startLine, startCol);

        case ':':
            if (current() == ':') { advance(); return Token(TokenType::DOUBLE_COLON, "::", filename_, startLine, startCol); }
            return Token(TokenType::COLON, ":", filename_, startLine, startCol);

        case '.':
            if (current() == '.' && peek() == '.') { advance(); advance(); return Token(TokenType::ELLIPSIS, "...", filename_, startLine, startCol); }
            return Token(TokenType::DOT, ".", filename_, startLine, startCol);

        case '+':
            if (current() == '+') { advance(); return Token(TokenType::OP_INCREMENT, "++", filename_, startLine, startCol); }
            if (current() == '=') { advance(); return Token(TokenType::OP_PLUS_ASSIGN, "+=", filename_, startLine, startCol); }
            return Token(TokenType::OP_PLUS, "+", filename_, startLine, startCol);

        case '-':
            if (current() == '>') { advance(); return Token(TokenType::ARROW, "->", filename_, startLine, startCol); }
            if (current() == '-') { advance(); return Token(TokenType::OP_DECREMENT, "--", filename_, startLine, startCol); }
            if (current() == '=') { advance(); return Token(TokenType::OP_MINUS_ASSIGN, "-=", filename_, startLine, startCol); }
            return Token(TokenType::OP_MINUS, "-", filename_, startLine, startCol);

        case '*':
            if (current() == '=') { advance(); return Token(TokenType::OP_STAR_ASSIGN, "*=", filename_, startLine, startCol); }
            return Token(TokenType::OP_STAR, "*", filename_, startLine, startCol);

        case '/':
            if (current() == '=') { advance(); return Token(TokenType::OP_SLASH_ASSIGN, "/=", filename_, startLine, startCol); }
            return Token(TokenType::OP_SLASH, "/", filename_, startLine, startCol);

        case '%':
            if (current() == '=') { advance(); return Token(TokenType::OP_PERCENT_ASSIGN, "%=", filename_, startLine, startCol); }
            return Token(TokenType::OP_PERCENT, "%", filename_, startLine, startCol);

        case '=':
            if (current() == '=') { advance(); return Token(TokenType::OP_EQ, "==", filename_, startLine, startCol); }
            return Token(TokenType::OP_ASSIGN, "=", filename_, startLine, startCol);

        case '!':
            if (current() == '=') { advance(); return Token(TokenType::OP_NEQ, "!=", filename_, startLine, startCol); }
            return Token(TokenType::OP_NOT, "!", filename_, startLine, startCol);

        case '<':
            if (current() == '<') {
                advance();
                if (current() == '=') { advance(); return Token(TokenType::OP_SHL_ASSIGN, "<<=", filename_, startLine, startCol); }
                return Token(TokenType::OP_SHL, "<<", filename_, startLine, startCol);
            }
            if (current() == '=') { advance(); return Token(TokenType::OP_LTE, "<=", filename_, startLine, startCol); }
            return Token(TokenType::OP_LT, "<", filename_, startLine, startCol);

        case '>':
            if (current() == '>') {
                advance();
                if (current() == '=') { advance(); return Token(TokenType::OP_SHR_ASSIGN, ">>=", filename_, startLine, startCol); }
                return Token(TokenType::OP_SHR, ">>", filename_, startLine, startCol);
            }
            if (current() == '=') { advance(); return Token(TokenType::OP_GTE, ">=", filename_, startLine, startCol); }
            return Token(TokenType::OP_GT, ">", filename_, startLine, startCol);

        case '&':
            if (current() == '&') { advance(); return Token(TokenType::OP_AND, "&&", filename_, startLine, startCol); }
            if (current() == '=') { advance(); return Token(TokenType::OP_AND_ASSIGN, "&=", filename_, startLine, startCol); }
            return Token(TokenType::OP_BIT_AND, "&", filename_, startLine, startCol);

        case '|':
            if (current() == '|') { advance(); return Token(TokenType::OP_OR, "||", filename_, startLine, startCol); }
            if (current() == '=') { advance(); return Token(TokenType::OP_OR_ASSIGN, "|=", filename_, startLine, startCol); }
            return Token(TokenType::OP_BIT_OR, "|", filename_, startLine, startCol);

        case '^':
            if (current() == '=') { advance(); return Token(TokenType::OP_XOR_ASSIGN, "^=", filename_, startLine, startCol); }
            return Token(TokenType::OP_BIT_XOR, "^", filename_, startLine, startCol);

        default:
            return Token(TokenType::TOKEN_ERROR, std::string(1, c), filename_, startLine, startCol);
    }
}

} // namespace verslang
