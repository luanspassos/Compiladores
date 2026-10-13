/* =====================================================================
 * lexer.hpp
 *
 * Analisador Lexico da VC (../lexer.hpp), reaproveitado na VF.
 * Alteracoes em relacao a VC (todas documentadas no README):
 *   - palavra reservada 'return' (token RETURN);
 *   - identificadores aceitam digitos apos a primeira letra (ER da gramatica);
 *   - 'true'/'false' ganham token secundario (indice no pool de constantes);
 *   - posicao (linha/coluna) e lexema de cada token, para as mensagens;
 *   - erros lexicos acumulados em errors() em vez de impressos na hora;
 *   - numeral grande demais vira erro lexico (stoi lancava excecao).
 * lineNumber() continua com o comportamento da VC (usado por --tokens).
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
    INTEGER, OF, RETURN, STRING, STRUCT, TRUE_, TYPE, VAR, WHILE,

    /* ---- simbolos ---- */
    COLON, SEMI_COLON, COMMA, EQUALS, LEFT_SQUARE, RIGHT_SQUARE,
    LEFT_BRACES, RIGHT_BRACES, LEFT_PARENTHESIS, RIGHT_PARENTHESIS, AND,
    OR, LESS_THAN, GREATER_THAN, LESS_OR_EQUAL, GREATER_OR_EQUAL,
    NOT_EQUAL, EQUAL_EQUAL, PLUS, PLUS_PLUS, MINUS, MINUS_MINUS, TIMES,
    DIVIDE, DOT, NOT,

    /* ---- tokens regulares ---- */
    CHARACTER, NUMERAL, STRINGVAL, ID,

    /* ---- token desconhecido (erro lexico) ---- */
    UNKNOWN,

    /* ---- fim de arquivo (o '$' do analisador sintatico) ---- */
    END_OF_FILE
};

const int kNumTokens = static_cast<int>(Token::END_OF_FILE) + 1;

/* Token lido, com token secundario e posicao (entrada do analisador sintatico) */
struct Tok {
    Token token;
    int sec;                  /* token secundario (-1 se nao ha) */
    int line, col;
    std::string lexeme;
};

const std::string &tokenName(Token tok);

struct LexError {
    int line, col;
    std::string msg;
};

class Lexer {
public:
    explicit Lexer(std::istream &input);

    /* Le e retorna o proximo token; atualiza token()/tokenSecundario() */
    Token nextToken();

    Token token() const { return token_; }
    int tokenSecundario() const { return tokenSecundario_; }
    int lineNumber() const { return lineNumber_; }

    /* Posicao e texto do ultimo token lido */
    int tokenLine() const { return tokLine_; }
    int tokenColumn() const { return tokCol_; }
    const std::string &lexeme() const { return lexeme_; }
    Tok current() const { return Tok{token_, tokenSecundario_, tokLine_, tokCol_, lexeme_}; }

    const std::vector<LexError> &errors() const { return errors_; }

    /* Acesso a tabela de identificadores / pool de constantes */
    const std::string &getName(int index) const;
    char getCharConst(int index) const;
    int getIntConst(int index) const;
    bool getBoolConst(int index) const;
    const std::string &getStringConst(int index) const;
    int numConsts() const { return static_cast<int>(consts_.size()); }
    /* tipo da constante: 0-char, 1-int, 2-string, 3-boolean */
    int constType(int index) const;

private:
    char readChar();
    Token scan();
    void error(const std::string &msg);
    int searchName(const std::string &name);
    static Token searchKeyword(const std::string &name);

    std::string src_;
    size_t pos_ = 0;
    char nextChar_ = ' ';
    bool atEOF_ = false;

    Token token_ = Token::UNKNOWN;
    int tokenSecundario_ = -1;
    int lineNumber_ = 1;

    /* posicao do caractere nextChar_ e do proximo a ser lido */
    int chLine_ = 1, chCol_ = 0, nextLine_ = 1, nextCol_ = 1;
    int tokLine_ = 1, tokCol_ = 1;
    size_t tokStart_ = 0;
    std::string lexeme_;
    std::vector<LexError> errors_;

    std::unordered_map<std::string, int> nameIndex_;
    std::vector<std::string> names_;

    struct Const {
        unsigned char type; /* 0-char, 1-int, 2-string, 3-boolean */
        char cVal = 0;
        int nVal = 0;
        std::string sVal;
    };
    std::vector<Const> consts_;

    int addCharConst(char c);
    int addIntConst(int value);
    int addBoolConst(bool value);
    int addStringConst(const std::string &s);
};

} // namespace ssl

#endif // SSL_LEXER_HPP
