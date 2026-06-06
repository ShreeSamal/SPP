#pragma once
#include "ir.hpp"
#include "ast.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>

class CodeGen {
public:
    std::string generate(const IRProgram& ir, const Program& prog);

private:
    std::ostringstream out;
    std::ostringstream dataSection;   // accumulates .data entries

    std::unordered_map<std::string, int>    stackMap;
    int                                      stackSize = 0;
    std::vector<std::string>                 pendingArgs;
    std::unordered_map<std::string, std::string> fnReturnTypes;

    // string literal pool: content → label name
    std::unordered_map<std::string, std::string> stringPool;
    int stringCount = 0;

    // type map: temp/var name → "int"|"float"|"string"|"bool"
    std::unordered_map<std::string, std::string> typeMap;

    void        collectFnInfo(const Program& prog);
    int         allocSlot(const std::string& name);
    std::string slot(const std::string& name);
    void        emit(const std::string& line);
    void        emitLabel(const std::string& label);
    std::string internString(const std::string& content);

    void genFunction(const IRFunction& fn);
    void scanLocals(const IRFunction& fn);
    void genInstr(const IRInstr& ins);

    void genBinary(const IRInstr& ins);
    void genUnary(const IRInstr& ins);
    void genCall(const IRInstr& ins);
    void genPrint(const IRInstr& ins);
    void genReturn(const IRInstr& ins);
};