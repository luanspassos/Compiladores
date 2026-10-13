#include "lexer.hpp"

#include <array>
#include <cctype>
#include <iterator>
#include <limits>

namespace ssl {

namespace {
/* Em ordem alfabetica (obrigatorio para a busca binaria), alinhada 1:1
 * com o inicio do enum Token. */
const std::array<const char *, 19> kKeywords = {{
    "array", "boolean", "break", "char", "continue", "do", "else",
    "false", "function", "if", "integer", "of", "return", "string",
    "struct", "true", "type", "var", "while"}};
} // namespace

const std::string &tokenName(Token tok) {
    static const std::array<std::string, 51> names = {{
        "ARRAY", "BOOLEAN", "BREAK", "CHAR", "CONTINUE", "DO", "ELSE", "FALSE",
        "FUNCTION", "IF", "INTEGER", "OF", "RETURN", "STRING", "STRUCT", "TRUE",
        "TYPE", "VAR", "WHILE",
        "COLON", "SEMI_COLON", "COMMA", "EQUALS", "LEFT_SQUARE", "RIGHT_SQUARE",
        "LEFT_BRACES", "RIGHT_BRACES", "LEFT_PARENTHESIS", "RIGHT_PARENTHESIS",
        "AND", "OR", "LESS_THAN", "GREATER_THAN", "LESS_OR_EQUAL",
        "GREATER_OR_EQUAL", "NOT_EQUAL", "EQUAL_EQUAL", "PLUS", "PLUS_PLUS",
        "MINUS", "MINUS_MINUS", "TIMES", "DIVIDE", "DOT", "NOT",
        "CHARACTER", "NUMERAL", "STRINGVAL", "ID",
        "UNKNOWN", "END_OF_FILE"}};
    static const std::string unknown = "???";
    size_t idx = static_cast<size_t>(tok);
    if (idx >= names.size()) return unknown;
    return names[idx];
}

Lexer::Lexer(std::istream &input)
    : src_((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>()) {}

char Lexer::readChar() {
    chLine_ = nextLine_;
    chCol_ = nextCol_;
    if (pos_ >= src_.size()) {
        atEOF_ = true;
        return '\0';
    }
    char c = src_[pos_++];
    if (c == '\n') { ++lineNumber_; ++nextLine_; nextCol_ = 1; }
    else ++nextCol_;
    return c;
}

void Lexer::error(const std::string &msg) {
    errors_.push_back(LexError{tokLine_, tokCol_, msg});
}

Token Lexer::searchKeyword(const std::string &name) {
    int lo = 0, hi = static_cast<int>(kKeywords.size()) - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int cmp = name.compare(kKeywords[static_cast<size_t>(mid)]);
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

    /* nextChar_ ocupa a posicao pos_-1 do fonte (ja foi lido: e o lookahead) */
    tokLine_ = chLine_;
    tokCol_ = chCol_;
    tokStart_ = pos_ == 0 ? 0 : pos_ - 1;

    Token t = scan();

    size_t end = atEOF_ ? src_.size() : pos_ - 1;
    lexeme_ = end > tokStart_ ? src_.substr(tokStart_, end - tokStart_) : std::string();
    return t;
}

Token Lexer::scan() {
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
        } while (!atEOF_ && std::isalnum(static_cast<unsigned char>(nextChar_)));

        token_ = searchKeyword(text);
        if (token_ == Token::ID) tokenSecundario_ = searchName(text);
        else if (token_ == Token::TRUE_) tokenSecundario_ = addBoolConst(true);
        else if (token_ == Token::FALSE_) tokenSecundario_ = addBoolConst(false);
        else tokenSecundario_ = -1;
        return token_;
    }

    /* --- NUM: digito . digito* --- */
    if (std::isdigit(static_cast<unsigned char>(nextChar_))) {
        long long value = 0;
        bool overflow = false;
        do {
            value = value * 10 + (nextChar_ - '0');
            if (value > std::numeric_limits<int>::max()) { overflow = true; value = 0; }
            nextChar_ = readChar();
        } while (!atEOF_ && std::isdigit(static_cast<unsigned char>(nextChar_)));

        if (overflow) error("numeral grande demais");
        token_ = Token::NUMERAL;
        tokenSecundario_ = addIntConst(static_cast<int>(value));
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
            error("string nao terminada");
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
            error("caractere nao terminado");
            token_ = Token::UNKNOWN;
            tokenSecundario_ = -1;
            return token_;
        }
        char c = nextChar_;
        nextChar_ = readChar(); /* deveria ser a aspa de fechamento */
        if (nextChar_ != '\'') {
            error("esperado ''' apos caractere");
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
                error("'&' isolado (esperado '&&')");
                token_ = Token::UNKNOWN;
            }
            break;

        case '|':
            nextChar_ = readChar();
            if (nextChar_ == '|') { token_ = Token::OR; nextChar_ = readChar(); }
            else {
                error("'|' isolado (esperado '||')");
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
            error(std::string("caractere invalido '") +
                  (std::isprint(static_cast<unsigned char>(c)) ? c : '?') + "'");
            token_ = Token::UNKNOWN;
            nextChar_ = readChar();
    }
    return token_;
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
    consts_.push_back(c);
    return static_cast<int>(consts_.size()) - 1;
}

int Lexer::addIntConst(int value) {
    Const c;
    c.type = 1;
    c.nVal = value;
    consts_.push_back(c);
    return static_cast<int>(consts_.size()) - 1;
}

int Lexer::addBoolConst(bool value) {
    Const c;
    c.type = 3;
    c.nVal = value ? 1 : 0;
    consts_.push_back(c);
    return static_cast<int>(consts_.size()) - 1;
}

const std::string &Lexer::getName(int index) const {
    return names_.at(static_cast<size_t>(index));
}

char Lexer::getCharConst(int index) const { return consts_.at(static_cast<size_t>(index)).cVal; }
int Lexer::getIntConst(int index) const { return consts_.at(static_cast<size_t>(index)).nVal; }
bool Lexer::getBoolConst(int index) const { return consts_.at(static_cast<size_t>(index)).nVal != 0; }
int Lexer::constType(int index) const { return consts_.at(static_cast<size_t>(index)).type; }

const std::string &Lexer::getStringConst(int index) const {
    return consts_.at(static_cast<size_t>(index)).sVal;
}

} // namespace ssl
