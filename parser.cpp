#include "parser.hpp"
#include <stdexcept>
#include <cassert>

// ─── Constructor ─────────────────────────────────────────────────────────────
Parser::Parser(std::vector<Token> toks)
    : tokens(std::move(toks)), pos(0) {}

// ═════════════════════════════════════════════════════════════════════════════
// Token navigation
// ═════════════════════════════════════════════════════════════════════════════
Token& Parser::current() { return tokens[pos]; }

Token& Parser::peek(int offset) {
    size_t idx = pos + offset;
    if (idx >= tokens.size()) return tokens.back(); // EOF
    return tokens[idx];
}

Token Parser::advance() {
    Token t = tokens[pos];
    if (t.type != TokenType::EOF_TOKEN) pos++;
    return t;
}

bool Parser::check(TokenType t) const { return tokens[pos].type == t; }

bool Parser::match(TokenType t) {
    if (check(t)) { advance(); return true; }
    return false;
}

Token Parser::expect(TokenType t, const std::string& msg) {
    if (!check(t)) error(msg);
    return advance();
}

void Parser::error(const std::string& msg) {
    throw std::runtime_error(
        "Parse error at line " + std::to_string(current().line) +
        " ('" + current().value + "'): " + msg);
}

// ═════════════════════════════════════════════════════════════════════════════
// Type
// ═════════════════════════════════════════════════════════════════════════════
TypeKind Parser::parseType() {
    switch (current().type) {
        case TokenType::TYPE_INT:   advance(); return TypeKind::INT;
        case TokenType::TYPE_FLOAT: advance(); return TypeKind::FLOAT;
        case TokenType::TYPE_BOOL:  advance(); return TypeKind::BOOL;
        case TokenType::TYPE_VOID:  advance(); return TypeKind::VOID;
        default: error("expected a type (int, float, bool, void)");
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Top level
// ═════════════════════════════════════════════════════════════════════════════
Program Parser::parse() {
    Program prog;
    while (!check(TokenType::EOF_TOKEN))
        prog.fns.push_back(parseFnDecl());
    return prog;
}

std::unique_ptr<FnDecl> Parser::parseFnDecl() {
    int line = current().line;
    expect(TokenType::FN, "expected 'fn'");

    auto fn       = std::make_unique<FnDecl>();
    fn->line      = line;
    fn->name      = expect(TokenType::IDENT, "expected function name").value;

    expect(TokenType::LPAREN, "expected '(' after function name");
    fn->params = parseParams();
    expect(TokenType::RPAREN, "expected ')' after parameters");
    expect(TokenType::ARROW,  "expected '->' after parameters");
    fn->returnType = parseType();
    fn->body       = parseBlock();
    return fn;
}

std::vector<Param> Parser::parseParams() {
    std::vector<Param> params;
    if (check(TokenType::RPAREN)) return params; // empty

    do {
        Param p;
        p.name = expect(TokenType::IDENT, "expected parameter name").value;
        expect(TokenType::COLON, "expected ':' after parameter name");
        p.type = parseType();
        params.push_back(std::move(p));
    } while (match(TokenType::COMMA));

    return params;
}

// ═════════════════════════════════════════════════════════════════════════════
// Block & Statements
// ═════════════════════════════════════════════════════════════════════════════
BlockPtr Parser::parseBlock() {
    expect(TokenType::LBRACE, "expected '{'");
    auto block = std::make_unique<Block>();
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN))
        block->stmts.push_back(parseStmt());
    expect(TokenType::RBRACE, "expected '}'");
    return block;
}

StmtPtr Parser::parseStmt() {
    switch (current().type) {
        case TokenType::LET:    return parseLetStmt();
        case TokenType::IF:     return parseIfStmt();
        case TokenType::WHILE:  return parseWhileStmt();
        case TokenType::RETURN: return parseReturnStmt();
        case TokenType::PRINT:  return parsePrintStmt();
        default:                return parseAssignOrExprStmt();
    }
}

// let IDENT : type = expr ;
StmtPtr Parser::parseLetStmt() {
    int line = current().line;
    expect(TokenType::LET,   "expected 'let'");
    std::string name = expect(TokenType::IDENT, "expected variable name").value;
    expect(TokenType::COLON, "expected ':'");
    TypeKind type    = parseType();
    expect(TokenType::EQ,    "expected '='");
    ExprPtr init     = parseExpr();
    expect(TokenType::SEMICOLON, "expected ';'");
    return makeLetStmt(name, type, std::move(init), line);
}

// IDENT = expr ;   OR   expr ;
StmtPtr Parser::parseAssignOrExprStmt() {
    int line = current().line;

    // Look-ahead: IDENT followed by '=' (not '==') → assignment
    if (check(TokenType::IDENT) && peek().type == TokenType::EQ) {
        std::string name = advance().value; // consume IDENT
        advance();                          // consume '='
        ExprPtr val = parseExpr();
        expect(TokenType::SEMICOLON, "expected ';'");
        return makeAssignStmt(name, std::move(val), line);
    }

    // Otherwise it's a plain expression statement (e.g. a function call)
    ExprPtr e = parseExpr();
    expect(TokenType::SEMICOLON, "expected ';'");
    return makeExprStmt(std::move(e), line);
}

