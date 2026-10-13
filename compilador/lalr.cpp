#include "lalr.hpp"

#include <algorithm>
#include <map>
#include <stdint.h>

namespace ssl {

namespace {

typedef uint64_t Mask;   /* conjunto de terminais (kNumTokens <= 64) */

inline Mask bit(Sym s) { return Mask(1) << s; }

struct Builder {
    const std::vector<Production> &g;
    std::vector<std::vector<int> > rulesOf;   /* regras de cada nao terminal */
    std::vector<int> base;                    /* id do item (r,0) */
    std::vector<int> itemRule, itemDot;       /* id do item -> (r,dot) */
    std::vector<bool> nullable;
    std::vector<Mask> first;                  /* FIRST dos nao terminais */

    struct State {
        std::vector<int> kernel;   /* itens do nucleo (ordenados) */
        std::vector<int> go;       /* desvio por simbolo, -1 se nao ha */
        std::vector<Mask> la;      /* lookahead de cada item do nucleo */
    };
    std::vector<State> states;
    std::map<std::vector<int>, int> stateIndex;

    explicit Builder(const std::vector<Production> &grammar) : g(grammar) {
        rulesOf.resize(NT_COUNT);
        int n = 0;
        for (size_t r = 0; r < g.size(); ++r) {
            rulesOf[g[r].lhs].push_back(static_cast<int>(r));
            base.push_back(n);
            for (size_t d = 0; d <= g[r].rhs.size(); ++d) {
                itemRule.push_back(static_cast<int>(r));
                itemDot.push_back(static_cast<int>(d));
                ++n;
            }
        }
        computeFirst();
        buildLR0();
        propagateLookaheads();
    }

    int len(int r) const { return static_cast<int>(g[static_cast<size_t>(r)].rhs.size()); }
    Sym symAt(int item) const {
        return g[static_cast<size_t>(itemRule[static_cast<size_t>(item)])]
            .rhs[static_cast<size_t>(itemDot[static_cast<size_t>(item)])];
    }
    bool atEnd(int item) const {
        return itemDot[static_cast<size_t>(item)] == len(itemRule[static_cast<size_t>(item)]);
    }

    /* ---- FIRST e anulaveis (ponto fixo) ---- */
    Mask firstOf(Sym s) const { return isTerminal(s) ? bit(s) : first[static_cast<size_t>(symNt(s))]; }

    void computeFirst() {
        nullable.assign(NT_COUNT, false);
        first.assign(NT_COUNT, 0);
        for (bool changed = true; changed;) {
            changed = false;
            for (size_t r = 0; r < g.size(); ++r) {
                size_t a = static_cast<size_t>(g[r].lhs);
                bool allNull = true;
                for (size_t i = 0; i < g[r].rhs.size() && allNull; ++i) {
                    Sym s = g[r].rhs[i];
                    Mask f = first[a] | firstOf(s);
                    if (f != first[a]) { first[a] = f; changed = true; }
                    allNull = !isTerminal(s) && nullable[static_cast<size_t>(symNt(s))];
                }
                if (allNull && !nullable[a]) { nullable[a] = true; changed = true; }
            }
        }
    }

    /* FIRST(rhs[from..]) de uma regra; 'all' indica se o sufixo e anulavel */
    Mask firstOfSuffix(int rule, int from, bool &all) const {
        const std::vector<Sym> &rhs = g[static_cast<size_t>(rule)].rhs;
        Mask m = 0;
        all = true;
        for (size_t i = static_cast<size_t>(from); i < rhs.size() && all; ++i) {
            m |= firstOf(rhs[i]);
            all = !isTerminal(rhs[i]) && nullable[static_cast<size_t>(symNt(rhs[i]))];
        }
        return m;
    }

    /* ---- automato LR(0) ---- */
    std::vector<int> closure0(const std::vector<int> &kernel) const {
        std::vector<int> items = kernel;
        std::vector<bool> added(NT_COUNT, false);
        for (size_t i = 0; i < items.size(); ++i) {
            if (atEnd(items[i])) continue;
            Sym s = symAt(items[i]);
            if (isTerminal(s) || added[static_cast<size_t>(symNt(s))]) continue;
            added[static_cast<size_t>(symNt(s))] = true;
            const std::vector<int> &rs = rulesOf[static_cast<size_t>(symNt(s))];
            for (size_t k = 0; k < rs.size(); ++k) items.push_back(base[static_cast<size_t>(rs[k])]);
        }
        return items;
    }

    int addState(const std::vector<int> &kernel) {
        std::map<std::vector<int>, int>::iterator it = stateIndex.find(kernel);
        if (it != stateIndex.end()) return it->second;
        State s;
        s.kernel = kernel;
        s.go.assign(static_cast<size_t>(numSyms()), -1);
        s.la.assign(kernel.size(), 0);
        states.push_back(s);
        int id = static_cast<int>(states.size()) - 1;
        stateIndex[kernel] = id;
        return id;
    }

    void buildLR0() {
        addState(std::vector<int>(1, base[R_START]));
        for (size_t s = 0; s < states.size(); ++s) {
            std::vector<int> items = closure0(states[s].kernel);
            std::map<Sym, std::vector<int> > next;
            for (size_t i = 0; i < items.size(); ++i)
                if (!atEnd(items[i])) next[symAt(items[i])].push_back(items[i] + 1);
            for (std::map<Sym, std::vector<int> >::iterator it = next.begin(); it != next.end(); ++it) {
                std::sort(it->second.begin(), it->second.end());
                int t = addState(it->second);
                states[s].go[static_cast<size_t>(it->first)] = t;   /* 'states' pode ter crescido */
            }
        }
    }

