#include "lexer.hpp"

#include <array>
#include <cctype>
#include <iostream>

namespace ssl {

namespace {
/* Em ordem alfabetica (obrigatorio para a busca binaria), alinhada 1:1
 * com o inicio do enum Token. */
constexpr std::array<const char *, 18> kKeywords = {
    "array", "boolean", "break", "char", "continue", "do", "else",
    "false", "function", "if", "integer", "of", "string", "struct",
    "true", "type", "var", "while"};
} // namespace

const std::string &tokenName(Token tok) {
    static const std::array<std::string, 50> names = {
        "ARRAY", "BOOLEAN", "BREAK", "CHAR", "CONTINUE", "DO", "ELSE", "FALSE",
        "FUNCTION", "IF", "INTEGER", "OF", "STRING", "STRUCT", "TRUE", "TYPE",
        "VAR", "WHILE",
        "COLON", "SEMI_COLON", "COMMA", "EQUALS", "LEFT_SQUARE", "RIGHT_SQUARE",
        "LEFT_BRACES", "RIGHT_BRACES", "LEFT_PARENTHESIS", "RIGHT_PARENTHESIS",
        "AND", "OR", "LESS_THAN", "GREATER_THAN", "LESS_OR_EQUAL",
        "GREATER_OR_EQUAL", "NOT_EQUAL", "EQUAL_EQUAL", "PLUS", "PLUS_PLUS",
        "MINUS", "MINUS_MINUS", "TIMES", "DIVIDE", "DOT", "NOT",
        "CHARACTER", "NUMERAL", "STRINGVAL", "ID",
        "UNKNOWN", "END_OF_FILE"};
    static const std::string unknown = "???";
    size_t idx = static_cast<size_t>(tok);
    if (idx >= names.size()) return unknown;
    return names[idx];
}

Lexer::Lexer(std::istream &input) : in_(input) {}

char Lexer::readChar() {
    if (atEOF_) return '\0';
    int c = in_.get();
    if (c == EOF) {
        atEOF_ = true;
        return '\0';
    }
    if (c == '\n') ++lineNumber_;
    return static_cast<char>(c);
}

Token Lexer::searchKeyword(const std::string &name) {
    int lo = 0, hi = static_cast<int>(kKeywords.size()) - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int cmp = name.compare(kKeywords[mid]);
        if (cmp == 0) return static_cast<Token>(mid); /* enum comeca em ARRAY = 0 */
        if (cmp < 0) hi = mid - 1;
        else lo = mid + 1;
    }
    return Token::ID;
}

int Lexer::searchName(const std::string &name) {
    auto it = nameIndex_.find(name);
    if (it != nameIndex_.end()) return it->second;
    int id = static_cast<int>(names_.size());
    names_.push_back(name);
    nameIndex_.emplace(name, id);
    return id;
}

