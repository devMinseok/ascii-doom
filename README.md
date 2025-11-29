# Chocolate Doom (Web/WASM Port)

Chocolate Doom compiled to WebAssembly for running in web browsers.

## Prerequisites

**Docker Desktop만 설치하면 됩니다** (모든 플랫폼: macOS, Windows, Linux)

- [Docker Desktop 다운로드](https://www.docker.com/products/docker-desktop)
- Docker Compose는 Docker Desktop에 포함되어 있음

## Build

### Dev Container 사용 (권장)

**VS Code 또는 Cursor에서:**

1. 프로젝트 열기
2. 명령 팔레트 (`Cmd+Shift+P` / `Ctrl+Shift+P`)
3. **"Dev Containers: Reopen in Container"** 선택
4. 컨테이너 빌드 완료 후 자동 연결

컨테이너 내부 터미널에서:

```bash
# 빌드 스크립트 실행
/usr/local/bin/build.sh
```

빌드된 파일은 `src/` 디렉토리에 생성됩니다:
- `chocolate-doom.html`, `chocolate-heretic.html`, `chocolate-hexen.html`, `chocolate-strife.html`
- `chocolate-setup.html`, `chocolate-server.html`
- 각각의 `.wasm` 및 `.js` 파일

### Docker Compose 사용 (대안)

Dev Container를 사용할 수 없는 경우:

```bash
# 이미지 빌드 (최초 1회)
docker-compose build

# 빌드 실행
docker-compose run --rm build /usr/local/bin/build.sh
```

## Run

```bash
cd src
python3 -m http.server 8000
```

브라우저에서 `http://localhost:8000/chocolate-doom.html` 열기

**Note**: 게임을 실행하려면 WAD 파일(`doom1.wad`, `doom2.wad` 등)이 필요합니다. `index.html`의 `preRun` 섹션을 수정하여 WAD 파일을 미리 로드하세요.

## Deploy to GitHub Pages

GitHub Actions가 자동으로 빌드하고 배포합니다.

### Setup

1. Repository Settings → Pages
2. Source: **"GitHub Actions"** 선택
3. `main` 브랜치에 push하면 자동 배포

사이트는 `https://<username>.github.io/ascii-doom/`에서 확인할 수 있습니다.

## Troubleshooting

- **컨테이너 연결 실패**: Docker Desktop이 실행 중인지 확인
- **빌드 실패**: `emmake make clean && emmake make -j4 -k` 재시도
- **아이콘 파일 에러**: 무시해도 됨 (게임 빌드에는 영향 없음)

## 조작키
이동 : 방향키
공격 : a
문 열기 : s