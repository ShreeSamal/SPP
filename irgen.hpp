#pragma once
#include "ast.hpp"
#include "ir.hpp"

class IRGen {
public:
    IRProgram generate(const Program& prog);

private:
    IRFunction* currentFn = nullptr;   // function being built
    int         tempCount  = 0;        // global temp counter  t0, t1, ...
    int         labelCount = 0;        // global label counter L0, L1, ...

    // ── Helpers ───────────────────────────────────────────────────────────
    std::string newTemp();             // allocate a new temp: t0, t1, ...
    std::string newLabel();            // allocate a new label: L0, L1, ...
    void        emit(IRInstr instr);   // append to current function

    // ── Generators ────────────────────────────────────────────────────────
    void        genFunction(const FnDecl& fn);
    void        genBlock(const Block& block);
    void        genStmt(const Stmt& stmt);
    std::string genExpr(const Expr& expr);   // returns temp holding result
};