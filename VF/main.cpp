/* =====================================================================
 * main.cpp - compilador da Simple Script Language (VF)
 *
 * Uso:  ssl [opcoes] [arquivo.ssl]       (sem arquivo, le a entrada padrao)
 *   --tokens   so a analise lexica, com a saida da VC
 *   --table    constroi a tabela la-LR(1) e mostra estatisticas e conflitos
 *   --grammar  lista as regras numeradas
 *   -o saida   grava o codigo gerado em 'saida' (padrao: saida padrao)
 * Codigo de saida: 0 sem erros, 1 com erros, 2 uso incorreto.
 * ===================================================================== */

#include "parser.hpp"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace ssl;

static void printTokens(Lexer &lex) {
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
        } else if (t == Token::TRUE_ || t == Token::FALSE_) {
            std::cout << std::setw(12) << lex.tokenSecundario() << " "
                      << (lex.getBoolConst(lex.tokenSecundario()) ? "true" : "false") << "\n";
        } else {
            std::cout << std::setw(12) << "-" << " -\n";
        }
    }
    std::cout << std::left << std::setw(6) << lex.lineNumber() << ' '
              << std::setw(20) << "END_OF_FILE" << ' '
              << std::setw(12) << "-" << " -\n";
}

static void printTable() {
    ParseTable t = buildTable();
    std::cout << "Tabela la-LR(1)\n"
              << "  regras (com a regra aumentada): " << grammar().size() << "\n"
              << "  estados:                        " << t.numStates() << "\n"
              << "  entradas de empilha:            " << t.countEntries(true, false, false) << "\n"
              << "  entradas de desvio (goto):      " << t.countEntries(false, true, false) << "\n"
              << "  entradas de reduz:              " << t.countEntries(false, false, true) << "\n"
              << "  conflitos empilha/reduz:        " << t.countConflicts(true) << "\n"
              << "  conflitos reduz/reduz:          " << t.countConflicts(false) << "\n";
    for (size_t i = 0; i < t.conflicts.size(); ++i) {
        const Conflict &c = t.conflicts[i];
        std::cout << "  estado " << c.state << ", lookahead " << symName(static_cast<Sym>(c.token))
                  << (c.shiftReduce ? ": empilha/reduz, resolvido por empilhar; reduziria "
                                    : ": reduz/reduz, mantida a regra ")
                  << c.rule1 << " (" << ruleText(c.rule1) << ")\n";
    }
}

static void printGrammar() {
    for (size_t r = 0; r < grammar().size(); ++r)
        std::cout << std::setw(3) << r << "  " << std::left << std::setw(13) << grammar()[r].name
                  << std::right << " " << ruleText(static_cast<int>(r)) << "\n";
}

int main(int argc, char *argv[]) {
    bool tokensOnly = false;
    const char *path = nullptr, *outPath = nullptr;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--tokens") tokensOnly = true;
        else if (arg == "--table") { printTable(); return 0; }
        else if (arg == "--grammar") { printGrammar(); return 0; }
        else if (arg == "-o" && i + 1 < argc) outPath = argv[++i];
        else if (arg[0] != '-' && path == nullptr) path = argv[i];
        else {
            std::cerr << "uso: " << argv[0] << " [--tokens | --table | --grammar] [-o saida] [arquivo.ssl]\n";
            return 2;
        }
    }

    std::ifstream file;
    std::istream *in = &std::cin;
    if (path != nullptr) {
        file.open(path);
        if (!file.is_open()) {
            std::cerr << path << ": " << std::strerror(errno) << "\n";
            return 1;
        }
        in = &file;
    }
    std::string name = path != nullptr ? path : "<stdin>";

    Lexer lex(*in);

    if (tokensOnly) {
        printTokens(lex);
        for (size_t i = 0; i < lex.errors().size(); ++i)
            std::cerr << "Erro lexico (linha " << lex.errors()[i].line << "): " << lex.errors()[i].msg << "\n";
        return lex.errors().empty() ? 0 : 1;
    }

    /* Lexico: erros lexicos impedem a analise sintatica */
    std::vector<Tok> toks;
    do {
        lex.nextToken();
        toks.push_back(lex.current());
    } while (lex.token() != Token::END_OF_FILE);

    if (!lex.errors().empty()) {
        for (size_t i = 0; i < lex.errors().size(); ++i) {
            const LexError &e = lex.errors()[i];
            std::cerr << name << ":" << e.line << ":" << e.col << ": erro lexico: " << e.msg << "\n";
        }
        return 1;
    }

    /* Sintatico + escopo + tipos + codigo, numa unica passada */
    ParseTable table = buildTable();
    Semantics sem(lex);
    parse(table, toks, sem);

    if (sem.hasErrors()) {
        for (size_t i = 0; i < sem.diagnostics().size(); ++i) {
            const Diagnostic &d = sem.diagnostics()[i];
            std::cerr << name << ":" << d.line << ":" << d.col << ": erro " << d.phase() << ": " << d.msg << "\n";
        }
        std::cerr << sem.diagnostics().size() << " erro(s); nenhum codigo gerado\n";
        return 1;
    }

    if (outPath != nullptr) {
        std::ofstream out(outPath);
        if (!out.is_open()) {
            std::cerr << outPath << ": " << std::strerror(errno) << "\n";
            return 1;
        }
        out << sem.assembly();
    } else {
        std::cout << sem.assembly();
    }
    return 0;
}
