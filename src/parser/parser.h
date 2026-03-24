#pragma once
#include "ast.h"
#include "../lexer/lexer.h"
#include <vector>
#include <string>
#include <stdexcept>

namespace verslang {

class ParseError : public std::runtime_error {
public:
    int line, column;
    std::string filename;
    ParseError(const std::string& msg, const Token& tok)
        : std::runtime_error(msg), line(tok.line), column(tok.column), filename(tok.filename) {}
};

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens, const std::string& filename = "");

    std::shared_ptr<ASTNode> parse();

private:
    std::vector<Token> tokens_;
    std::string filename_;
    size_t pos_ = 0;
    bool pendingCloseAngle_ = false;  // For splitting >> into > >

    // Token access
    const Token& current() const;
    const Token& peek(int offset = 1) const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    const Token& expect(TokenType type, const std::string& msg);
    bool isAtEnd() const;
    bool expectCloseAngle();

    // Top-level parsing
    std::shared_ptr<ASTNode> parseProgram();
    std::shared_ptr<ASTNode> parseStatement();
    std::shared_ptr<ASTNode> parseBlock();

    // Declarations
    std::shared_ptr<ASTNode> parseVarDecl(TokenType declType);
    std::shared_ptr<ASTNode> parseFuncDecl(bool isExport, bool isExtern, bool isInline);
    std::shared_ptr<ASTNode> parseStructDecl();
    std::shared_ptr<ASTNode> parseEnumDecl();
    std::shared_ptr<ASTNode> parseUnionDecl();
    std::shared_ptr<ASTNode> parseModuleDecl();
    std::shared_ptr<ASTNode> parseImport();
    std::shared_ptr<ASTNode> parseInterruptHandler();

    // Statements
    std::shared_ptr<ASTNode> parseIfStmt();
    std::shared_ptr<ASTNode> parseWhileStmt();
    std::shared_ptr<ASTNode> parseForStmt();
    std::shared_ptr<ASTNode> parseLoopStmt();
    std::shared_ptr<ASTNode> parseReturnStmt();
    std::shared_ptr<ASTNode> parseSwitchStmt();
    std::shared_ptr<ASTNode> parseAssemblyBlock();
    std::shared_ptr<ASTNode> parseDirective();
    std::shared_ptr<ASTNode> parseWhenStmt();
    std::shared_ptr<ASTNode> parseLabelStmt();
    std::shared_ptr<ASTNode> parseGotoStmt();

    // Type parsing
    TypePtr parseType();
    TypePtr parsePtrType();
    TypePtr parseArrayType();

    // Parameters and fields
    std::vector<ParamDef> parseParamList();
    std::vector<FieldDef> parseFieldList();

    // Expression parsing (precedence climbing)
    std::shared_ptr<ASTNode> parseExpression();
    std::shared_ptr<ASTNode> parseAssignment();
    std::shared_ptr<ASTNode> parseTernary();
    std::shared_ptr<ASTNode> parseLogicalOr();
    std::shared_ptr<ASTNode> parseLogicalAnd();
    std::shared_ptr<ASTNode> parseBitwiseOr();
    std::shared_ptr<ASTNode> parseBitwiseXor();
    std::shared_ptr<ASTNode> parseBitwiseAnd();
    std::shared_ptr<ASTNode> parseEquality();
    std::shared_ptr<ASTNode> parseComparison();
    std::shared_ptr<ASTNode> parseShift();
    std::shared_ptr<ASTNode> parseAddition();
    std::shared_ptr<ASTNode> parseMultiplication();
    std::shared_ptr<ASTNode> parseUnary();
    std::shared_ptr<ASTNode> parsePostfix();
    std::shared_ptr<ASTNode> parsePrimary();

    // Special expressions
    std::shared_ptr<ASTNode> parseCastExpr();
    std::shared_ptr<ASTNode> parseSizeofExpr();
    std::shared_ptr<ASTNode> parseAddressOfExpr();
    std::shared_ptr<ASTNode> parseDerefExpr();
    std::shared_ptr<ASTNode> parseSyscallExpr();

    // Helpers
    void error(const std::string& msg);
    void optionalSemicolon();
};

} // namespace verslang
