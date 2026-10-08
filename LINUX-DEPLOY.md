# SimCity Linux Deploy - Simple

## Build simples

### Opção 1: Make simples
```bash
make -f Makefile.linux.simple all
```

### Opção 2: Deploy script
```bash
./deploy-linux.sh
```

### Opção 3: Para Steam Deck
```bash
make -f Makefile.linux.simple steam-deck
```

## Estrutura
```
dist/linux/
├── simcity-linux    # Binary
├── run.sh          # Script de execução
└── README.md       # Instruções
```

## Uso
```bash
./dist/linux/run.sh /path/to/rom.sfc
```

Ou direto:
```bash
./dist/linux/simcity-linux --rom /path/to/rom.sfc --resolution 2 --widescreen
```

## Requisitos
- CMake 3.20+
- SDL2 2.0+
- GCC/Clang

## Steam Deck
Deploy automático com scp para deck@steamdeck:~/simcity-build/
