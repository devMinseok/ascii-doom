# Chocolate Doom (Web/WASM Port)

Chocolate Doom compiled to WebAssembly for running in web browsers.

## Prerequisites

- **Emscripten SDK**: `brew install emscripten` (macOS) or install from [emscripten.org](https://emscripten.org)
- **Autotools**: `brew install autoconf automake libtool` (macOS)
- **Python 3**

## Build

```bash
# Generate configure script
autoreconf -fiv

# Configure with Emscripten
emconfigure ./configure --enable-emscripten

# Build (ignore icon file errors - games will build successfully)
emmake make -j4 -k
```

**Note**: You may see errors about missing icon files (`doom.png`, `hexen.png`, etc.) in the `data/` directory. These are optional and can be ignored - the game files will build successfully.

Output files in `src/`:
- `chocolate-doom.html`, `chocolate-heretic.html`, `chocolate-hexen.html`, `chocolate-strife.html`
- `chocolate-setup.html`, `chocolate-server.html`
- Corresponding `.wasm` and `.js` files

## Run

```bash
cd src
python3 -m http.server 8000
```

Open `http://localhost:8000/chocolate-doom.html` in your browser.

**Note**: You need a WAD file (e.g., `doom1.wad`, `doom2.wad`) to play. Modify `index.html`'s `preRun` section to preload your WAD file.

## Troubleshooting

- **Build fails with "No rule to make target"**: Run `autoreconf -fiv` first
- **Icon file errors** (`doom.png`, `hexen.png`, etc.): These are optional - ignore them. Use `-k` flag to continue building despite errors
- **wasm-ld error**: `emmake make clean && emmake make -k`
