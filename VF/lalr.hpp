/* =====================================================================
 * lalr.hpp
 *
 * Construcao da tabela de analise la-LR(1) (cap. 3.8 do livro) a partir
 * da gramatica: FIRST, automato LR(0), propagacao de lookaheads por
 * iteracao ate o ponto fixo (equivale a construir os conjuntos LR(1) e
 * fundir os estados de mesmo nucleo) e preenchimento de Action[q][a].
 * ===================================================================== */

#ifndef SSL_LALR_HPP
#define SSL_LALR_HPP

#include "grammar.hpp"

#include <vector>

namespace ssl {

/* Codificacao das entradas de Action[q][s]:
 *    0      erro
 *    p > 0  empilha o estado p (terminal) / desvio para p (nao terminal)
 *    p < 0  reduz pela regra RULE(p)                                     */
inline bool isShift(int p) { return p > 0; }
inline bool isReduction(int p) { return p < 0; }
inline int ruleOf(int p) { return -p - 1; }
inline int reduceOf(int rule) { return -(rule + 1); }

struct Conflict {
    int state;
    Token token;
    bool shiftReduce;   /* false: reduz/reduz */
    int rule1, rule2;   /* regra reduzida (e, em reduz/reduz, a descartada) */
};

struct ParseTable {
    std::vector<std::vector<int> > action;   /* [estado][simbolo] */
    std::vector<int> len;                    /* Len[r]  */
    std::vector<int> left;                   /* Left[r] = simbolo do lado esquerdo */
    std::vector<Conflict> conflicts;

    int numStates() const { return static_cast<int>(action.size()); }
    int countConflicts(bool shiftReduce) const;
    int countEntries(bool shifts, bool gotos, bool reductions) const;
};

ParseTable buildTable();

} // namespace ssl

#endif // SSL_LALR_HPP
