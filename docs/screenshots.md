# Capturas de tela

A galeria do README usa capturas reais do Cyber Make aplicado no Omarchy.

| Arquivo | Conteúdo |
| --- | --- |
| `docs/images/screenshots/desktop.png` | Terminal Kitty com fastfetch e paleta, Flea no repositório e btop com CPU, memória e rede |
| `docs/images/screenshots/browser-herdr.png` | Chromium na página do Omarchy e Herdr com os arquivos do repositório |
| `studio/preview.png` | Interface do Studio com JetBrains Mono e a paleta neon |

As duas capturas do desktop estão em 2880 × 1800. `preview.png`, na raiz,
é uma prévia do wallpaper; `docs/images/palette.svg` mostra as cores de
`colors.toml`.

## Renovar as capturas

Em uma sessão gráfica, aplique o tema:

```bash
omarchy theme set cyber-make
```

Abra os aplicativos em uma área de trabalho dedicada. Use apenas arquivos do
projeto e uma página pública no navegador. Para o primeiro print, abra um
terminal, o Flea na pasta do repositório e o btop. Para o segundo, abra o
navegador em `https://omarchy.org` e o Herdr.

Na raiz do projeto, capture a tela:

```bash
mkdir -p docs/images/screenshots
OMASNAP_SCREENSHOT_DIR="$PWD/docs/images/screenshots" \
  omarchy capture screenshot fullscreen save
```

Para ter tempo de voltar à composição antes de capturar:

```bash
sleep 5
OMASNAP_SCREENSHOT_DIR="$PWD/docs/images/screenshots" \
  omarchy capture screenshot fullscreen save
```

O Omasnap informa o nome do PNG salvo. Revise a captura e substitua o arquivo
correspondente na galeria por `desktop.png` ou `browser-herdr.png`. Confirme
que os gráficos do btop estão visíveis, que o navegador mostra a aba desejada
e que nenhuma janela, notificação ou informação pessoal entrou no print.

Para capturar somente uma janela ou região:

```bash
omarchy capture screenshot windows save
omarchy capture screenshot region save
```

## Prévia do Studio

Para renovar a imagem do editor com a mesma configuração usada pelo teste
gráfico, execute na raiz do projeto:

```bash
CYBER_MAKE_STUDIO_SCREENSHOT="$PWD/studio/preview.png" studio/bin/test
```

O teste usa uma sessão Qt isolada, arquivos temporários e os textos em
português; não aplica alterações ao desktop.
