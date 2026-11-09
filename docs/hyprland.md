# Personalização opcional do Hyprland

O tema instalado com `omarchy theme install` usa o `hyprland.lua` gerado pelo
Omarchy a partir de `colors.toml`. O destaque lavanda das bordas vem de `accent`.

O arquivo Lua que antes ficava na raiz do projeto também ajustava o formato e a
transparência das janelas. Como o Omarchy ignora o `hyprland.lua` fornecido por
temas clonados, esses ajustes foram separados da instalação do tema.

Para reproduzir o arredondamento e o blur anteriores no Omarchy 4 com Hyprland
Lua, faça uma cópia de segurança do seu `~/.config/hypr/looknfeel.lua` e adicione
o trecho abaixo ao arquivo:

```lua
hl.config({
  decoration = {
    rounding = 4,
    rounding_power = 3,
    blur = {
      enabled = true,
      size = 3,
      passes = 2,
    },
  },
})
```

A opacidade pode ser ajustada pelo menu de aparência do Omarchy. A regra antiga
usava a tag `default-opacity`, que não é usada pelos padrões da versão 4.5.0-3;
por isso ela não foi mantida no exemplo.

Valide a configuração depois de salvar:

```bash
hyprctl reload
hyprctl configerrors
```

Esses ajustes são locais e continuam ativos ao trocar de tema. Para desfazê-los,
remova somente o trecho adicionado e valide novamente. Não edite arquivos em
`/usr/share/omarchy/` nem os arquivos gerados em
`~/.local/state/omarchy/current/theme/`.
