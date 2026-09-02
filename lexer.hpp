/* =====================================================================
 * lexer.hpp
 *
 * Analisador Lexico (C++) para a "Simple Script Language", definida no
 * capitulo 2 da apostila de Compiladores (Prof. Luiz Guedes - IME).
 *
 * Porta em C++ idiomatico do lexer em C (../lexer.h / ../lexer.c),
 * mesma gramatica e mesmas regras lexicas.
 * ===================================================================== */

#ifndef SSL_LEXER_HPP
#define SSL_LEXER_HPP

#include <istream>
#include <string>
#include <unordered_map>
#include <vector>

namespace ssl {

enum class Token {
    /* ---- palavras reservadas (ordem alfabetica) ---- */
    ARRAY, BOOLEAN, BREAK, CHAR, CONTINUE, DO, ELSE, FALSE_, FUNCTION, IF,
    INTEGER, OF, STRING, STRUCT, TRUE_, TYPE, VAR, WHILE,

    /* ---- simbolos ---- */
    COLON, SEMI_COLON, COMMA, EQUALS, LEFT_SQUARE, RIGHT_SQUARE,
    LEFT_BRACES, RIGHT_BRACES, LEFT_PARENTHESIS, RIGHT_PARENTHESIS, AND,
    OR, LESS_THAN, GREATER_THAN, LESS_OR_EQUAL, GREATER_OR_EQUAL,
    NOT_EQUAL, EQUAL_EQUAL, PLUS, PLUS_PLUS, MINUS, MINUS_MINUS, TIMES,
    DIVIDE, DOT, NOT,

    /* ---- tokens regulares ---- */
    CHARACTER, NUMERAL, STRINGVAL, ID,

    /* ---- token desconhecido ---- */
    UNKNOWN,

    /* ---- fim de arquivo (nao esta na apostila, mas e necessaria
     *      para o analisador sintatico saber quando parar)     ---- */
    END_OF_FILE
};

const std::string &tokenName(Token tok);

class Lexer {
public:
    explicit Lexer(std::istream &input);

    /* Le e retorna o proximo token; atualiza token()/tokenSecundario() */
    Token nextToken();

    Token token() const { return token_; }
    int tokenSecundario() const { return tokenSecundario_; }
    int lineNumber() const { return lineNumber_; }

    /* Acesso a tabela de identificadores / pool de constantes */
    const std::string &getName(int index) const;
    char getCharConst(int index) const;
    int getIntConst(int index) const;
    const std::string &getStringConst(int index) const;

private:
    char readChar();
    int searchName(const std::string &name);
    static Token searchKeyword(const std::string &name);

    std::istream &in_;
    char nextChar_ = ' ';
    bool atEOF_ = false;

    Token token_ = Token::UNKNOWN;
    int tokenSecundario_ = -1;
    int lineNumber_ = 1;

    std::unordered_map<std::string, int> nameIndex_;
    std::vector<std::string> names_;

    struct Const {
        unsigned char type; /* 0-char, 1-int, 2-string */
        char cVal = 0;
        int nVal = 0;
        std::string sVal;
    };
    std::vector<Const> consts_;

    int addCharConst(char c);
    int addIntConst(const std::string &numeral);
    int addStringConst(const std::string &s);
};

} // namespace ssl

#endif // SSL_LEXER_HPP
