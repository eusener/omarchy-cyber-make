# Capturas de tela

`preview.png` é uma prévia do wallpaper. `docs/images/palette.svg` mostra as
cores de `colors.toml`. Nenhuma dessas imagens representa uma captura da
interface do Omarchy.

Para registrar o tema aplicado, execute na raiz deste projeto, em um terminal
da sua sessão gráfica:

```bash
omarchy theme set cyber-make
mkdir -p docs/images/screenshots
OMASNAP_SCREENSHOT_DIR="$PWD/docs/images/screenshots" \
  omarchy capture screenshot fullscreen save
```

Para um segundo print, abra o launcher ou um menu e execute a captura com um
pequeno atraso, dando tempo de voltar à interface:

```bash
sleep 5
OMASNAP_SCREENSHOT_DIR="$PWD/docs/images/screenshots" \
  omarchy capture screenshot fullscreen save
```

O Omasnap informa o nome do PNG salvo. Revise o conteúdo visível antes de
publicar e use os nomes reais dos arquivos no README, por exemplo:

```markdown
![Desktop com o tema Cyber Make](docs/images/screenshots/desktop.png)
![Launcher com o tema Cyber Make](docs/images/screenshots/launcher.png)
```

Renomeie os arquivos correspondentes para `desktop.png` e `launcher.png` antes
de adicionar esses links. Para capturar somente uma janela ou região, use
`omarchy capture screenshot windows save` ou
`omarchy capture screenshot region save`.
