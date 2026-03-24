#pragma once
#include <string>
#include <vector>
#include "token.h"

namespace verslang {

class Lexer {
public:
    Lexer(const std::string& source, const std::string& filename = "<stdin>");
    std::vector<Token> tokenize();
    Token nextToken();

private:
    std::string source_;
    std::string filename_;
    size_t pos_ = 0;
    int line_ = 1;
    int column_ = 1;

    char current() const;
    char peek(int offset = 1) const;
    char advance();
    bool isAtEnd() const;
    void skipWhitespace();
    void skipComment();

    Token makeToken(TokenType type, const std::string& value);
    Token scanString();
    Token scanChar();
    Token scanNumber();
    Token scanIdentifierOrKeyword();
    Token scanOperator();
    TokenType lookupKeyword(const std::string& word) const;
};

} // namespace verslang