// if ( expr ) block ( else block )?
StmtPtr Parser::parseIfStmt() {
    int line = current().line;
    expect(TokenType::IF,     "expected 'if'");
    expect(TokenType::LPAREN, "expected '('");
    ExprPtr cond = parseExpr();
    expect(TokenType::RPAREN, "expected ')'");
    BlockPtr thenBlock = parseBlock();
    BlockPtr elseBlock;
    if (match(TokenType::ELSE))
        elseBlock = parseBlock();
    return makeIfStmt(std::move(cond), std::move(thenBlock),
                      std::move(elseBlock), line);
}

// while ( expr ) block
StmtPtr Parser::parseWhileStmt() {
    int line = current().line;
    expect(TokenType::WHILE,  "expected 'while'");
    expect(TokenType::LPAREN, "expected '('");
    ExprPtr cond = parseExpr();
    expect(TokenType::RPAREN, "expected ')'");
    BlockPtr body = parseBlock();
    return makeWhileStmt(std::move(cond), std::move(body), line);
}

// return expr ;
StmtPtr Parser::parseReturnStmt() {
    int line = current().line;
    expect(TokenType::RETURN, "expected 'return'");
    ExprPtr val = parseExpr();
    expect(TokenType::SEMICOLON, "expected ';'");
    return makeReturnStmt(std::move(val), line);
}

// print ( expr ) ;
StmtPtr Parser::parsePrintStmt() {
    int line = current().line;
    expect(TokenType::PRINT,  "expected 'print'");
    expect(TokenType::LPAREN, "expected '('");
    ExprPtr val = parseExpr();
    expect(TokenType::RPAREN, "expected ')'");
    expect(TokenType::SEMICOLON, "expected ';'");
    return makePrintStmt(std::move(val), line);
}

// ═════════════════════════════════════════════════════════════════════════════
// Expressions  (precedence: low → high)
// ═════════════════════════════════════════════════════════════════════════════
ExprPtr Parser::parseExpr()       { return parseOr(); }

// ||
ExprPtr Parser::parseOr() {
    ExprPtr left = parseAnd();
    while (check(TokenType::PIPE_PIPE)) {
        int line = current().line;
        std::string op = advance().value;
        left = makeBinary(op, std::move(left), parseAnd(), line);
    }
    return left;
}

// &&
ExprPtr Parser::parseAnd() {
    ExprPtr left = parseEquality();
    while (check(TokenType::AMP_AMP)) {
        int line = current().line;
        std::string op = advance().value;
        left = makeBinary(op, std::move(left), parseEquality(), line);
    }
    return left;
}

// == !=
ExprPtr Parser::parseEquality() {
    ExprPtr left = parseComparison();
    while (check(TokenType::EQ_EQ) || check(TokenType::BANG_EQ)) {
        int line = current().line;
        std::string op = advance().value;
        left = makeBinary(op, std::move(left), parseComparison(), line);
    }
    return left;
}

// < > <= >=
ExprPtr Parser::parseComparison() {
    ExprPtr left = parseAddSub();
    while (check(TokenType::LT)    || check(TokenType::GT) ||
           check(TokenType::LT_EQ) || check(TokenType::GT_EQ)) {
        int line = current().line;
        std::string op = advance().value;
        left = makeBinary(op, std::move(left), parseAddSub(), line);
    }
    return left;
}

// + -
ExprPtr Parser::parseAddSub() {
    ExprPtr left = parseMulDiv();
    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        int line = current().line;
        std::string op = advance().value;
        left = makeBinary(op, std::move(left), parseMulDiv(), line);
    }
    return left;
}

// * / %
ExprPtr Parser::parseMulDiv() {
    ExprPtr left = parseUnary();
    while (check(TokenType::STAR)  || check(TokenType::SLASH) ||
           check(TokenType::PERCENT)) {
        int line = current().line;
        std::string op = advance().value;
        left = makeBinary(op, std::move(left), parseUnary(), line);
    }
    return left;
}

// ! -
ExprPtr Parser::parseUnary() {
    if (check(TokenType::BANG) || check(TokenType::MINUS)) {
        int line = current().line;
        std::string op = advance().value;
        return makeUnary(op, parseUnary(), line);
    }
    return parsePrimary();
}

// literals, variables, function calls, grouped expr
ExprPtr Parser::parsePrimary() {
    int line = current().line;

    // Integer literal
    if (check(TokenType::INT_LIT)) {
        long long v = std::stoll(current().value);
        advance();
        return makeIntLit(v, line);
    }

    // Float literal
    if (check(TokenType::FLOAT_LIT)) {
        double v = std::stod(current().value);
        advance();
        return makeFloatLit(v, line);
    }

    // Bool literal
    if (check(TokenType::BOOL_LIT)) {
        bool v = (current().value == "true");
        advance();
        return makeBoolLit(v, line);
    }

    // Identifier: variable or function call
    if (check(TokenType::IDENT)) {
        std::string name = advance().value;

        // Function call: name ( args )
        if (match(TokenType::LPAREN)) {
            std::vector<ExprPtr> args;
            if (!check(TokenType::RPAREN)) {
                do { args.push_back(parseExpr()); }
                while (match(TokenType::COMMA));
            }
            expect(TokenType::RPAREN, "expected ')' after arguments");
            return makeCall(name, std::move(args), line);
        }

        // Plain variable
        return makeVar(name, line);
    }

    // Grouped expression: ( expr )
    if (match(TokenType::LPAREN)) {
        ExprPtr e = parseExpr();
        expect(TokenType::RPAREN, "expected ')'");
        return e;
    }

    error("unexpected token in expression");
}