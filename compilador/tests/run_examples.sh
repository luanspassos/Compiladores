#!/bin/sh
# Compila os exemplos e compara com as saidas esperadas.
#   exemplos/*.ssl        -> devem compilar; codigo igual a exemplos/esperado/<nome>.asm
#   exemplos/erros/*.ssl  -> devem falhar (codigo de saida 1), sem gerar codigo;
#                            mensagens iguais a exemplos/esperado/erros/<nome>.txt
#   --tokens              -> mesma saida do lexer da VC (../main.cpp) em exemplos/vc_tokens.ssl
# Com a opcao --atualiza, regrava os arquivos esperados (revise-os antes de commitar).
cd "$(dirname "$0")/.." || exit 1
tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT
fail=0
update=0
[ "$1" = "--atualiza" ] && update=1
mkdir -p exemplos/esperado/erros

check() {  # check <nome> <obtido> <esperado>
    if [ $update = 1 ]; then cp "$2" "$3"; return; fi
    if ! cmp -s "$2" "$3"; then
        echo "FALHA: $1 difere de $3"; diff "$3" "$2" | head -10; fail=1
    fi
}

for f in exemplos/*.ssl; do
    n=$(basename "$f" .ssl)
    [ "$n" = vc_tokens ] && continue
    if ! ./ssl "$f" > "$tmp/$n.asm" 2> "$tmp/$n.err"; then
        echo "FALHA: $f deveria compilar"; cat "$tmp/$n.err"; fail=1; continue
    fi
    check "$f" "$tmp/$n.asm" "exemplos/esperado/$n.asm"
done

for f in exemplos/erros/*.ssl; do
    n=$(basename "$f" .ssl)
    ./ssl "$f" > "$tmp/$n.out" 2> "$tmp/$n.err"
    code=$?
    if [ $code != 1 ] || [ -s "$tmp/$n.out" ]; then
        echo "FALHA: $f deveria falhar (saida $code) sem gerar codigo"; fail=1; continue
    fi
    check "$f" "$tmp/$n.err" "exemplos/esperado/erros/$n.txt"
done

if [ -f ../lexer.cpp ] && [ -f ../main.cpp ]; then
    g++ -std=c++17 -o "$tmp/vc" ../main.cpp ../lexer.cpp 2>/dev/null &&
    "$tmp/vc" exemplos/vc_tokens.ssl > "$tmp/vc.txt" 2>/dev/null
    ./ssl --tokens exemplos/vc_tokens.ssl > "$tmp/vf.txt" 2>/dev/null
    if ! cmp -s "$tmp/vc.txt" "$tmp/vf.txt"; then echo "FALHA: --tokens difere da VC"; diff "$tmp/vc.txt" "$tmp/vf.txt" | head; fail=1; fi
fi

[ $fail = 0 ] && echo "exemplos: ok"
exit $fail