Token Lexer::nextToken() {
    while (!atEOF_ && std::isspace(static_cast<unsigned char>(nextChar_))) {
        nextChar_ = readChar();
    }

    if (atEOF_) {
        token_ = Token::END_OF_FILE;
        tokenSecundario_ = -1;
        return token_;
    }

    /* --- ID / palavra reservada: letra . (letra + digito)* --- */
    if (std::isalpha(static_cast<unsigned char>(nextChar_))) {
        std::string text;
        do {
            text.push_back(nextChar_);
            nextChar_ = readChar();
        } while (!atEOF_ && std::isalpha(static_cast<unsigned char>(nextChar_)));

        token_ = searchKeyword(text);
        tokenSecundario_ = (token_ == Token::ID) ? searchName(text) : -1;
        return token_;
    }

    /* --- NUM: digito . digito* --- */
    if (std::isdigit(static_cast<unsigned char>(nextChar_))) {
        std::string numeral;
        do {
            numeral.push_back(nextChar_);
            nextChar_ = readChar();
        } while (!atEOF_ && std::isdigit(static_cast<unsigned char>(nextChar_)));

        token_ = Token::NUMERAL;
        tokenSecundario_ = addIntConst(numeral);
        return token_;
    }

    /* --- STR: '"' . any* . '"' --- */
    if (nextChar_ == '"') {
        std::string text;
        nextChar_ = readChar(); /* consome a aspa de abertura */
        while (!atEOF_ && nextChar_ != '"') {
            text.push_back(nextChar_);
            nextChar_ = readChar();
        }
        if (atEOF_) {
            std::cerr << "Erro lexico (linha " << lineNumber_ << "): string nao terminada\n";
            token_ = Token::UNKNOWN;
            tokenSecundario_ = -1;
            return token_;
        }
        nextChar_ = readChar(); /* consome a aspa de fechamento */

        token_ = Token::STRINGVAL;
        tokenSecundario_ = addStringConst(text);
        return token_;
    }

    /* --- CHR: ''' . any . ''' --- */
    if (nextChar_ == '\'') {
        nextChar_ = readChar(); /* le o caractere do meio */
        if (atEOF_) {
            std::cerr << "Erro lexico (linha " << lineNumber_ << "): caractere nao terminado\n";
            token_ = Token::UNKNOWN;
            tokenSecundario_ = -1;
            return token_;
        }
        char c = nextChar_;
        nextChar_ = readChar(); /* deveria ser a aspa de fechamento */
        if (nextChar_ != '\'') {
            std::cerr << "Erro lexico (linha " << lineNumber_ << "): esperado ''' apos caractere\n";
            token_ = Token::UNKNOWN;
            tokenSecundario_ = -1;
            return token_;
        }
        nextChar_ = readChar(); /* consome a aspa de fechamento */

        token_ = Token::CHARACTER;
        tokenSecundario_ = addCharConst(c);
        return token_;
    }

    /* --- simbolos --- */
    {
        char c = nextChar_;
        tokenSecundario_ = -1;

        switch (c) {
            case ':': nextChar_ = readChar(); token_ = Token::COLON; break;
            case ';': nextChar_ = readChar(); token_ = Token::SEMI_COLON; break;
            case ',': nextChar_ = readChar(); token_ = Token::COMMA; break;
            case '[': nextChar_ = readChar(); token_ = Token::LEFT_SQUARE; break;
            case ']': nextChar_ = readChar(); token_ = Token::RIGHT_SQUARE; break;
            case '{': nextChar_ = readChar(); token_ = Token::LEFT_BRACES; break;
            case '}': nextChar_ = readChar(); token_ = Token::RIGHT_BRACES; break;
            case '(': nextChar_ = readChar(); token_ = Token::LEFT_PARENTHESIS; break;
            case ')': nextChar_ = readChar(); token_ = Token::RIGHT_PARENTHESIS; break;
            case '.': nextChar_ = readChar(); token_ = Token::DOT; break;
            case '*': nextChar_ = readChar(); token_ = Token::TIMES; break;
            case '/': nextChar_ = readChar(); token_ = Token::DIVIDE; break;

            case '=':
                nextChar_ = readChar();
                if (nextChar_ == '=') { token_ = Token::EQUAL_EQUAL; nextChar_ = readChar(); }
                else { token_ = Token::EQUALS; }
                break;

            case '&':
                nextChar_ = readChar();
                if (nextChar_ == '&') { token_ = Token::AND; nextChar_ = readChar(); }
                else {
                    std::cerr << "Erro lexico (linha " << lineNumber_ << "): '&' isolado (esperado '&&')\n";
                    token_ = Token::UNKNOWN;
                }
                break;

            case '|':
                nextChar_ = readChar();
                if (nextChar_ == '|') { token_ = Token::OR; nextChar_ = readChar(); }
                else {
                    std::cerr << "Erro lexico (linha " << lineNumber_ << "): '|' isolado (esperado '||')\n";
                    token_ = Token::UNKNOWN;
                }
                break;

            case '<':
                nextChar_ = readChar();
                if (nextChar_ == '=') { token_ = Token::LESS_OR_EQUAL; nextChar_ = readChar(); }
                else { token_ = Token::LESS_THAN; }
                break;

            case '>':
                nextChar_ = readChar();
                if (nextChar_ == '=') { token_ = Token::GREATER_OR_EQUAL; nextChar_ = readChar(); }
                else { token_ = Token::GREATER_THAN; }
                break;

            case '!':
                nextChar_ = readChar();
                if (nextChar_ == '=') { token_ = Token::NOT_EQUAL; nextChar_ = readChar(); }
                else { token_ = Token::NOT; }
                break;

            case '+':
                nextChar_ = readChar();
                if (nextChar_ == '+') { token_ = Token::PLUS_PLUS; nextChar_ = readChar(); }
                else { token_ = Token::PLUS; }
                break;

            case '-':
                nextChar_ = readChar();
                if (nextChar_ == '-') { token_ = Token::MINUS_MINUS; nextChar_ = readChar(); }
                else { token_ = Token::MINUS; }
                break;

            default:
                if (atEOF_) {
                    token_ = Token::END_OF_FILE;
                } else {
                    std::cerr << "Erro lexico (linha " << lineNumber_ << "): caractere invalido '"
                               << (std::isprint(static_cast<unsigned char>(c)) ? c : '?')
                               << "' (0x" << std::hex << static_cast<int>(static_cast<unsigned char>(c))
                               << std::dec << ")\n";
                    token_ = Token::UNKNOWN;
                    nextChar_ = readChar();
                }
        }
        return token_;
    }
}

int Lexer::addCharConst(char c) {
    Const cst;
    cst.type = 0;
    cst.cVal = c;
    consts_.push_back(cst);
    return static_cast<int>(consts_.size()) - 1;
}

int Lexer::addStringConst(const std::string &s) {
    Const c;
    c.type = 2;
    c.sVal = s;
    consts_.push_back(std::move(c));
    return static_cast<int>(consts_.size()) - 1;
}

int Lexer::addIntConst(const std::string &numeral) {
    Const c;
    c.type = 1;
    c.nVal = std::stoi(numeral);
    consts_.push_back(std::move(c));
    return static_cast<int>(consts_.size()) - 1;
}

const std::string &Lexer::getName(int index) const {
    return names_.at(static_cast<size_t>(index));
}

char Lexer::getCharConst(int index) const { return consts_.at(static_cast<size_t>(index)).cVal; }
int Lexer::getIntConst(int index) const { return consts_.at(static_cast<size_t>(index)).nVal; }

const std::string &Lexer::getStringConst(int index) const {
    return consts_.at(static_cast<size_t>(index)).sVal;
}

} // namespace ssl
