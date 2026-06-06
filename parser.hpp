#pragma once
#include "lexer.hpp"
#include "ast.hpp"
#include <vector>

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    Program parse();

private:
    std::vector<Token> tokens;
    size_t pos;

    // ── Token navigation ──────────────────────────────────────────────────
    Token&       current();
    Token&       peek(int offset = 1);
    Token        advance();
    Token        expect(TokenType t, const std::string& msg);
    bool         check(TokenType t) const;
    bool         match(TokenType t);

    // ── Type parsing ──────────────────────────────────────────────────────
    TypeKind     parseType();

    // ── Top-level ─────────────────────────────────────────────────────────
    std::unique_ptr<FnDecl> parseFnDecl();
    std::vector<Param>      parseParams();

    // ── Statements ────────────────────────────────────────────────────────
    BlockPtr  parseBlock();
    StmtPtr   parseStmt();
    StmtPtr   parseLetStmt();
    StmtPtr   parseAssignOrExprStmt();
    StmtPtr   parseIfStmt();
    StmtPtr   parseWhileStmt();
    StmtPtr   parseReturnStmt();
    StmtPtr   parsePrintStmt();

    // ── Expressions (precedence climbing) ────────────────────────────────
    ExprPtr   parseExpr();
    ExprPtr   parseOr();
    ExprPtr   parseAnd();
    ExprPtr   parseEquality();
    ExprPtr   parseComparison();
    ExprPtr   parseAddSub();
    ExprPtr   parseMulDiv();
    ExprPtr   parseUnary();
    ExprPtr   parsePrimary();

    // ── Error helper ─────────────────────────────────────────────────────
    [[noreturn]] void error(const std::string& msg);
};