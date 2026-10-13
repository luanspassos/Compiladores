#include "symtab.hpp"

namespace ssl {

SymbolTable::SymbolTable() {
    pInt = make(-1, SCALAR_TYPE_, 1);
    pChar = make(-1, SCALAR_TYPE_, 1);
    pBool = make(-1, SCALAR_TYPE_, 1);
    pString = make(-1, SCALAR_TYPE_, 1);
    /* kind UNIVERSAL_ (e nao SCALAR_TYPE_, como na figura 6.3): todo teste
     * de kind reconhece o tipo universal e silencia erros em cascata */
    pUniversal = make(-1, UNIVERSAL_, 0);
    newBlock();   /* nivel 0: declaracoes globais (funcoes e tipos) */
}

Object *SymbolTable::make(int name, Kind kind, int size) {
    pool_.push_back(Object());
    Object *o = &pool_.back();
    o->name = name;
    o->kind = kind;
    o->size = size;
    return o;
}

int SymbolTable::newBlock() {
    levels_.push_back(std::vector<Object *>());
    return level();
}

int SymbolTable::endBlock() {
    levels_.pop_back();
    return level();
}

Object *SymbolTable::define(int name) {
    Object *o = make(name, NO_KIND_DEF_, 0);
    levels_.back().push_back(o);
    return o;
}

Object *SymbolTable::detached(int name) { return make(name, NO_KIND_DEF_, 0); }

Object *SymbolTable::search(int name) const {
    const std::vector<Object *> &lv = levels_.back();
    for (size_t i = 0; i < lv.size(); ++i)
        if (lv[i]->name == name) return lv[i];
    return nullptr;
}

Object *SymbolTable::find(int name) const {
    for (int i = level(); i >= 0; --i) {
        const std::vector<Object *> &lv = levels_[static_cast<size_t>(i)];
        for (size_t k = 0; k < lv.size(); ++k)
            if (lv[k]->name == name) return lv[k];
    }
    return nullptr;
}

} // namespace ssl
