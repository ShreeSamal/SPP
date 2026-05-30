#pragma once
#include "ast.hpp"
#include <string>
#include <vector>
#include <unordered_map>

// ─── Function signature (built in pass 1) ────────────────────────────────────
struct FnSig {
    TypeKind              returnType;
    std::vector<TypeKind> paramTypes;
    std::vector<std::string> paramNames;
    int line;
};

// ─── Scope stack entry ───────────────────────────────────────────────────────
using Scope = std::unordered_map<std::string, TypeKind>;

// ─── Semantic Analyzer ───────────────────────────────────────────────────────
class SemanticAnalyzer {
public:
    void analyze(const Program& prog);   // throws on error

private:
    // function table (populated in pass 1)
    std::unordered_map<std::string, FnSig> fnTable;

    // scope stack for variables
    std::vector<Scope> scopes;

    // current function's return type (for return-stmt checking)
    TypeKind currentReturnType = TypeKind::VOID;

    // ── Scope helpers ─────────────────────────────────────────────────────
    void        pushScope();
    void        popScope();
    void        declare(const std::string& name, TypeKind type, int line);
    TypeKind    lookup(const std::string& name, int line) const;
    bool        declaredInCurrentScope(const std::string& name) const;

    // ── Passes ────────────────────────────────────────────────────────────
    void collectFunctions(const Program& prog);   // pass 1
    void checkFunction(const FnDecl& fn);         // pass 2

    // ── Checkers ──────────────────────────────────────────────────────────
    void     checkBlock(const Block& block);
    void     checkStmt(const Stmt& stmt);
    TypeKind checkExpr(const Expr& expr);

    // ── Error helper ──────────────────────────────────────────────────────
    [[noreturn]] static void error(int line, const std::string& msg);
};