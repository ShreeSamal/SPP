#include "lexer.hpp"
#include "parser.hpp"
#include "sema.hpp"
#include "irgen.hpp"
#include "codegen.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

int main(int argc, char* argv[]) {
    if (argc < 2) { std::cerr << "Usage: spp <file.spp>\n"; return 1; }

    std::ifstream file(argv[1]);
    if (!file) { std::cerr << "Error: cannot open '" << argv[1] << "'\n"; return 1; }
    std::ostringstream ss; ss << file.rdbuf();

    // derive output filename:  foo.spp → foo.asm
    std::string infile  = argv[1];
    std::string outfile = infile.substr(0, infile.rfind('.')) + ".asm";

    try {
        // Phase 1: Lex
        Lexer lexer(ss.str());
        auto tokens = lexer.tokenize();

        // Phase 2: Parse
        Parser parser(std::move(tokens));
        Program prog = parser.parse();

        // Phase 3: Semantic analysis
        SemanticAnalyzer sema;
        sema.analyze(prog);
        std::cerr << "[sema]   OK\n";

        // Phase 4: IR generation
        IRGen irgen;
        IRProgram ir = irgen.generate(prog);
        std::cerr << "[irgen]  OK\n";

        // Phase 5: Code generation
        CodeGen codegen;
        std::string asmOut = codegen.generate(ir, prog);
        std::cerr << "[codegen] OK\n";

        // Write .asm file
        std::ofstream out(outfile);
        if (!out) { std::cerr << "Error: cannot write '" << outfile << "'\n"; return 1; }
        out << asmOut;
        std::cerr << "[output] " << outfile << "\n";

    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}