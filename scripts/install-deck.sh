#!/bin/sh
#
# install-deck.sh — one-command install for the Steam Deck.
#
#   curl -fsSL https://raw.githubusercontent.com/linuxkafe/SimCity-SNES-Static-Recomp/main/scripts/install-deck.sh | sh
#
# What it does, in order:
#   1. picks an install directory: the microSD card if one is mounted, else home
#   2. clones or updates the source there (git pull if already present)
#   3. checks the build dependencies and says exactly what to install if any
#      are missing
#   4. asks for the ROM and copies it in
#   5. builds
#   6. writes a launcher and registers it with Steam
#
# Run it with --help for the flags. Everything it changes is under one
# directory, plus the Steam shortcut, and it prints each step as it goes.

set -eu

REPO_URL="${SIMCITY_REPO:-https://github.com/linuxkafe/SimCity-SNES-Static-Recomp.git}"
BRANCH="${SIMCITY_BRANCH:-main}"
REL_SUBDIR="Games/simcity-snes"

# Where the script is allowed to install, most preferred first. A microSD card
# mounted by the SteamOS automounter appears under /run/media/<user>/<label>.
# Fall back to the home directory when there is none, or when the card turns out
# not to be writable.
candidate_roots() {
    for m in /run/media/*/*; do
        [ -d "$m" ] && printf '%s\n' "$m"
    done
    printf '%s\n' "$HOME"
}

say()  { printf '\n\033[1;36m==>\033[0m %s\n' "$*"; }
info() { printf '    %s\n' "$*"; }
warn() { printf '\033[1;33maviso:\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31merro:\033[0m %s\n' "$*" >&2; exit 1; }

usage() {
    cat <<'EOF'
Instala o SimCity SNES Static Recomp no Steam Deck.

Uso:
  curl -fsSL <url>/scripts/install-deck.sh | sh
  ... | sh -s -- [opcoes]

Opcoes:
  --dir CAMINHO     Instalar em CAMINHO em vez de escolher automaticamente.
  --rom CAMINHO     Usar esta ROM em vez de perguntar.
  --no-steam        Nao registar o atalho no Steam.
  --no-deps         Nao instalar dependencias; apenas verificar.
  --from-source     Compilar do fonte, ignorando o binario pre-compilado.
  --force           Reinstalar mesmo que o directorio ja exista.
  -h, --help        Esta ajuda.

Variaveis de ambiente:
  SIMCITY_REPO      URL do repositorio git.
  SIMCITY_BRANCH    Ramo a usar (por omissao: main).
EOF
}

want_rom=""
install_dir=""
no_steam=0
no_sudo=0
prefer_source=0
force=0

while [ $# -gt 0 ]; do
    case "$1" in
        --dir)      [ $# -ge 2 ] || die "--dir precisa de um caminho"; install_dir="$2"; shift 2 ;;
        --rom)      [ $# -ge 2 ] || die "--rom precisa de um caminho"; want_rom="$2"; shift 2 ;;
        --no-steam) no_steam=1; shift ;;
        --no-deps)   no_sudo=1; shift ;;
        --from-source) prefer_source=1; shift ;;
        --force)    force=1; shift ;;
        -h|--help)  usage; exit 0 ;;
        *)          die "opcao desconhecida: $1 (tente --help)" ;;
    esac
done

# ---------------------------------------------------------------------------
# 1. Choose where to install.
# ---------------------------------------------------------------------------
say "A escolher o directorio de instalacao"

if [ -z "$install_dir" ]; then
    chosen=""
    sd_note=""
    for root in $(candidate_roots); do
        target="$root/$REL_SUBDIR"
        if [ ! -d "$root" ] || [ ! -w "$root" ]; then
            continue
        fi
        # A microSD is the preferred home for this: it is the card the user can
        # remove, and it leaves the internal disk alone. Only report it as such
        # when it really is removable media rather than the internal partition.
        case "$root" in
            /run/media/*) sd_note="cartao microSD" ;;
            *)            sd_note="directorio pessoal" ;;
        esac
        if [ -z "$chosen" ]; then
            chosen="$target"; picked="$sd_note"
            info "$picked: $target"
        else
            info "alternativa disponivel: $target"
        fi
    done
    [ -n "$chosen" ] || die "nao encontrei nenhum sitio gravavel para instalar"
    install_dir="$chosen"
fi

mkdir -p "$install_dir" || die "nao consegui criar $install_dir"
[ -w "$install_dir" ] || die "$install_dir nao e gravavel"
info "instalar em: $install_dir"

# ---------------------------------------------------------------------------
# 2. Fetch the source.
# ---------------------------------------------------------------------------
say "A obter o codigo-fonte"

src="$install_dir/src"
if [ -d "$src/.git" ]; then
    if [ "$force" -eq 1 ]; then
        info "a remover a copia anterior (--force)"
        rm -rf "$src"
    else
        info "actualizando a copia existente"
        if ! git -C "$src" pull --ff-only origin "$BRANCH"; then
            warn "o git pull falhou (divergencia ou rede); a usar o que ja existe"
        fi
    fi
fi

if [ ! -d "$src/.git" ]; then
    command -v git >/dev/null 2>&1 || die "git nao esta instalado (pacman -S git)"
    info "clonar $BRANCH de $REPO_URL"
    git clone --depth 1 --branch "$BRANCH" "$REPO_URL" "$src" \
        || die "o clone falhou. Se este repositorio e' privado, clone-o antes e use --dir"
fi

[ -f "$src/CMakeLists.txt" ] || die "$src nao parece um checkout valido (falta CMakeLists.txt)"

# ---------------------------------------------------------------------------
# 3. Get the executable.
#
# The prebuilt binary is published as a release asset so most people never need
# a compiler.  It is checked against its sha256 and then run to confirm it is a
# working executable, and building from source stays available as a fallback.
# The binary contains no game data: scripts/make-release.sh proves that with the
# ROM before publishing, and scripts/verify-release.py re-checks it.
# ---------------------------------------------------------------------------
say "A obter o executavel"

RELEASE_BASE="${SIMCITY_RELEASE_BASE:-https://github.com/linuxkafe/SimCity-SNES-Static-Recomp/releases/latest/download}"
binary=""
release_ok=0

if [ "$prefer_source" -eq 0 ]; then
    info "a tentar o binario pre-compilado de $RELEASE_BASE"
    tmpd=$(mktemp -d 2>/dev/null || printf '%s' "$install_dir/.dl")
    mkdir -p "$tmpd"
    bin_url="$RELEASE_BASE/simcity-linux"
    sum_url="$RELEASE_BASE/simcity-linux.sha256"

    if command -v curl >/dev/null 2>&1; then
        curl -fsSL --retry 2 -o "$tmpd/simcity-linux" "$bin_url" 2>/dev/null || true
        curl -fsSL --retry 2 -o "$tmpd/simcity-linux.sha256" "$sum_url" 2>/dev/null || true
    elif command -v wget >/dev/null 2>&1; then
        wget -q -O "$tmpd/simcity-linux" "$bin_url" 2>/dev/null || true
        wget -q -O "$tmpd/simcity-linux.sha256" "$sum_url" 2>/dev/null || true
    fi

    if [ -s "$tmpd/simcity-linux" ] && [ -s "$tmpd/simcity-linux.sha256" ]; then
        expected=$(cut -d' ' -f1 < "$tmpd/simcity-linux.sha256" 2>/dev/null || true)
        actual=$(sha256sum "$tmpd/simcity-linux" 2>/dev/null | cut -d' ' -f1 || true)
        if [ -n "$expected" ] && [ "$expected" = "$actual" ]; then
            info "sha256 confirmado"
            chmod +x "$tmpd/simcity-linux"
            # Prove it actually runs before trusting it.
            if "$tmpd/simcity-linux" --help >/dev/null 2>&1; then
                mkdir -p "$install_dir/bin"
                cp "$tmpd/simcity-linux" "$install_dir/bin/simcity-linux"
                chmod +x "$install_dir/bin/simcity-linux"
                binary="$install_dir/bin/simcity-linux"
                release_ok=1
                info "executavel pronto: $binary"
            else
                warn "o binario descarregado nao executa; a compilar do fonte"
            fi
        else
            warn "sha256 nao coincide; a compilar do fonte"
        fi
    else
        info "nao foi possivel descarregar o binario; a compilar do fonte"
    fi
    rm -rf "$tmpd"
fi

if [ "$release_ok" -eq 0 ]; then
    # ------------------------------------------------------------------------
    # Build from source.
    # ------------------------------------------------------------------------
    say "A compilar a partir do fonte"

    missing=""
    need_tools=""
    for tool in git cc c++ cmake make pkg-config; do
        command -v "$tool" >/dev/null 2>&1 || need_tools="$need_tools $tool"
    done
    [ -n "$need_tools" ] && missing="$missing$need_tools"
    pkg-config --exists sdl2 2>/dev/null || missing="$missing sdl2"

    # A real question is whether a C++ file can be compiled at all, and checking
    # for individual headers keeps missing cases: this image has gcc, glibc and
    # linux-api-headers installed yet still cannot find linux/limits.h.  A probe
    # compile answers the question that actually matters.
    if [ -z "$missing" ] && command -v c++ >/dev/null 2>&1; then
        probe=$(mktemp -d 2>/dev/null || printf '%s' "$install_dir/.probe")
        mkdir -p "$probe"
        printf '#include <SDL.h>\nint main(void){return 0;}\n' > "$probe/p.cpp"
        if ! c++ -fsyntax-only "$probe/p.cpp" > "$probe/log" 2>&1; then
            missing="$missing build-probe"
        fi
        rm -rf "$probe"
    fi

    if [ -n "$missing" ]; then
        info "em falta:$missing"

        set -- base-devel cmake git pkgconf
        case "$missing" in
            *sdl2*) set -- "$@" sdl2-compat ;;
        esac
        case "$missing" in
            *build-probe*) set -- "$@" glibc linux-api-headers ;;
        esac

        if ! command -v pacman >/dev/null 2>&1; then
            warn "isto nao e um sistema Arch; instale as ferramentas de compilacao e SDL2 manualmente"
            printf '    Deps:'
            for p in "$@"; do printf ' %s' "$p"; done
            printf '\n'
            exit 1
        fi
        if [ "$no_sudo" -eq 1 ]; then
            warn "--no-deps: a instalacao das dependencias foi saltada"
            printf '    Instale manualmente:  sudo pacman -S --needed'
            for p in "$@"; do printf ' %s' "$p"; done
            printf '\n'
            exit 1
        fi

        say "A instalar as dependencias (sudo)"
        if sudo -n true 2>/dev/null; then
            sudo pacman -S --needed --noconfirm "$@" \
                || die "a instalacao das dependencias falhou"
        else
            printf '\n'
            printf '    Compilar precisa de dependencias e, being piped in, nao pode\n'
            printf '    pedir a palavra-passe. Corra:\n'
            printf '\n'
            printf '      sudo pacman -S --needed'
            for p in "$@"; do printf ' %s' "$p"; done
            printf '\n'
            printf '\n'
            printf '    Ou instale primeiro e corra este comando com --no-deps.\n'
            exit 1
        fi

        still=""
        for tool in git cc c++ cmake make pkg-config; do
            command -v "$tool" >/dev/null 2>&1 || still="$still $tool"
        done
        pkg-config --exists sdl2 2>/dev/null || still="$still sdl2"
        if [ -z "$still" ] && command -v c++ >/dev/null 2>&1; then
            probe=$(mktemp -d 2>/dev/null || printf '%s' "$install_dir/.probe2")
            mkdir -p "$probe"
            printf '#include <SDL.h>\nint main(void){return 0;}\n' > "$probe/p.cpp"
            c++ -fsyntax-only "$probe/p.cpp" >/dev/null 2>&1 || still="$still build-probe"
            rm -rf "$probe"
        fi
        [ -z "$still" ] || die "ainda em falta apos instalar:$still"
    fi
    info "dependencias ok"

    build="$install_dir/build"
    cmake -S "$src" -B "$build" -DCMAKE_BUILD_TYPE=Release > "$install_dir/build.log" 2>&1 \
        || { warn "a configuracao falhou, log em $install_dir/build.log"; tail -20 "$install_dir/build.log"; exit 1; }
    cmake --build "$build" -j "$(nproc)" >> "$install_dir/build.log" 2>&1 \
        || { warn "a compilacao falhou, log em $install_dir/build.log"; tail -20 "$install_dir/build.log"; exit 1; }

    binary="$build/frontend/linux/simcity-linux"
    [ -x "$binary" ] || die "a compilacao terminou mas o binario nao esta em $binary"
    info "binario: $binary"
fi

# ---------------------------------------------------------------------------
# 4. The ROM.  It is copyrighted, so it is never downloaded.
# ---------------------------------------------------------------------------
say "A ROM"

rom_dir="$install_dir/rom"
mkdir -p "$rom_dir"
rom="$rom_dir/SimCity.sfc"

if [ -f "$want_rom" ]; then
    cp "$want_rom" "$rom" || die "nao consegui copiar a ROM"
elif [ -f "$rom" ]; then
    info "ja esta em: $rom"
else
    printf '\n'
    printf '    Este jogo precisa da ROM original "SimCity (USA).sfc",\n'
    printf '    que nao pode ser descarregada automaticamente.\n'
    printf '\n'
    printf '    Copie-a para: %s\n' "$rom"
    printf '    Se ja a tem noutro sitio, indique o caminho aqui: '
    read -r answer
    if [ -n "$answer" ] && [ -f "$answer" ]; then
        cp "$answer" "$rom" || die "nao consegui copiar a ROM"
    else
        printf '    Sem ROM, cancellingo. Corra isto outra vez depois de a colocar.\n'
        exit 1
    fi
fi

size=$(wc -c < "$rom")
if [ "$size" -ne 524288 ]; then
    die "a ROM tem $size bytes; esperava-se 524288 (512 KiB). Nao e a ROM certa."
fi
info "ROM pronta ($size bytes)"

# ---------------------------------------------------------------------------
# 6. Launcher.
# ---------------------------------------------------------------------------
say "A criar o launcher"

cat > "$install_dir/run.sh" <<EOF
#!/bin/sh
# Gerado por install-deck.sh
cd "$src" || exit 1
exec "$binary" --rom "$rom" --size 1280x800 --widescreen --soft-mouse "\$@"
EOF
chmod +x "$install_dir/run.sh"
info "$install_dir/run.sh"

# A .desktop entry as well, so the game shows up outside Steam too.
desktop_dir="$HOME/.local/share/applications"
mkdir -p "$desktop_dir"
cat > "$desktop_dir/simcity-snes.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=SimCity SNES
Comment=SimCity (SNES) static recompilation
Exec=$install_dir/run.sh
Icon=applications-games
Terminal=false
Categories=Game;
EOF
info "$desktop_dir/simcity-snes.desktop"

# ---------------------------------------------------------------------------
# 7. Steam shortcut.
# ---------------------------------------------------------------------------
if [ "$no_steam" -eq 1 ]; then
    say "Atalho do Steam ignorado (--no-steam)"
else
    say "A registar o atalho no Steam"

    steam_root=""
    for r in "$HOME/.local/share/Steam" "$HOME/.steam/steam" "$HOME/.steam/root"; do
        [ -d "$r" ] && { steam_root="$r"; break; }
    done

    if [ -z "$steam_root" ]; then
        warn "Steam nao encontrado; use o atalho do menu de aplicacoes"
    elif pgrep -x steam >/dev/null 2>&1; then
        # Writing shortcuts.vdf under a running Steam is lost when Steam exits,
        # so refuse rather than pretend it worked.
        warn "o Steam esta a correr e sobrescreve o ficheiro de atalhos ao sair."
        printf '\n'
        printf '    Feche o Steam e corra este comando outra vez, ou\n'
        printf '    lance agora pelo atalho do menu de aplicacoes.\n'
    else
        userdata=""
        for u in "$steam_root"/userdata/*/config; do
            [ -d "$u" ] && userdata="$u" && break
        done
        if [ -z "$userdata" ]; then
            warn "nao encontrei userdata/config; registe o atalho a mao"
        else
            vdf="$userdata/shortcuts.vdf"
            if SIMCITY_VDF="$vdf" SIMCITY_NAME="SimCity SNES" \
               SIMCITY_EXE="$install_dir/run.sh" SIMCITY_START="$src" \
               python3 "$src/scripts/add-steam-shortcut.py"; then
                info "atalho registado no Steam"
            else
                warn "nao consegui registar o atalho; use o launcher do menu de aplicacoes"
            fi
        fi
    fi
fi

say "Instalacao concluida"
printf '\n'
printf '    directório : %s\n' "$install_dir"
printf '    Executar   : %s/run.sh\n' "$install_dir"
printf '    ROM        : %s\n' "$rom"
printf '\n'