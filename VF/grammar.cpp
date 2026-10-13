#include "grammar.hpp"

#include <array>

namespace ssl {

const std::vector<Production> &grammar() {
    static const std::vector<Production> g = {
#define SSL_X(name, lhs, ...) Production{NT_##lhs, std::vector<Sym>{__VA_ARGS__}, #name},
        SSL_RULES(SSL_X)
#undef SSL_X
    };
    return g;
}

std::string symName(Sym s) {
    static const std::array<const char *, NT_COUNT> nt = {{
        "START", "P", "LDE", "DE", "T", "DT", "DC", "DF", "LP", "B", "LDV", "LS",
        "DV", "LI", "S", "E", "L", "R", "Y", "F", "LE", "LV", "IDD", "IDU", "ID",
        "TRUE", "FALSE", "CHR", "STR", "NUM", "NB", "NF", "MF", "MT", "ME", "MW",
        "MJ", "MD", "MK", "MC"}};
    static const std::array<const char *, 51> tk = {{
        "'array'", "'boolean'", "'break'", "'char'", "'continue'", "'do'",
        "'else'", "'false'", "'function'", "'if'", "'integer'", "'of'",
        "'return'", "'string'", "'struct'", "'true'", "'type'", "'var'",
        "'while'", "':'", "';'", "','", "'='", "'['", "']'", "'{'", "'}'",
        "'('", "')'", "'&&'", "'||'", "'<'", "'>'", "'<='", "'>='", "'!='",
        "'=='", "'+'", "'++'", "'-'", "'--'", "'*'", "'/'", "'.'", "'!'",
        "caractere", "numeral", "string", "identificador", "token invalido",
        "fim do arquivo"}};
    if (isTerminal(s)) return tk[static_cast<size_t>(s)];
    return nt[static_cast<size_t>(s - kNumTokens)];
}

std::string ruleText(int rule) {
    const Production &p = grammar()[static_cast<size_t>(rule)];
    std::string text = symName(ntSym(p.lhs)) + " ->";
    if (p.rhs.empty()) text += " (vazio)";
    for (size_t i = 0; i < p.rhs.size(); ++i) text += " " + symName(p.rhs[i]);
    return text;
}

} // namespace ssl