    /* ---- fecho LR(1) de um estado, com os lookaheads atuais ---- */
    void closure1(const State &st, std::vector<Mask> &la, std::vector<bool> &present) const {
        la.assign(itemRule.size(), 0);
        present.assign(itemRule.size(), false);
        for (size_t k = 0; k < st.kernel.size(); ++k) {
            la[static_cast<size_t>(st.kernel[k])] = st.la[k];
            present[static_cast<size_t>(st.kernel[k])] = true;
        }
        for (bool changed = true; changed;) {
            changed = false;
            for (size_t it = 0; it < itemRule.size(); ++it) {
                if (!present[it] || atEnd(static_cast<int>(it))) continue;
                Sym s = symAt(static_cast<int>(it));
                if (isTerminal(s)) continue;
                bool all;
                Mask f = firstOfSuffix(itemRule[it], itemDot[it] + 1, all);
                if (all) f |= la[it];
                const std::vector<int> &rs = rulesOf[static_cast<size_t>(symNt(s))];
                for (size_t k = 0; k < rs.size(); ++k) {
                    size_t item = static_cast<size_t>(base[static_cast<size_t>(rs[k])]);
                    if (!present[item] || (la[item] | f) != la[item]) {
                        present[item] = true;
                        la[item] |= f;
                        changed = true;
                    }
                }
            }
        }
    }

    void propagateLookaheads() {
        states[0].la[0] = bit(static_cast<Sym>(Token::END_OF_FILE));
        std::vector<Mask> la;
        std::vector<bool> present;
        for (bool changed = true; changed;) {
            changed = false;
            for (size_t s = 0; s < states.size(); ++s) {
                closure1(states[s], la, present);
                for (size_t it = 0; it < itemRule.size(); ++it) {
                    if (!present[it] || atEnd(static_cast<int>(it))) continue;
                    State &target = states[static_cast<size_t>(states[s].go[static_cast<size_t>(symAt(static_cast<int>(it)))])];
                    size_t k = static_cast<size_t>(std::lower_bound(target.kernel.begin(), target.kernel.end(),
                                                                   static_cast<int>(it) + 1) - target.kernel.begin());
                    if ((target.la[k] | la[it]) != target.la[k]) {
                        target.la[k] |= la[it];
                        changed = true;
                    }
                }
            }
        }
    }

    /* ---- Action[q][s]: empilha, desvia e reduz; conta conflitos ---- */
    ParseTable table() const {
        ParseTable t;
        for (size_t r = 0; r < g.size(); ++r) {
            t.len.push_back(len(static_cast<int>(r)));
            t.left.push_back(ntSym(g[r].lhs));
        }
        t.action.assign(states.size(), std::vector<int>(static_cast<size_t>(numSyms()), 0));

        std::vector<Mask> la;
        std::vector<bool> present;
        for (size_t s = 0; s < states.size(); ++s) {
            std::vector<int> &row = t.action[s];
            for (size_t x = 0; x < row.size(); ++x)
                if (states[s].go[x] >= 0) row[x] = states[s].go[x];

            closure1(states[s], la, present);
            for (size_t it = 0; it < itemRule.size(); ++it) {
                if (!present[it] || !atEnd(static_cast<int>(it))) continue;
                int r = itemRule[it];
                for (int a = 0; a < kNumTokens; ++a) {
                    if (!(la[it] & bit(a))) continue;
                    int &cell = row[static_cast<size_t>(a)];
                    if (cell == 0) {
                        cell = reduceOf(r);
                    } else if (isShift(cell)) {
                        /* empilha/reduz: resolvido a favor de empilhar (else pendente) */
                        Conflict c = {static_cast<int>(s), static_cast<Token>(a), true, r, -1};
                        t.conflicts.push_back(c);
                    } else if (ruleOf(cell) != r) {
                        int keep = std::min(ruleOf(cell), r), drop = std::max(ruleOf(cell), r);
                        Conflict c = {static_cast<int>(s), static_cast<Token>(a), false, keep, drop};
                        t.conflicts.push_back(c);
                        cell = reduceOf(keep);
                    }
                }
            }
        }
        return t;
    }
};

} // namespace

int ParseTable::countConflicts(bool shiftReduce) const {
    int n = 0;
    for (size_t i = 0; i < conflicts.size(); ++i)
        if (conflicts[i].shiftReduce == shiftReduce) ++n;
    return n;
}

int ParseTable::countEntries(bool shifts, bool gotos, bool reductions) const {
    int n = 0;
    for (size_t s = 0; s < action.size(); ++s)
        for (size_t x = 0; x < action[s].size(); ++x) {
            int p = action[s][x];
            if (isShift(p) && x < static_cast<size_t>(kNumTokens) && shifts) ++n;
            if (isShift(p) && x >= static_cast<size_t>(kNumTokens) && gotos) ++n;
            if (isReduction(p) && reductions) ++n;
        }
    return n;
}

ParseTable buildTable() {
    Builder b(grammar());
    return b.table();
}

} // namespace ssl
