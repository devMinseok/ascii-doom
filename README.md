# Chocolate Doom (Web/WASM Port)

Chocolate Doom compiled to WebAssembly for running in web browsers.

## Prerequisites

### Docker (모든 플랫폼 - 권장)

**Docker만 설치하면 모든 플랫폼(macOS, Windows, Linux)에서 동일하게 빌드할 수 있습니다.**

- **Docker Desktop**: [docker.com](https://www.docker.com/products/docker-desktop)에서 다운로드 및 설치
- **Docker Compose**: Docker Desktop에 포함되어 있음

### macOS

- **Emscripten SDK**: `brew install emscripten` or install from [emscripten.org](https://emscripten.org)
- **Autotools**: `brew install autoconf automake libtool`
- **pkg-config**: `brew install pkgconf`
- **Python 3**: Usually pre-installed, or `brew install python3`

### Windows

Windows에서는 다음 두 가지 방법 중 하나를 선택할 수 있습니다:

#### 방법 1: MSYS2 사용 (권장)

1. **MSYS2 설치**: [msys2.org](https://www.msys2.org/)에서 다운로드 및 설치
2. **MSYS2 터미널 실행** 후 다음 명령어 실행:
   ```bash
   # 패키지 업데이트
   pacman -Syu
   
   # 필수 도구 설치
   pacman -S base-devel autoconf automake libtool pkgconf python
   
   # Emscripten SDK 설치
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk
   ./emsdk install latest
   ./emsdk activate latest
   source ./emsdk_env.sh
   ```

3. **MSYS2 터미널에서 프로젝트 빌드** (이 터미널에서 모든 빌드 명령어 실행)

#### 방법 2: WSL (Windows Subsystem for Linux)

1. **WSL 설치**: PowerShell에서 `wsl --install` 실행
2. **WSL 터미널 실행** 후:
   ```bash
   # Ubuntu/Debian 기반
   sudo apt-get update
   sudo apt-get install -y autoconf automake libtool pkg-config python3 python3-pip
   
   # Emscripten SDK 설치
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk
   ./emsdk install latest
   ./emsdk activate latest
   source ./emsdk_env.sh
   ```

3. **WSL 터미널에서 프로젝트 빌드**

### Linux

- **Autotools**: `sudo apt-get install autoconf automake libtool pkg-config` (Ubuntu/Debian) or `sudo yum install autoconf automake libtool pkgconfig` (RHEL/CentOS)
- **Emscripten SDK**: [emscripten.org](https://emscripten.org)에서 설치
- **Python 3**: Usually pre-installed

## Build

### Dev Container 사용 (가장 권장 - VS Code/Cursor)

**Cursor나 VS Code에서 컨테이너 내부에서 직접 개발할 수 있습니다:**

1. **Cursor/VS Code에서 프로젝트 열기**
2. **명령 팔레트** (`Cmd+Shift+P` / `Ctrl+Shift+P`) 열기
3. **"Dev Containers: Reopen in Container"** 선택
4. 컨테이너가 빌드되고 자동으로 연결됩니다

컨테이너 내부에서:
- 터미널이 자동으로 컨테이너 환경에서 실행됩니다
- Emscripten이 자동으로 설정됩니다
- 바로 빌드할 수 있습니다:

```bash
# 빌드 스크립트 사용
/usr/local/bin/build.sh

# 또는 수동으로
source /opt/emsdk/emsdk_env.sh
autoreconf -fiv
emconfigure ./configure --enable-emscripten
emmake make -j4 -k
```

### Docker Compose 사용

모든 플랫폼에서 동일하게 작동합니다:

```bash
# Docker 이미지 빌드 (최초 1회만, 시간이 걸릴 수 있음)
docker-compose build

# 컨테이너 내에서 빌드 실행
docker-compose run --rm build /usr/local/bin/build.sh

# 또는 컨테이너에 접속해서 수동으로 빌드
docker-compose run --rm build bash
# 컨테이너 내에서:
#   source /opt/emsdk/emsdk_env.sh
#   autoreconf -fiv
#   emconfigure ./configure --enable-emscripten
#   emmake make -j4 -k
```

빌드된 파일은 호스트의 `src/` 디렉토리에 생성됩니다.

### 로컬 빌드

Docker 없이 로컬에서 빌드하려면:

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

## Deploy to GitHub Pages

This project includes a GitHub Actions workflow that automatically builds and deploys to GitHub Pages.

### Setup

1. **Enable GitHub Pages** in your repository settings:
   - Go to Settings → Pages
   - Source: Select "GitHub Actions"

2. **Push to main/master branch** - The workflow will automatically:
   - Build the project with Emscripten
   - Deploy to GitHub Pages
   - Your site will be available at `https://<username>.github.io/ascii-doom/`

### Manual Deployment

If you want to deploy manually:

```bash
# Build the project
autoreconf -fiv
emconfigure ./configure --enable-emscripten
emmake make -j4 -k

# Copy files to docs/ directory (if using docs/ folder for Pages)
# Or push src/ directory contents to gh-pages branch
```

## Troubleshooting

- **Build fails with "No rule to make target"**: Run `autoreconf -fiv` first
- **Icon file errors** (`doom.png`, `hexen.png`, etc.): These are optional - ignore them. Use `-k` flag to continue building despite errors
- **wasm-ld error**: `emmake make clean && emmake make -k`
