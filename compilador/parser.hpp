/* =====================================================================
 * parser.hpp
 *
 * Analisador sintatico ascendente dirigido por tabela, com o algoritmo
 * da figura 4.6 (Action[q][a], Len[r], Left[r], Semantics(r)).
 * ===================================================================== */

#ifndef SSL_PARSER_HPP
#define SSL_PARSER_HPP

#include "lalr.hpp"
#include "semantics.hpp"

#include <vector>

namespace ssl {

/* Retorna true se o programa e sintaticamente correto. Em erro sintatico
 * registra um diagnostico (linha, coluna, token encontrado e esperados)
 * em 'sem' e para: nao ha recuperacao de erros. 'toks' termina com END_OF_FILE.
 * Se 'reductions' nao for nulo, conta quantas vezes cada regra foi reduzida
 * (usado pelos testes para verificar que todas as regras foram exercitadas). */
bool parse(const ParseTable &table, const std::vector<Tok> &toks, Semantics &sem,
           std::vector<int> *reductions = nullptr);

} // namespace ssl

#endif // SSL_PARSER_HPP
