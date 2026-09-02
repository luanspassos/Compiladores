/* =====================================================================
 * main.cpp - driver de teste do Analisador Lexico (C++) da Simple
 *            Script Language
 *
 * Uso:  ./lexer_cpp  arquivo_fonte.ssl
 *       ./lexer_cpp            (le da entrada padrao)
 * ===================================================================== */

#include "lexer.hpp"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>

using ssl::Lexer;
using ssl::Token;
using ssl::tokenName;

int main(int argc, char *argv[]) {
    std::ifstream file;
    std::istream *in = &std::cin;

    if (argc > 1) {
        file.open(argv[1]);
        if (!file.is_open()) {
            std::cerr << argv[1] << ": " << std::strerror(errno) << "\n";
            return 1;
        }
        in = &file;
    }

    Lexer lex(*in);

    std::cout << std::left << std::setw(6) << "LINHA" << ' '
               << std::setw(20) << "TOKEN" << ' '
               << std::setw(12) << "SECUNDARIO" << " VALOR\n";
    std::cout << "---------------------------------------------------------\n";

    for (Token t = lex.nextToken(); t != Token::END_OF_FILE; t = lex.nextToken()) {
        std::cout << std::left << std::setw(6) << lex.lineNumber() << ' '
                   << std::setw(20) << tokenName(t) << ' ';

        if (t == Token::ID) {
            std::cout << std::setw(12) << lex.tokenSecundario() << ' '
                       << lex.getName(lex.tokenSecundario()) << "\n";
        } else if (t == Token::NUMERAL) {
            std::cout << std::setw(12) << lex.tokenSecundario() << ' '
                       << lex.getIntConst(lex.tokenSecundario()) << "\n";
        } else if (t == Token::CHARACTER) {
            std::cout << std::setw(12) << lex.tokenSecundario() << " '"
                       << lex.getCharConst(lex.tokenSecundario()) << "'\n";
        } else if (t == Token::STRINGVAL) {
            std::cout << std::setw(12) << lex.tokenSecundario() << " \""
                       << lex.getStringConst(lex.tokenSecundario()) << "\"\n";
        } else {
            std::cout << std::setw(12) << "-" << " -\n";
        }
    }
    std::cout << std::left << std::setw(6) << lex.lineNumber() << ' '
               << std::setw(20) << "END_OF_FILE" << ' '
               << std::setw(12) << "-" << " -\n";

    return 0;
}
