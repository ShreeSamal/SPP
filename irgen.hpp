#pragma once
#include "ast.hpp"
#include "ir.hpp"
#include <vector>
#include <string>

class IRGen {
public:
    IRProgram generate(const Program& prog);

private:
    IRFunction* currentFn   = nullptr;
    int         tempCount   = 0;
    int         labelCount  = 0;

    std::string newTemp();
    std::string newLabel();
    void        emit(IRInstr instr);

    void        genFunction(const FnDecl& fn);
    void        genBlock(const Block& block);
    void        genStmt(const Stmt& stmt);
    std::string genExpr(const Expr& expr);
};