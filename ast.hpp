#pragma once
#include <string>
#include <vector>
#include <memory>

// ─── Types ───────────────────────────────────────────────────────────────────
enum class TypeKind { INT, FLOAT, BOOL, VOID, STRING };

static const char* typeKindName(TypeKind t) {
    switch (t) {
        case TypeKind::INT:    return "int";
        case TypeKind::FLOAT:  return "float";
        case TypeKind::BOOL:   return "bool";
        case TypeKind::VOID:   return "void";
        case TypeKind::STRING: return "string";
        default:               return "unknown";
    }
}

// ─── Forward declarations ────────────────────────────────────────────────────
struct Expr;
struct Stmt;
struct Block;

using ExprPtr  = std::unique_ptr<Expr>;
using StmtPtr  = std::unique_ptr<Stmt>;
using BlockPtr = std::unique_ptr<Block>;

// ═════════════════════════════════════════════════════════════════════════════
// EXPRESSIONS
// ═════════════════════════════════════════════════════════════════════════════
enum class ExprKind {
    LITERAL_INT, LITERAL_FLOAT, LITERAL_BOOL, LITERAL_STRING,
    VAR, BINARY, UNARY, CALL
};

struct Expr {
    ExprKind    kind;
    int         line;

    long long   ival = 0;
    double      fval = 0.0;
    bool        bval = false;
    std::string sval;        // string literal value  OR  var/call name  OR  op
    std::string name;        // var name / call name
    std::string op;          // operator
    ExprPtr     left;
    ExprPtr     right;
    std::vector<ExprPtr> args;
};

// ─── Expr factory helpers ────────────────────────────────────────────────────
inline ExprPtr makeIntLit(long long v, int line) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::LITERAL_INT; e->ival = v; e->line = line;
    return e;
}
inline ExprPtr makeFloatLit(double v, int line) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::LITERAL_FLOAT; e->fval = v; e->line = line;
    return e;
}
inline ExprPtr makeBoolLit(bool v, int line) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::LITERAL_BOOL; e->bval = v; e->line = line;
    return e;
}
inline ExprPtr makeStringLit(std::string v, int line) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::LITERAL_STRING; e->sval = std::move(v); e->line = line;
    return e;
}
inline ExprPtr makeVar(std::string n, int line) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::VAR; e->name = std::move(n); e->line = line;
    return e;
}
inline ExprPtr makeBinary(std::string op, ExprPtr l, ExprPtr r, int line) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::BINARY; e->op = std::move(op);
    e->left = std::move(l); e->right = std::move(r); e->line = line;
    return e;
}
inline ExprPtr makeUnary(std::string op, ExprPtr operand, int line) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::UNARY; e->op = std::move(op);
    e->left = std::move(operand); e->line = line;
    return e;
}
inline ExprPtr makeCall(std::string n, std::vector<ExprPtr> a, int line) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::CALL; e->name = std::move(n);
    e->args = std::move(a); e->line = line;
    return e;
}

// ═════════════════════════════════════════════════════════════════════════════
// STATEMENTS
// ═════════════════════════════════════════════════════════════════════════════
enum class StmtKind { LET, ASSIGN, IF, WHILE, RETURN, PRINT, EXPR };

struct Block {
    std::vector<StmtPtr> stmts;
};

struct Stmt {
    StmtKind kind;
    int      line;

    std::string name;
    TypeKind    varType;
    ExprPtr     expr;
    ExprPtr     cond;
    BlockPtr    thenBlock;
    BlockPtr    elseBlock;
};

// ─── Stmt factory helpers ────────────────────────────────────────────────────
inline StmtPtr makeLetStmt(std::string n, TypeKind t, ExprPtr init, int line) {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::LET; s->name = std::move(n);
    s->varType = t; s->expr = std::move(init); s->line = line;
    return s;
}
inline StmtPtr makeAssignStmt(std::string n, ExprPtr val, int line) {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::ASSIGN; s->name = std::move(n);
    s->expr = std::move(val); s->line = line;
    return s;
}
inline StmtPtr makeIfStmt(ExprPtr cond, BlockPtr then, BlockPtr els, int line) {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::IF; s->cond = std::move(cond);
    s->thenBlock = std::move(then); s->elseBlock = std::move(els);
    s->line = line;
    return s;
}
inline StmtPtr makeWhileStmt(ExprPtr cond, BlockPtr body, int line) {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::WHILE; s->cond = std::move(cond);
    s->thenBlock = std::move(body); s->line = line;
    return s;
}
inline StmtPtr makeReturnStmt(ExprPtr val, int line) {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::RETURN; s->expr = std::move(val); s->line = line;
    return s;
}
inline StmtPtr makePrintStmt(ExprPtr val, int line) {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::PRINT; s->expr = std::move(val); s->line = line;
    return s;
}
inline StmtPtr makeExprStmt(ExprPtr val, int line) {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::EXPR; s->expr = std::move(val); s->line = line;
    return s;
}

// ═════════════════════════════════════════════════════════════════════════════
// FUNCTION & PROGRAM
// ═════════════════════════════════════════════════════════════════════════════
struct Param {
    std::string name;
    TypeKind    type;
};

struct FnDecl {
    std::string        name;
    std::vector<Param> params;
    TypeKind           returnType;
    BlockPtr           body;
    int                line;
};

struct Program {
    std::vector<std::unique_ptr<FnDecl>> fns;
};