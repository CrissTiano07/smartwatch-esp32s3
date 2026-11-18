# components/ui_fonts — fontes próprias da UI

Fontes geradas com [`lv_font_conv`](https://github.com/lvgl/lv_font_conv) a partir
do **Montserrat Regular**. Mantidas versionadas (o `.c` gerado entra no git) para
não depender de ter Node/rede na hora do build.

## Fontes

| Arquivo | Fonte base | Tamanho | bpp | Glifos |
|---------|-----------|---------|-----|--------|
| `font_digits_180.c` → `font_digits_180` | Montserrat Regular | 180 px (~128 px reais) | 4 | `0-9` (`0x30-0x39`) |

Sem `:` de propósito: o relógio empilha **HH** sobre **MM**, centralizados, sem dois-pontos (ver `docs/DECISIONS.md` #12).

## Como regenerar (reproduzível)

Pré-requisitos: **Node** (testado com v22) — o `npx` baixa o `lv_font_conv` na hora.

Arquivos-fonte versionados em [`tools/fonts/`](../../tools/fonts/):
- `Montserrat-Regular.ttf` — fonte base (repo oficial `JulietaUla/Montserrat`).
- `OFL.txt` — licença SIL Open Font License 1.1 do Montserrat.

Comando **exato** (rodar da raiz do projeto):

```sh
npx --yes lv_font_conv@1.5.3 \
  --font tools/fonts/Montserrat-Regular.ttf \
  --size 180 --bpp 4 --range 0x30-0x39 \
  --format lvgl --lv-include lvgl.h --no-compress \
  -o components/ui_fonts/font_digits_180.c
```

No PowerShell, usar numa linha só (sem as `\`).

Notas:
- `--range 0x30-0x39` = só os dígitos `0`..`9` (mantém a fonte pequena).
- `--no-compress` = bitmap sem compressão (dispensa descompressão em runtime; com
  10 glifos o custo em flash é baixo).
- O arquivo gerado tem guards de versão do LVGL e compila no **LVGL 9.5**.

## Licença da fonte

O Montserrat é distribuído sob a **SIL Open Font License 1.1** — ver
[`tools/fonts/OFL.txt`](../../tools/fonts/OFL.txt). A licença cobre também os
bitmaps gerados (`font_digits_180.c`).
