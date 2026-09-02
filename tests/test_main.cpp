/* Harness de testes minimo (sem dependencias externas) para o lexer C++. */
#include "../lexer.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using ssl::Lexer;
using ssl::Token;

static int g_checks = 0;
static int g_failures = 0;

#define CHECK_EQ(a, b) \
    do { \
        ++g_checks; \
        auto va = (a); \
        auto vb = (b); \
        if (!(va == vb)) { \
            ++g_failures; \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " << #a << " == " << #b << "\n"; \
        } \
    } while (0)

#define CHECK(cond) \
    do { \
        ++g_checks; \
        if (!(cond)) { \
            ++g_failures; \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " << #cond << "\n"; \
        } \
    } while (0)

static std::vector<Token> tokenize(const std::string &src) {
    std::istringstream in(src);
    Lexer lex(in);
    std::vector<Token> out;
    for (;;) {
        Token t = lex.nextToken();
        out.push_back(t);
        if (t == Token::END_OF_FILE) break;
    }
    return out;
}

/* ---- Ciclo 1: infraestrutura basica (espacos/EOF/linhas) ---- */
static void test_empty_input_is_eof() {
    std::istringstream in("");
    Lexer lex(in);
    CHECK_EQ(lex.nextToken(), Token::END_OF_FILE);
    CHECK_EQ(lex.lineNumber(), 1);
}

static void test_whitespace_only_counts_lines() {
    std::istringstream in("\n\n  \n");
    Lexer lex(in);
    CHECK_EQ(lex.nextToken(), Token::END_OF_FILE);
    CHECK_EQ(lex.lineNumber(), 4);
}

/* ---- Ciclo 2: identificadores e palavras reservadas ---- */
static void test_identifier_is_recognized_with_name() {
    std::istringstream in("abc");
    Lexer lex(in);
    CHECK_EQ(lex.nextToken(), Token::ID);
    CHECK_EQ(lex.getName(lex.tokenSecundario()), std::string("abc"));
}

static void test_repeated_identifier_reuses_same_index() {
    std::istringstream in("foo bar foo");
    Lexer lex(in);
    CHECK_EQ(lex.nextToken(), Token::ID);
    int firstFoo = lex.tokenSecundario();
    CHECK_EQ(lex.nextToken(), Token::ID); /* bar */
    CHECK_EQ(lex.nextToken(), Token::ID);
    CHECK_EQ(lex.tokenSecundario(), firstFoo);
}

static void test_all_reserved_words_recognized() {
    static const std::pair<const char *, Token> kw[] = {
        {"array", Token::ARRAY},       {"boolean", Token::BOOLEAN},
        {"break", Token::BREAK},       {"char", Token::CHAR},
        {"continue", Token::CONTINUE}, {"do", Token::DO},
        {"else", Token::ELSE},         {"false", Token::FALSE_},
        {"function", Token::FUNCTION}, {"if", Token::IF},
        {"integer", Token::INTEGER},   {"of", Token::OF},
        {"string", Token::STRING},     {"struct", Token::STRUCT},
        {"true", Token::TRUE_},        {"type", Token::TYPE},
        {"var", Token::VAR},           {"while", Token::WHILE},
    };
    for (auto &kv : kw) {
        std::istringstream in(kv.first);
        Lexer lex(in);
        CHECK_EQ(lex.nextToken(), kv.second);
    }
}

static void test_non_keyword_prefix_is_id() {
    std::istringstream in("arrayx");
    Lexer lex(in);
    CHECK_EQ(lex.nextToken(), Token::ID);
}

/* ---- Ciclo 3: numeros ---- */
static void test_number_literal() {
    std::istringstream in("42");
    Lexer lex(in);
    CHECK_EQ(lex.nextToken(), Token::NUMERAL);
    CHECK_EQ(lex.getIntConst(lex.tokenSecundario()), 42);
}

static void test_number_then_identifier() {
    auto toks = tokenize("10 abc");
    CHECK_EQ(toks.size(), size_t(3)); /* NUMERAL, ID, EOF */
    if (toks.size() == 3) {
        CHECK_EQ(toks[0], Token::NUMERAL);
        CHECK_EQ(toks[1], Token::ID);
    }
}

/* ---- Ciclo 4: strings ---- */
static void test_string_literal() {
    std::istringstream in("\"hello world\"");
    Lexer lex(in);
    CHECK_EQ(lex.nextToken(), Token::STRINGVAL);
    CHECK_EQ(lex.getStringConst(lex.tokenSecundario()), std::string("hello world"));
}

static void test_unterminated_string_is_unknown_and_stops_consuming() {
    std::istringstream in("\"abc");
    Lexer lex(in);
    CHECK_EQ(lex.nextToken(), Token::UNKNOWN);
    CHECK_EQ(lex.nextToken(), Token::END_OF_FILE); /* nao trava, segue ate o fim */
}

/* ---- Ciclo 5: caracteres ---- */
static void test_char_literal() {
    std::istringstream in("'A'");
    Lexer lex(in);
    Token t = lex.nextToken();
    CHECK_EQ(t, Token::CHARACTER);
    if (t == Token::CHARACTER) {
        CHECK_EQ(lex.getCharConst(lex.tokenSecundario()), 'A');
    }
}

static void test_unterminated_char_is_unknown() {
    std::istringstream in("'A");
    Lexer lex(in);
    CHECK_EQ(lex.nextToken(), Token::UNKNOWN);
}

static void test_malformed_char_missing_closing_quote_is_unknown() {
    std::istringstream in("'AB'");
    Lexer lex(in);
    CHECK_EQ(lex.nextToken(), Token::UNKNOWN);
}

/* ---- Ciclo 6: simbolos e operadores ---- */
static void test_single_symbols() {
    static const std::pair<const char *, Token> syms[] = {
        {":", Token::COLON},       {";", Token::SEMI_COLON},
        {",", Token::COMMA},       {"[", Token::LEFT_SQUARE},
        {"]", Token::RIGHT_SQUARE}, {"{", Token::LEFT_BRACES},
        {"}", Token::RIGHT_BRACES}, {"(", Token::LEFT_PARENTHESIS},
        {")", Token::RIGHT_PARENTHESIS}, {".", Token::DOT},
        {"*", Token::TIMES},       {"/", Token::DIVIDE},
        {"=", Token::EQUALS},      {"<", Token::LESS_THAN},
        {">", Token::GREATER_THAN}, {"!", Token::NOT},
        {"+", Token::PLUS},        {"-", Token::MINUS},
    };
    for (auto &kv : syms) {
        std::istringstream in(kv.first);
        Lexer lex(in);
        CHECK_EQ(lex.nextToken(), kv.second);
    }
}

static void test_compound_operators() {
    static const std::pair<const char *, Token> ops[] = {
        {"==", Token::EQUAL_EQUAL},  {"&&", Token::AND},
        {"||", Token::OR},           {"<=", Token::LESS_OR_EQUAL},
        {">=", Token::GREATER_OR_EQUAL}, {"!=", Token::NOT_EQUAL},
        {"++", Token::PLUS_PLUS},    {"--", Token::MINUS_MINUS},
    };
    for (auto &kv : ops) {
        std::istringstream in(kv.first);
        Lexer lex(in);
        CHECK_EQ(lex.nextToken(), kv.second);
    }
}

static void test_isolated_ampersand_is_unknown() {
    std::istringstream in("&x");
    Lexer lex(in);
    CHECK_EQ(lex.nextToken(), Token::UNKNOWN);
}

static void test_isolated_pipe_is_unknown() {
    std::istringstream in("|x");
    Lexer lex(in);
    CHECK_EQ(lex.nextToken(), Token::UNKNOWN);
}

static void test_invalid_character_reports_unknown_and_continues() {
    auto toks = tokenize("@ abc");
    CHECK_EQ(toks.size(), size_t(3));
    if (toks.size() == 3) {
        CHECK_EQ(toks[0], Token::UNKNOWN);
        CHECK_EQ(toks[1], Token::ID);
    }
}

/* ---- Ciclo 7: tokenName e integracao com exemplo.ssl ---- */
static void test_token_name() {
    CHECK_EQ(ssl::tokenName(Token::IF), std::string("IF"));
    CHECK_EQ(ssl::tokenName(Token::PLUS_PLUS), std::string("PLUS_PLUS"));
    CHECK_EQ(ssl::tokenName(Token::END_OF_FILE), std::string("END_OF_FILE"));
}

static void test_tokenizes_exemplo_ssl_without_unknown_tokens() {
    std::ifstream in("../exemplo.ssl");
    CHECK(in.is_open());
    if (!in.is_open()) return;

    Lexer lex(in);
    int total = 0;
    int unknown = 0;
    for (;;) {
        Token t = lex.nextToken();
        if (t == Token::END_OF_FILE) break;
        ++total;
        if (t == Token::UNKNOWN) ++unknown;
    }
    CHECK(total > 0);
    CHECK_EQ(unknown, 0);
}

int main() {
    test_empty_input_is_eof();
    test_whitespace_only_counts_lines();

    test_identifier_is_recognized_with_name();
    test_repeated_identifier_reuses_same_index();
    test_all_reserved_words_recognized();
    test_non_keyword_prefix_is_id();

    test_number_literal();
    test_number_then_identifier();

    test_string_literal();
    test_unterminated_string_is_unknown_and_stops_consuming();

    test_char_literal();
    test_unterminated_char_is_unknown();
    test_malformed_char_missing_closing_quote_is_unknown();

    test_single_symbols();
    test_compound_operators();
    test_isolated_ampersand_is_unknown();
    test_isolated_pipe_is_unknown();
    test_invalid_character_reports_unknown_and_continues();

    test_token_name();
    test_tokenizes_exemplo_ssl_without_unknown_tokens();

    std::cerr << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
