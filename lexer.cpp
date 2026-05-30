#include "lexer.hpp"
#include <stdexcept>

// ─── Constructor ─────────────────────────────────────────────────────────────
Lexer::Lexer(std::string source)
    : src(std::move(source)), pos(0), line(1) {}

// ─── Helpers ─────────────────────────────────────────────────────────────────
char Lexer::current() const {
    return pos < src.size() ? src[pos] : '\0';
}

char Lexer::peek() const {
    return (pos + 1) < src.size() ? src[pos + 1] : '\0';
}

char Lexer::advance() {
    char c = src[pos++];
    if (c == '\n') line++;
    return c;
}

void Lexer::skipWhitespaceAndComments() {
    while (pos < src.size()) {
        char c = current();

        // whitespace
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();

        // single-line comment
        } else if (c == '/' && peek() == '/') {
            while (pos < src.size() && current() != '\n')
                advance();

        } else break;
    }
}

// ─── Identifier / Keyword ────────────────────────────────────────────────────
Token Lexer::readIdentifierOrKeyword() {
    int startLine = line;
    std::string word;

    while (pos < src.size() && (isalnum(current()) || current() == '_'))
        word += advance();

    TokenType t = keywordType(word);
    return Token(t, word, startLine);
}

TokenType Lexer::keywordType(const std::string& w) {
    if (w == "let")    return TokenType::LET;
    if (w == "fn")     return TokenType::FN;
    if (w == "if")     return TokenType::IF;
    if (w == "else")   return TokenType::ELSE;
    if (w == "while")  return TokenType::WHILE;
    if (w == "return") return TokenType::RETURN;
    if (w == "print")  return TokenType::PRINT;
    if (w == "true")   return TokenType::BOOL_LIT;
    if (w == "false")  return TokenType::BOOL_LIT;
    if (w == "int")    return TokenType::TYPE_INT;
    if (w == "float")  return TokenType::TYPE_FLOAT;
    if (w == "bool")   return TokenType::TYPE_BOOL;
    if (w == "void")   return TokenType::TYPE_VOID;
    return TokenType::IDENT;
}

// ─── Number ──────────────────────────────────────────────────────────────────
Token Lexer::readNumber() {
    int startLine = line;
    std::string num;
    bool isFloat = false;

    while (pos < src.size() && isdigit(current()))
        num += advance();

    if (current() == '.' && isdigit(peek())) {
        isFloat = true;
        num += advance(); // consume '.'
        while (pos < src.size() && isdigit(current()))
            num += advance();
    }

    return Token(isFloat ? TokenType::FLOAT_LIT : TokenType::INT_LIT, num, startLine);
}

// ─── Symbols & Operators ─────────────────────────────────────────────────────
Token Lexer::readSymbol() {
    int startLine = line;
    char c = advance();

    // Two-char operators
    char n = current();
    if (c == '=' && n == '=') { advance(); return Token(TokenType::EQ_EQ,    "==", startLine); }
    if (c == '!' && n == '=') { advance(); return Token(TokenType::BANG_EQ,  "!=", startLine); }
    if (c == '<' && n == '=') { advance(); return Token(TokenType::LT_EQ,    "<=", startLine); }
    if (c == '>' && n == '=') { advance(); return Token(TokenType::GT_EQ,    ">=", startLine); }
    if (c == '&' && n == '&') { advance(); return Token(TokenType::AMP_AMP,  "&&", startLine); }
    if (c == '|' && n == '|') { advance(); return Token(TokenType::PIPE_PIPE,"||", startLine); }
    if (c == '-' && n == '>') { advance(); return Token(TokenType::ARROW,    "->", startLine); }

    // Single-char operators & punctuation
    switch (c) {
        case '+': return Token(TokenType::PLUS,      "+", startLine);
        case '-': return Token(TokenType::MINUS,     "-", startLine);
        case '*': return Token(TokenType::STAR,      "*", startLine);
        case '/': return Token(TokenType::SLASH,     "/", startLine);
        case '%': return Token(TokenType::PERCENT,   "%", startLine);
        case '<': return Token(TokenType::LT,        "<", startLine);
        case '>': return Token(TokenType::GT,        ">", startLine);
        case '!': return Token(TokenType::BANG,      "!", startLine);
        case '=': return Token(TokenType::EQ,        "=", startLine);
        case '(': return Token(TokenType::LPAREN,    "(", startLine);
        case ')': return Token(TokenType::RPAREN,    ")", startLine);
        case '{': return Token(TokenType::LBRACE,    "{", startLine);
        case '}': return Token(TokenType::RBRACE,    "}", startLine);
        case ':': return Token(TokenType::COLON,     ":", startLine);
        case ';': return Token(TokenType::SEMICOLON, ";", startLine);
        case ',': return Token(TokenType::COMMA,     ",", startLine);
        default:
            throw std::runtime_error(
                "Unknown character '" + std::string(1, c) +
                "' at line " + std::to_string(startLine));
    }
}

// ─── Main tokenize loop ──────────────────────────────────────────────────────
std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        skipWhitespaceAndComments();

        if (pos >= src.size()) {
            tokens.emplace_back(TokenType::EOF_TOKEN, "EOF", line);
            break;
        }

        char c = current();

        if (isalpha(c) || c == '_')  tokens.push_back(readIdentifierOrKeyword());
        else if (isdigit(c))         tokens.push_back(readNumber());
        else                         tokens.push_back(readSymbol());
    }

    return tokens;
}