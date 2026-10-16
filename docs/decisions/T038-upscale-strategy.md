# T038: Runtime Upscaling via Texture Scale Mode

## Objective
Substituir o "stretch" padrão do SDL (nearest-point) por um algoritmo de upscaling suave para evitar pixéis quadrados/duros em janelas grandes.

## Current State
- Frontend usa `SDL_TEXTUREACCESS_STREAMING` + `SDL_RenderCopy`.
- Resolução de output estática: 398×239 (widescreen) ou 256×239 (standard).
- Render mode atual não aplica filtering (pixel art puro).

## Proposed Approaches (AES Decision)
1.  **SDL 2.0.12+ Texture Scale Mode:** Usar `SDL_SetTextureScaleMode(g_texture, SDL_ScaleModeLinear)`. É a via mais segura, sem escrever GL shader manual, mas menos "premium" que FSR.
2.  **OpenGL Shader (GLSL):** Se o backend é OpenGL (Steam Deck/Linux), injetar um fragment shader FSR 1.0. Complexidade alta para um ganho visual marginal em comparação com Linear.
3.  **Upscale via RenderTarget:** Renderizar num FBO menor e fazer copy com filtro hardware.

**Decisão AES:**
A via `SDL_SetTextureScaleMode` é a correta porque:
- Mantém compatibilidade cross-platform (Wayland/X11/Metal).
- Não requer parsing de strings GLSL manuais (bug-prone).
- Hardware moderno (AMD/IGPU do Deck) faz hardware-accelerated bilinear instantaneamente.

**Gating:** Se o usuário pedir "Sharpen" ou "FSR", reavaliar para GLSL.

## Implementation Notes
- **API correction:** The system SDL2 is 2.30.0. `SDL_SetTextureScaleMode` takes `SDL_ScaleMode` (enum), not `SDL_TEXTURESCALEMODE_*` macros (those are SDL3). Use `SDL_ScaleModeLinear` / `SDL_ScaleModeNearest`.
- Applied at texture creation (main.c:416) and at dynamic resize (main.c:188).