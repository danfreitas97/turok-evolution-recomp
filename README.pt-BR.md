# turok-evolution-recomp

[English](README.md)

Turok: Evolution do Xbox original, **versão USA (NTSC)**, recompilado como um
programa nativo para Windows, macOS e Linux (incluindo SteamOS e Steam Deck).
Você usa a sua própria cópia do jogo, e ele é compilado a partir dela no seu
computador. Este repositório e os releases não contêm código nem dados do jogo.

- [Jogar](#jogar): baixe, aponte para o seu disco e jogue.
- [Compilar você mesmo](#compilar-você-mesmo): para quem trabalha no port.

# Jogar

## O que você precisa

- Turok: Evolution, a versão USA (NTSC): uma imagem de disco (`.iso` ou
  `.xiso`) ou uma pasta com os arquivos do disco. Outras versões são recusadas
  (veja [Versões do jogo](#versões-do-jogo)).
- Windows 10 ou 11 de 64 bits, um Mac com Apple Silicon (macOS 13 ou mais novo)
  ou Linux x86-64, incluindo SteamOS, com drivers Vulkan.
- Cerca de 500 MB livres, mais o espaço dos arquivos do jogo se você usar uma
  imagem de disco (a imagem é extraída uma vez; uma pasta é usada onde está).

Não é preciso instalar mais nada: o download traz tudo que é usado para
compilar o jogo.

## Baixar e iniciar

Baixe o arquivo do seu sistema no
[release mais recente](https://github.com/danfreitas97/turok-evolution-recomp/releases/latest):

| Sistema | Arquivo | Como iniciar |
| --- | --- | --- |
| Windows | `turok-<versão>-windows-x86_64.zip` | Descompacte e rode `turok.exe` |
| macOS | `turok-<versão>-macos-arm64.tar.xz` | Descompacte e veja [macOS](#macos) abaixo |
| Linux, SteamOS | `turok-<versão>-linux-x86_64.tar.xz` | Descompacte e rode `./turok` |

Na primeira vez, ele pede a sua cópia do jogo: escolha **Disc image...** ou
**Folder...**, ou cole um caminho. Depois ele compila o jogo, o que leva alguns
minutos (mais num Steam Deck), e abre as configurações. Clique em **Play**. Das
próximas vezes ele abre direto nas configurações.

O **Windows** pode dizer que protegeu o seu PC, porque o release não é
assinado: clique em Mais informações e depois em Executar assim mesmo.

### macOS

Mova `Turok Evolution.app` para Aplicativos. Ele não é assinado com um Apple
Developer ID, então antes de abrir pela primeira vez, abra o Terminal e rode

```sh
xattr -dr com.apple.quarantine "/Applications/Turok Evolution.app"
```

Isso remove a marca que o macOS coloca em tudo que é baixado, do app e das
ferramentas com que ele compila o jogo. Depois é só abrir.

### Steam Deck e SteamOS

Descompacte o release no modo desktop e rode `turok` uma vez para compilar o
jogo. Para jogar pelo Modo de Jogo, adicione `turok` à Steam (Adicionar um jogo
não Steam) e use o `icon.png` da pasta do release como ícone do atalho.

## Situação

Em andamento. Este é um port do
[phobos665/turok-evolution-recomp](https://github.com/phobos665/turok-evolution-recomp),
que roda a versão PAL (vídeos de abertura, menus, a primeira fase na selva),
para o `default.xbe` da versão USA. O toolkit é o mesmo; o que muda são os
endereços em `config/` e em `src/recomp_manual.c`. Ainda não há release
publicado; por enquanto, compile você mesmo.

Recursos herdados do port PAL: 60 fps e taxas maiores, widescreen Hor+,
renderização até 8x 640x480, tela cheia e taxa de atualização variável,
controles, teclado e mouse para olhar.

## Controles

Os controles funcionam como um controle de Xbox. O teclado joga como o
controle 1:

| Xbox | Tecla |
| --- | --- |
| Direcional | Setas |
| A, B, X, Y | Z, X, A, S |
| Start, Back | Enter, Backspace |
| White, Black | Q, W |
| Gatilho esquerdo, direito | E, R |

Mude-os na aba Input do launcher. A opção de mouse transforma o mouse no
analógico direito.

Durante o jogo:

| Tecla | |
| --- | --- |
| F9 | Mostra ou esconde a taxa de quadros |
| F10 | Muda o limite de quadros: adaptativo, 60, 30, 120, 144, sem limite |
| F11 | Salva uma captura de tela e uma captura do quadro, para relatar bugs |
| Alt+Enter | Alterna entre tela cheia e janela |

## Configurações

Na aba Video do launcher:

| Opção | O que faz |
| --- | --- |
| Resolution | Renderiza de 1 a 8 vezes 640x480 e reduz. 2 já fica bem mais nítido e custa pouco |
| Widescreen | 16:9, com a câmera mais aberta |
| Texture sharpness | Filtragem anisotrópica, de 1 a 16 |
| Frame rate | Adaptativo (o padrão, 60 como no console), 60, 30, 90, 120, 144, 165, 240 ou sem limite |
| Show frame rate | Começa com o contador ligado |
| Fullscreen | Tela cheia sem bordas |
| Variable refresh rate | Em tela cheia, os quadros vão para a tela assim que ficam prontos. Precisa estar ligado também no driver do monitor |

Para um movimento mais suave, aumente o limite de quadros até a taxa do seu
monitor em vez de usar interpolação de quadros, que não é ajustada para este
jogo. Sem limite, o jogo pode passar de várias centenas de quadros por segundo,
e as telas de carregamento de milhares; um limite economiza energia, o que
importa num portátil.

## Onde as coisas ficam

O jogo compilado, os saves e os logs ficam numa pasta por usuário:

| | |
| --- | --- |
| Windows | `%LOCALAPPDATA%\xboxrecomp\builds\41430004\` |
| macOS | `~/Library/Application Support/xboxrecomp/builds/41430004/` |
| Linux | `~/.local/share/xboxrecomp/builds/41430004/` |

Lá dentro, `game/UDATA/41430004/` guarda os seus saves, `build.log` é o log da
compilação e `turok_recomp.log` o do jogo. O seu disco ou pasta nunca é
alterado.

As configurações e os controles ficam separados da compilação:

| | Configurações | Controles |
| --- | --- | --- |
| Windows | `%APPDATA%\xboxrecomp\titles\41430004.conf` | `%APPDATA%\xboxrecomp\input_bindings.json` |
| macOS | `~/Library/Application Support/xboxrecomp/titles/41430004.conf` | `~/Library/Application Support/xboxrecomp/input_bindings.json` |
| Linux | `~/.config/xboxrecomp/titles/41430004.conf` | `~/.config/xboxrecomp/input_bindings.json` |

## Atualizar

Baixe o release novo e inicie. Ele percebe que o jogo foi compilado por um
release antigo e oferece **Build again**, lembrando o seu disco. Saves e
configurações continuam.

## Problemas

- O disco é recusado: não é a versão listada abaixo. Abra uma issue com o
  título, a região e o SHA-1 que a mensagem mostra.
- A compilação falha: abra uma issue e anexe o `build.log` (veja [Onde as
  coisas ficam](#onde-as-coisas-ficam)).
- Só uma janela preta: anexe o `turok_recomp.log` da mesma pasta.
- Um erro de imagem num lugar específico: aperte F11 ali e anexe os arquivos
  `.bmp` e `.d3dcap` que ele grava ao lado do jogo.

## Versões do jogo

O jogo é reconhecido pelo SHA-1 do seu `default.xbe`:

| Versão | SHA-1 do `default.xbe` |
| --- | --- |
| USA (NTSC) | `25e1d77b216a2e557d7a5de6266cda64ae62627b` |

Qualquer outro disco é recusado; abra uma issue com o SHA-1 dele. Para a versão
europeia (PAL), use o
[phobos665/turok-evolution-recomp](https://github.com/phobos665/turok-evolution-recomp).

# Compilar você mesmo

Para quem trabalha no port. Isto compila o mesmo jogo à mão, a partir de um
clone, com o toolkit
[DirectXbox-Recomp](https://github.com/phobos665/DirectXbox-Recomp)
(`xboxrecomp/` aqui): a compilação traduz o código do jogo para C e depois o
compila.

## O que você precisa para compilar

Turok: Evolution, a versão USA (NTSC), extraído do seu disco. A compilação lê o
`default.xbe` e avisa se for outra versão.

Windows 10 ou 11, 64 bits:

- Visual Studio 2022 ou 2019 com a carga de trabalho Desenvolvimento para
  desktop com C++
- [CMake](https://cmake.org/download/) 3.20 ou mais novo, no PATH
- Python 3.10 ou mais novo com `capstone`: `py -3 -m pip install capstone`
- Git

Linux, x86-64:

- GCC ou Clang, CMake 3.20 ou mais novo, Ninja, pkg-config, Git, curl
- Headers de desenvolvimento de X11 e Wayland, PulseAudio ou PipeWire, OpenSSL,
  FreeType e fontconfig
- Drivers Vulkan (RADV ou ANV do Mesa, ou o da NVIDIA)
- Python 3.10 ou mais novo com `capstone`, num ambiente virtual:
  `python3 -m venv ~/.venvs/recomp && ~/.venvs/recomp/bin/pip install capstone`

No Debian ou Ubuntu:

```sh
sudo apt install build-essential cmake ninja-build pkg-config git curl python3-venv \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev \
    libxfixes-dev libxtst-dev libxkbcommon-dev libwayland-dev wayland-protocols \
    libegl-dev libdrm-dev libgbm-dev libpulse-dev libasound2-dev libudev-dev \
    libssl-dev libfreetype-dev libfontconfig-dev
```

O SteamOS não tem compilador e o sistema é somente leitura, então compile
dentro do Freedesktop SDK, que roda como Flatpak e tem tudo isso. Muitas vezes
ele já está instalado, porque apps Flatpak como o VS Code o usam; se
`flatpak list` não mostrar `org.freedesktop.Sdk` 25.08, instale com
`flatpak install flathub org.freedesktop.Sdk//25.08`. Abra um shell nele pelo
Konsole:

```sh
flatpak run --filesystem=home --share=network --device=dri \
    --socket=wayland --socket=fallback-x11 --socket=pulseaudio \
    --command=bash org.freedesktop.Sdk//25.08
```

e faça tudo abaixo nesse shell, incluindo o venv. O jogo compilado também roda
fora dele, direto no SteamOS.

## Compilar

```sh
git clone --recursive https://github.com/danfreitas97/turok-evolution-recomp.git
cd turok-evolution-recomp
```

Copie o disco extraído para uma pasta chamada `game`, de forma que exista
`game/default.xbe` com `Data/`, `Game.txt` e os arquivos `supertree*.tre` ao
lado.

No Linux, baixe antes o compilador de shaders (uma vez; as distribuições não o
empacotam):

```sh
bash xboxrecomp/tools/linux/fetch_dxc.sh
```

Depois:

```sh
py -3 scripts/build.py                        # Windows
~/.venvs/recomp/bin/python scripts/build.py   # Linux
```

A primeira execução traduz o jogo, o que leva alguns minutos, e depois compila,
o que demora mais. As próximas só recompilam. O programa fica em
`build/Release/`:

| | Windows | Linux |
| --- | --- | --- |
| O jogo | `turok_recomp.exe` | `turok_recomp` |
| Janela de configurações, depois o jogo | `turok_recomp_launcher.exe` | `turok_recomp_launcher` |

Se os seus jogos extraídos ficam em outro lugar, `game` pode ser um link para a
pasta (`ln -s "/caminho/para/Turok Evolution" game`), ou passe `--game PASTA` na
compilação e defina `RECOMP_GAME_DIR` ao jogar.

Se você clonou sem `--recursive`, rode `git submodule update --init --recursive`.

## Rodando uma compilação sua

Os controles e configurações são os de [Jogar](#jogar). Toda configuração é
também uma linha no arquivo de configurações e uma variável de ambiente, que
tem prioridade sobre o arquivo:

| Opção | Arquivo | Variável de ambiente |
| --- | --- | --- |
| Resolution | `resolution_scale` | `RECOMP_RES_SCALE` |
| Widescreen | `widescreen` | `RECOMP_WIDESCREEN` |
| Texture sharpness | `anisotropy` | `RECOMP_ANISO` |
| Frame rate | `frame_cap` (0 para sem limite) | `RECOMP_FPS_CAP` |
| Show frame rate | `fps_overlay` | `RECOMP_FPS_OVERLAY` |
| Fullscreen | `fullscreen` | `RECOMP_FULLSCREEN` |
| Variable refresh rate | `vrr` | `RECOMP_VRR` |

Os arquivos de configurações e controles estão em [Onde as coisas
ficam](#onde-as-coisas-ficam); o formato dos controles está em
`xboxrecomp/docs/technical/input-binding.md`. O que muda em relação a um
release:

| | Windows | Linux |
| --- | --- | --- |
| Saves | `game\UDATA\41430004\` | `game/UDATA/41430004/` |
| Log, quando não iniciado por um terminal | `build\Release\turok_recomp.log` | `build/Release/turok_recomp.log` |

## Outras versões do jogo

`config/` lista endereços dentro do `default.xbe` da versão USA: pontos de
entrada de funções de que a tradução precisa e as funções do XDK que o toolkit
substitui. As correções em `src/recomp_manual.c` também se prendem a endereços
dele. Qualquer outro disco precisa dos seus próprios endereços. A compilação
mostra um aviso com o SHA-1 e continua, mas o resultado provavelmente não vai
rodar; abra uma issue com ele.

## Atualizar um clone

```sh
git pull
git submodule update --init --recursive
py -3 scripts/build.py --relift
```

No Linux, use `~/.venvs/recomp/bin/python` no lugar de `py -3`. `--relift`
traduz o jogo de novo, o que é necessário sempre que o toolkit, `config/` ou
`src/recomp_manual.c` mudam. `--relift --from lift` repete só a última etapa, o
que basta depois de uma mudança em `src/recomp_manual.c`.

## Fazer um release

Enviar uma tag de versão (`git tag v1.0 && git push origin v1.0`) roda o
`.github/workflows/release.yml`, que compila os downloads de Windows, macOS e
Linux a partir de `release/turok.json` e os anexa ao release da tag no GitHub.
Nenhum arquivo do jogo é envolvido. Como os downloads funcionam está no
[self-build-release.md](https://github.com/phobos665/DirectXbox-Recomp/blob/main/docs/technical/self-build-release.md)
do toolkit.

## Problemas na compilação

- "no default.xbe in ...": os arquivos do disco não estão em `game/`.
- Linux, "DXC (the shader compiler) is missing": rode
  `bash xboxrecomp/tools/linux/fetch_dxc.sh` e compile de novo.

# Créditos

- [phobos665/turok-evolution-recomp](https://github.com/phobos665/turok-evolution-recomp)
  (GPL-3.0): o port PAL em que este repositório se baseia.
- [phobos665/DirectXbox-Recomp](https://github.com/phobos665/DirectXbox-Recomp)
  (GPL-3.0): o toolkit, `xboxrecomp/` aqui.
- [sp00nznet/xboxrecomp](https://github.com/sp00nznet/xboxrecomp) (MIT): o
  toolkit original do qual o DirectXbox-Recomp é um fork.

# Aviso legal

Você precisa da sua própria cópia de Turok: Evolution. Não compartilhe os
arquivos do jogo, o jogo compilado, o `src/recomp/` de um clone (é o código
traduzido do jogo) nem nada em `build/`. Este repositório, os releases e o
xboxrecomp não contêm código nem arquivos do jogo.

O código deste repositório é licenciado sob a GNU General Public License v3.0;
veja [LICENSE](LICENSE). Isso cobre os arquivos versionados aqui, não o jogo.
