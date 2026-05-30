#pragma once
#include <string>
#include <vector>

// ─── Token Types ─────────────────────────────────────────────────────────────
enum class TokenType {
    // Literals
    INT_LIT, FLOAT_LIT, BOOL_LIT,

    // Identifier
    IDENT,

    // Keywords
    LET, FN, IF, ELSE, WHILE, RETURN, PRINT,

    // Types
    TYPE_INT, TYPE_FLOAT, TYPE_BOOL, TYPE_VOID,

    // Arithmetic operators
    PLUS, MINUS, STAR, SLASH, PERCENT,

    // Comparison operators
    EQ_EQ, BANG_EQ, LT, GT, LT_EQ, GT_EQ,

    // Logical operators
    AMP_AMP, PIPE_PIPE, BANG,

    // Punctuation
    EQ,         // =
    LPAREN,     // (
    RPAREN,     // )
    LBRACE,     // {
    RBRACE,     // }
    COLON,      // :
    SEMICOLON,  // ;
    COMMA,      // ,
    ARROW,      // ->

    // Special
    EOF_TOKEN
};

// ─── Token ───────────────────────────────────────────────────────────────────
struct Token {
    TokenType   type;
    std::string value;   // raw text
    int         line;

    Token(TokenType t, std::string v, int l)
        : type(t), value(std::move(v)), line(l) {}
};

// ─── Lexer ───────────────────────────────────────────────────────────────────
class Lexer {
public:
    explicit Lexer(std::string source);
    std::vector<Token> tokenize();

private:
    std::string src;
    size_t      pos;
    int         line;

    char current() const;
    char peek() const;
    char advance();
    void skipWhitespaceAndComments();

    Token readIdentifierOrKeyword();
    Token readNumber();
    Token readSymbol();

    static TokenType keywordType(const std::string& word);
};