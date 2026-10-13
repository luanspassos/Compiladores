#include "parser.hpp"

namespace ssl {

/* O token 't' seria aceito a partir da pilha 'stack'? Executa as reducoes
 * (sem acoes semanticas) ate que 't' seja empilhado ou rejeitado. Como os
 * estados la-LR(1) fundem contextos, a linha da tabela pode ter reducoes
 * que so falham depois; esta simulacao deixa a lista de esperados exata. */
static bool accepts(const ParseTable &table, std::vector<int> stack, int t) {
    for (;;) {
        int p = table.action[static_cast<size_t>(stack.back())][static_cast<size_t>(t)];
        if (isShift(p)) return true;
        if (!isReduction(p)) return false;
        int r = ruleOf(p);
        if (r == R_START) return true;
        stack.resize(stack.size() - static_cast<size_t>(table.len[static_cast<size_t>(r)]));
        stack.push_back(table.action[static_cast<size_t>(stack.back())][static_cast<size_t>(table.left[static_cast<size_t>(r)])]);
    }
}

static void syntaxError(const ParseTable &table, const std::vector<int> &stack, const Tok &a, Semantics &sem) {
    std::string msg = "encontrado " + symName(static_cast<Sym>(a.token));
    if (a.token != Token::END_OF_FILE) msg += " \"" + a.lexeme + "\"";
    msg += "; esperado:";
    const char *sep = " ";
    for (int t = 0; t < kNumTokens; ++t) {
        if (!accepts(table, stack, t)) continue;
        msg += sep + symName(t);
        sep = ", ";
    }
    sem.addError(ERR_SYNTAX, a.line, a.col, msg);
}

bool parse(const ParseTable &table, const std::vector<Tok> &toks, Semantics &sem,
           std::vector<int> *reductions) {
    std::vector<int> stack(1, 0);                 /* q = 0; PUSH(0) */
    size_t next = 0;
    Tok a = toks[next++];                         /* a = nextToken() */

    for (;;) {
        int q = stack.back();
        int p = table.action[static_cast<size_t>(q)][static_cast<size_t>(a.token)];
        if (isShift(p)) {
            stack.push_back(p);
            sem.shifted(a);
            a = toks[next < toks.size() ? next++ : toks.size() - 1];
        } else if (isReduction(p)) {
            int r = ruleOf(p);
            if (reductions != nullptr) ++(*reductions)[static_cast<size_t>(r)];
            if (r == R_START) return true;        /* P reconhecido com '$': estado final */
            stack.resize(stack.size() - static_cast<size_t>(table.len[static_cast<size_t>(r)]));
            stack.push_back(table.action[static_cast<size_t>(stack.back())][static_cast<size_t>(table.left[static_cast<size_t>(r)])]);
            sem.run(r);
        } else {
            syntaxError(table, stack, a, sem);
            return false;
        }
    }
}

} // namespace ssl
