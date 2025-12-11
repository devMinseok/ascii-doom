// Doom args
const commonArgs = ["-iwad","doom1.wad","-window","-nogui","-nomusic","-config","default.cfg","-servername","doomflare","-force_software_renderer","1"];

// JS/WASM 해상도 (i_ascii.h의 ASCII_WIDTH/HEIGHT와 반드시 동일)
const ASCII_WIDTH  = 240;
const ASCII_HEIGHT = 80;
const SCREENWIDTH  = 320;
const SCREENHEIGHT = 200;

function setupRenderer() {
  // WASM exports
  const getBufferPtr  = Module.cwrap('ascii_get_buffer', 'number', []);
  const getBufferSize = Module.cwrap('ascii_get_buffer_size', 'number', []);
  const setSimd       = Module.cwrap('ascii_set_simd', null, ['number']);
  const getSimd       = Module.cwrap('ascii_get_simd', 'number', []);
  const simdSupported = Module.cwrap('ascii_simd_supported', 'number', []);
  const setBenchmarkMode = Module.cwrap('ascii_set_benchmark_mode', null, ['number']);
  const getBenchmarkMode = Module.cwrap('ascii_get_benchmark_mode', 'number', []);
  const resetBenchmarkStats = Module.cwrap('ascii_reset_benchmark_stats', null, []);
  
  // 벤치마크 통계 조회 함수들 (SIMD ON)
  const getBenchFrameCountOn = Module.cwrap('ascii_get_benchmark_frame_count_simd_on', 'number', []);
  const getBenchTotalTimeOn = Module.cwrap('ascii_get_benchmark_total_time_simd_on', 'number', []);
  const getBenchMinTimeOn = Module.cwrap('ascii_get_benchmark_min_time_simd_on', 'number', []);
  const getBenchMaxTimeOn = Module.cwrap('ascii_get_benchmark_max_time_simd_on', 'number', []);
  const getBenchAvgTimeOn = Module.cwrap('ascii_get_benchmark_avg_time_simd_on', 'number', []);
  
  // 벤치마크 통계 조회 함수들 (SIMD OFF)
  const getBenchFrameCountOff = Module.cwrap('ascii_get_benchmark_frame_count_simd_off', 'number', []);
  const getBenchTotalTimeOff = Module.cwrap('ascii_get_benchmark_total_time_simd_off', 'number', []);
  const getBenchMinTimeOff = Module.cwrap('ascii_get_benchmark_min_time_simd_off', 'number', []);
  const getBenchMaxTimeOff = Module.cwrap('ascii_get_benchmark_max_time_simd_off', 'number', []);
  const getBenchAvgTimeOff = Module.cwrap('ascii_get_benchmark_avg_time_simd_off', 'number', []);
  
  // 1초 윈도우 FPS 조회 함수들
  const getCurrentFpsOn = Module.cwrap('ascii_get_current_fps_simd_on', 'number', []);
  const getCurrentFpsOff = Module.cwrap('ascii_get_current_fps_simd_off', 'number', []);

  // JS 모드 관련 WASM exports
  const setJsMode = Module.cwrap('ascii_set_js_mode', null, ['number']);
  const getJsMode = Module.cwrap('ascii_get_js_mode', 'number', []);
  const getRgbaBufferPtr = Module.cwrap('ascii_get_rgba_buffer', 'number', []);
  const getRgbaBufferSize = Module.cwrap('ascii_get_rgba_buffer_size', 'number', []);
  const getRgbaWidth = Module.cwrap('ascii_get_rgba_width', 'number', []);
  const getRgbaHeight = Module.cwrap('ascii_get_rgba_height', 'number', []);

  if (!getBufferPtr || !getBufferSize) { console.error("Failed to wrap WASM buffer functions."); return; }

  // ===== JS ASCII 변환 로직 =====
  const ASCII_CHARS = " .:-=+*#%@";
  const ASCII_CHARS_LEN = ASCII_CHARS.length;
  const GAMMA_VALUE = 0.35;

  // LUT 초기화 (한 번만)
  const gammaTable = new Uint8Array(256);
  const idxLUT = new Uint8Array(256);
  let jsLutInitialized = false;

  function initJsLuts() {
    if (jsLutInitialized) return;
    // 감마 LUT
    for (let i = 0; i < 256; i++) {
      const normalized = i / 255.0;
      const corrected = Math.pow(normalized, GAMMA_VALUE);
      let v = corrected * 255.0;
      if (v < 0) v = 0; else if (v > 255) v = 255;
      gammaTable[i] = Math.round(v);
    }
    // 밝기 → 문자 인덱스 LUT
    for (let b = 0; b < 256; b++) {
      let idx = ((b * (ASCII_CHARS_LEN - 1) + 127) >> 8);
      if (idx < 0) idx = 0;
      else if (idx >= ASCII_CHARS_LEN) idx = ASCII_CHARS_LEN - 1;
      idxLUT[b] = idx;
    }
    jsLutInitialized = true;
  }

  // 적분영상 버퍼 (재사용)
  let integralR = null, integralG = null, integralB = null;
  let integralW = 0, integralH = 0;

  // 경계/역수 LUT
  let boundsX0 = null, boundsX1 = null, boundsY0 = null, boundsY1 = null;
  let invCount = null;
  let boundsAsciiW = 0, boundsAsciiH = 0;

  function ensureIntegralCapacity(srcW, srcH) {
    const W = srcW + 1;
    const H = srcH + 1;
    if (integralW === W && integralH === H && integralR) return;
    integralR = new Uint32Array(W * H);
    integralG = new Uint32Array(W * H);
    integralB = new Uint32Array(W * H);
    integralW = W;
    integralH = H;
  }

  function ensureBounds(srcW, srcH, asciiW, asciiH) {
    if (boundsAsciiW === asciiW && boundsAsciiH === asciiH && boundsX0) return;
    boundsX0 = new Int32Array(asciiW);
    boundsX1 = new Int32Array(asciiW);
    boundsY0 = new Int32Array(asciiH);
    boundsY1 = new Int32Array(asciiH);
    invCount = new Uint32Array(asciiW * asciiH);

    for (let x = 0; x < asciiW; x++) {
      let x0 = Math.floor(x * srcW / asciiW);
      let x1 = Math.floor((x + 1) * srcW / asciiW);
      if (x1 <= x0) x1 = x0 + 1;
      if (x0 < 0) x0 = 0;
      if (x1 > srcW) x1 = srcW;
      boundsX0[x] = x0;
      boundsX1[x] = x1;
    }
    for (let y = 0; y < asciiH; y++) {
      let y0 = Math.floor(y * srcH / asciiH);
      let y1 = Math.floor((y + 1) * srcH / asciiH);
      if (y1 <= y0) y1 = y0 + 1;
      if (y0 < 0) y0 = 0;
      if (y1 > srcH) y1 = srcH;
      boundsY0[y] = y0;
      boundsY1[y] = y1;
    }

    // 역수 LUT: (1 << 16) / cnt
    for (let y = 0; y < asciiH; y++) {
      const countY = boundsY1[y] - boundsY0[y];
      for (let x = 0; x < asciiW; x++) {
        const countX = boundsX1[x] - boundsX0[x];
        const cnt = countY * countX;
        invCount[y * asciiW + x] = cnt > 0 ? Math.floor((1 << 16) / cnt) : 0;
      }
    }
    boundsAsciiW = asciiW;
    boundsAsciiH = asciiH;
  }

  // 적분영상 빌드 (RGBA32 → R/G/B 분리)
  function buildIntegralImages(rgba, srcW, srcH) {
    ensureIntegralCapacity(srcW, srcH);
    const W = integralW;

    // 첫 행/열 0 클리어
    for (let i = 0; i < W; i++) {
      integralR[i] = 0;
      integralG[i] = 0;
      integralB[i] = 0;
    }
    for (let y = 1; y <= srcH; y++) {
      integralR[y * W] = 0;
      integralG[y * W] = 0;
      integralB[y * W] = 0;
    }

    // 행+열 누적
    for (let y = 0; y < srcH; y++) {
      let rsum = 0, gsum = 0, bsum = 0;
      const srcRowOffset = y * srcW;
      const dstRowOffset = (y + 1) * W + 1;
      const prevRowOffset = y * W + 1;

      for (let x = 0; x < srcW; x++) {
        const pixel = rgba[srcRowOffset + x];
        // ARGB8888: pixel = 0xAARRGGBB
        rsum += (pixel >> 16) & 0xFF;
        gsum += (pixel >> 8) & 0xFF;
        bsum += pixel & 0xFF;
        integralR[dstRowOffset + x] = rsum + integralR[prevRowOffset + x];
        integralG[dstRowOffset + x] = gsum + integralG[prevRowOffset + x];
        integralB[dstRowOffset + x] = bsum + integralB[prevRowOffset + x];
      }
    }
  }

  // JS ASCII 변환 메인 함수
  // 반환: Uint8Array (char, r, g, b) * ASCII_WIDTH * ASCII_HEIGHT
  function convertRGBAtoASCII_JS(rgba, srcW, srcH, asciiW, asciiH) {
    initJsLuts();
    ensureBounds(srcW, srcH, asciiW, asciiH);
    buildIntegralImages(rgba, srcW, srcH);

    const totalCells = asciiW * asciiH;
    const result = new Uint8Array(totalCells * 4);
    const W = integralW;

    for (let y = 0; y < asciiH; y++) {
      const y0 = boundsY0[y], y1 = boundsY1[y];
      const rowOffset = y * asciiW;

      for (let x = 0; x < asciiW; x++) {
        const x0 = boundsX0[x], x1 = boundsX1[x];
        const inv = invCount[rowOffset + x];

        // 적분영상에서 영역 합 조회
        const rsum = integralR[y1 * W + x1] - integralR[y0 * W + x1] - integralR[y1 * W + x0] + integralR[y0 * W + x0];
        const gsum = integralG[y1 * W + x1] - integralG[y0 * W + x1] - integralG[y1 * W + x0] + integralG[y0 * W + x0];
        const bsum = integralB[y1 * W + x1] - integralB[y0 * W + x1] - integralB[y1 * W + x0] + integralB[y0 * W + x0];

        // 평균 (곱셈+시프트)
        let rv = (rsum * inv) >>> 16;
        let gv = (gsum * inv) >>> 16;
        let bv = (bsum * inv) >>> 16;

        // clamp
        if (rv > 255) rv = 255;
        if (gv > 255) gv = 255;
        if (bv > 255) bv = 255;

        // 밝기 계산
        let lum = ((rv * 299 + gv * 587 + bv * 114) >> 10);
        if (lum > 255) lum = 255;
        if (lum < 0) lum = 0;

        // 결과 저장
        const idx = (rowOffset + x) * 4;
        result[idx] = ASCII_CHARS.charCodeAt(idxLUT[lum]);
        result[idx + 1] = gammaTable[rv];
        result[idx + 2] = gammaTable[gv];
        result[idx + 3] = gammaTable[bv];
      }
    }

    return result;
  }

  // JS 벤치마크 통계
  const BENCHMARK_WARMUP_FRAMES = 3;
  let jsBenchStats = {
    frameCount: 0,
    warmupCount: 0,
    totalTimeMs: 0.0,
    minTimeMs: 1e9,
    maxTimeMs: 0.0,
    avgTimeMs: 0.0,
    fpsWindowStart: 0.0,
    fpsFrameCount: 0,
    currentFps: 0.0
  };

  function resetJsBenchmarkStats() {
    jsBenchStats = {
      frameCount: 0,
      warmupCount: 0,
      totalTimeMs: 0.0,
      minTimeMs: 1e9,
      maxTimeMs: 0.0,
      avgTimeMs: 0.0,
      fpsWindowStart: 0.0,
      fpsFrameCount: 0,
      currentFps: 0.0
    };
  }

  function updateJsBenchmarkStats(elapsedMs) {
    jsBenchStats.frameCount++;
    
    if (jsBenchStats.warmupCount < BENCHMARK_WARMUP_FRAMES) {
      jsBenchStats.warmupCount++;
    } else {
      jsBenchStats.totalTimeMs += elapsedMs;
      const measuredFrames = jsBenchStats.frameCount - BENCHMARK_WARMUP_FRAMES;
      if (measuredFrames > 0) {
        if (elapsedMs < jsBenchStats.minTimeMs) jsBenchStats.minTimeMs = elapsedMs;
        if (elapsedMs > jsBenchStats.maxTimeMs) jsBenchStats.maxTimeMs = elapsedMs;
        jsBenchStats.avgTimeMs = jsBenchStats.totalTimeMs / measuredFrames;
      }
    }

    // 1초 윈도우 FPS 계산
    const now = performance.now();
    jsBenchStats.fpsFrameCount++;
    if (jsBenchStats.fpsWindowStart === 0.0) {
      jsBenchStats.fpsWindowStart = now;
    } else {
      const windowElapsed = now - jsBenchStats.fpsWindowStart;
      if (windowElapsed >= 1000.0) {
        jsBenchStats.currentFps = (jsBenchStats.fpsFrameCount * 1000.0) / windowElapsed;
        jsBenchStats.fpsFrameCount = 0;
        jsBenchStats.fpsWindowStart = now;
      }
    }
  }

  // 엔진 모드: 'cpp' | 'js'
  let engineMode = 'cpp';

  const canvas = document.getElementById('canvas');
  const ctx = canvas.getContext('2d', { alpha: false });

  // 폰트/간격 계산에 쓰는 상태
  const screenWrapper = document.querySelector('.screen-wrapper');
  let DPR, CW, CH, lineHeight, fontSize, xAdvance, yAdvance, FONT_FAMILY;

  // 폰트/캔버스 크기/advance 재계산
  function recalc() {
    const rect = screenWrapper.getBoundingClientRect();
    DPR = window.devicePixelRatio || 1;
    CW = Math.round(rect.width  * DPR);
    CH = Math.round(rect.height * DPR);
    canvas.width = CW;
    canvas.height = CH;

    // 폰트 설정
    FONT_FAMILY = `"VT323", "Courier New", monospace`;
    ctx.textBaseline = 'top';
    ctx.textAlign = 'left';

    // 목표 셀 크기
    const targetCellW = Math.floor(CW / ASCII_WIDTH);
    lineHeight = Math.floor(CH / ASCII_HEIGHT); // 세로 기준
    fontSize = lineHeight;
    ctx.font = `${fontSize}px ${FONT_FAMILY}`;

    // 실제 글자 폭 측정
    const measureAdvance = (px) => {
      ctx.font = `${px}px ${FONT_FAMILY}`;
      return ctx.measureText('M').width;
    };
    let adv = measureAdvance(fontSize);

    // 폭이 셀보다 크면 폰트 크기 줄이기
    if (adv > targetCellW) {
      const scale = targetCellW / adv;
      fontSize = Math.max(8, Math.floor(fontSize * scale));
      adv = measureAdvance(fontSize);
    }

    // 최종 확정
    ctx.font = `${fontSize}px ${FONT_FAMILY}`;
    xAdvance = Math.ceil(adv);    // 가독성 위해 올림
    yAdvance = lineHeight;
  }

  // 최초 계산 + 리사이즈 대응
  recalc();
  window.addEventListener('resize', () => { recalc(); });

  function getMemoryBuffer() {
    if (typeof HEAPU8 !== 'undefined' && HEAPU8 && HEAPU8.buffer) return HEAPU8.buffer;
    if (Module.HEAPU8 && Module.HEAPU8.buffer) return Module.HEAPU8.buffer;
    if (Module.HEAP8 && Module.HEAP8.buffer) return Module.HEAP8.buffer;
    if (Module.wasmMemory && Module.wasmMemory.buffer) return Module.wasmMemory.buffer;
    return null;
  }

  if (!getMemoryBuffer()) { console.error("Cannot access WASM memory buffer."); return; }

  // 벤치마크 모드 상태
  let benchmarkMode = false;
  let benchmarkFrameCounter = 0;
  const BENCHMARK_SWITCH_INTERVAL = 300; // 각 모드당 300프레임 측정

  // 벤치마크 UI 요소
  const benchmarkPanel = document.getElementById('benchmark-panel');
  const benchmarkToggle = document.getElementById('benchmark-toggle');
  const currentModeEl = document.getElementById('benchmark-current-mode');
  const benchmarkCppStats = document.getElementById('benchmark-cpp-stats');
  const benchmarkJsStats = document.getElementById('benchmark-js-stats');

  // 벤치마크 통계 업데이트 함수
  function updateBenchmarkStats() {
    if (!benchmarkMode) return;

    if (engineMode === 'cpp') {
      // C++ 모드: SIMD ON/OFF 통계
      const statsOn = {
        minTime: getBenchMinTimeOn(),
        maxTime: getBenchMaxTimeOn(),
        avgTime: getBenchAvgTimeOn(),
        currentFps: getCurrentFpsOn()
      };

      const statsOff = {
        minTime: getBenchMinTimeOff(),
        maxTime: getBenchMaxTimeOff(),
        avgTime: getBenchAvgTimeOff(),
        currentFps: getCurrentFpsOff()
      };

      // UI 업데이트 - SIMD ON
      const minTimeOn = statsOn.minTime < 1e8 ? statsOn.minTime : 0.0;
      document.getElementById('bench-simd-on-fps-current').textContent = statsOn.currentFps.toFixed(2);
      document.getElementById('bench-simd-on-latency-avg').textContent = statsOn.avgTime.toFixed(2) + ' ms';
      document.getElementById('bench-simd-on-latency-min').textContent = minTimeOn.toFixed(2) + ' ms';
      document.getElementById('bench-simd-on-latency-max').textContent = statsOn.maxTime.toFixed(2) + ' ms';

      // UI 업데이트 - SIMD OFF
      const minTimeOff = statsOff.minTime < 1e8 ? statsOff.minTime : 0.0;
      document.getElementById('bench-simd-off-fps-current').textContent = statsOff.currentFps.toFixed(2);
      document.getElementById('bench-simd-off-latency-avg').textContent = statsOff.avgTime.toFixed(2) + ' ms';
      document.getElementById('bench-simd-off-latency-min').textContent = minTimeOff.toFixed(2) + ' ms';
      document.getElementById('bench-simd-off-latency-max').textContent = statsOff.maxTime.toFixed(2) + ' ms';
    } else {
      // JS 모드: JS 통계
      const minTimeJs = jsBenchStats.minTimeMs < 1e8 ? jsBenchStats.minTimeMs : 0.0;
      document.getElementById('bench-js-fps-current').textContent = jsBenchStats.currentFps.toFixed(2);
      document.getElementById('bench-js-latency-avg').textContent = jsBenchStats.avgTimeMs.toFixed(2) + ' ms';
      document.getElementById('bench-js-latency-min').textContent = minTimeJs.toFixed(2) + ' ms';
      document.getElementById('bench-js-latency-max').textContent = jsBenchStats.maxTimeMs.toFixed(2) + ' ms';
    }
  }

  // 벤치마크 UI 모드 전환
  function updateBenchmarkUI() {
    if (engineMode === 'cpp') {
      benchmarkCppStats.style.display = 'block';
      benchmarkJsStats.style.display = 'none';
    } else {
      benchmarkCppStats.style.display = 'none';
      benchmarkJsStats.style.display = 'block';
    }
  }

  // 벤치마크 토글
  benchmarkToggle.addEventListener('click', () => {
    benchmarkMode = !benchmarkMode;
    
    if (engineMode === 'cpp') {
      setBenchmarkMode(benchmarkMode ? 1 : 0);
    }
    
    if (benchmarkMode) {
      if (engineMode === 'cpp') {
        resetBenchmarkStats();
        setSimd(1);
      } else {
        resetJsBenchmarkStats();
      }
      benchmarkPanel.classList.add('active');
      benchmarkToggle.textContent = 'BENCHMARK: ON';
      benchmarkToggle.style.color = '#0f0';
      benchmarkToggle.style.borderColor = '#0f0';
      benchmarkFrameCounter = 0;
      updateBenchmarkUI();
      if (engineMode === 'cpp') {
        currentModeEl.textContent = 'Testing: SIMD ON';
        currentModeEl.style.color = '#0f0';
      } else {
        currentModeEl.textContent = 'Testing: JavaScript';
        currentModeEl.style.color = '#ff0';
      }
    } else {
      benchmarkPanel.classList.remove('active');
      benchmarkToggle.textContent = 'BENCHMARK: OFF';
      benchmarkToggle.style.color = '#ff3333';
      benchmarkToggle.style.borderColor = '#ff3333';
    }
  });

  // 엔진 토글 (C++ / JS)
  const engineToggle = document.getElementById('engine-toggle');
  const simdToggle = document.getElementById('simd-toggle');

  engineToggle.addEventListener('click', () => {
    if (benchmarkMode) return; // 벤치마크 모드에서는 전환 불가
    
    if (engineMode === 'cpp') {
      engineMode = 'js';
      setJsMode(1);
      engineToggle.textContent = 'ENGINE: JS';
      engineToggle.style.color = '#ff0';
      engineToggle.style.borderColor = '#ff0';
      // JS 모드에서는 SIMD 토글 비활성화
      simdToggle.disabled = true;
      simdToggle.style.color = '#666';
      simdToggle.style.borderColor = '#666';
    } else {
      engineMode = 'cpp';
      setJsMode(0);
      engineToggle.textContent = 'ENGINE: C++';
      engineToggle.style.color = '#0f0';
      engineToggle.style.borderColor = '#0f0';
      // C++ 모드에서는 SIMD 토글 활성화
      if (simdSupported() === 1) {
        simdToggle.disabled = false;
        const simdState = getSimd();
        simdToggle.style.color = simdState ? '#0f0' : '#f00';
        simdToggle.style.borderColor = simdState ? '#0f0' : '#f00';
      }
    }
  });

  // SIMD 토글 (C++ 모드이고 벤치마크 모드가 아닐 때만 수동 제어)
  if (simdSupported() === 1) {
    simdToggle.addEventListener('click', () => {
      if (benchmarkMode || engineMode === 'js') return;
      const current = getSimd();
      setSimd(current ? 0 : 1);
      const newState = getSimd();
      simdToggle.textContent = `SIMD: ${newState ? 'ON' : 'OFF'}`;
      simdToggle.style.color = newState ? '#0f0' : '#f00';
      simdToggle.style.borderColor = newState ? '#0f0' : '#f00';
    });
  } else {
    simdToggle.textContent = 'SIMD: N/A';
    simdToggle.style.color = '#666';
    simdToggle.disabled = true;
  }

  // 렌더 루프
  function renderFrame() {
    if (engineMode === 'cpp') {
      // C++ 모드
      if (benchmarkMode) {
        // 벤치마크 모드일 때 SIMD ON/OFF 전환
        benchmarkFrameCounter++;
        if (benchmarkFrameCounter >= BENCHMARK_SWITCH_INTERVAL) {
          benchmarkFrameCounter = 0;
          const current = getSimd();
          setSimd(current ? 0 : 1);
          const newState = getSimd();
          currentModeEl.textContent = `Testing: SIMD ${newState ? 'ON' : 'OFF'}`;
          currentModeEl.style.color = newState ? '#0f0' : '#ff3333';
        }
        updateBenchmarkStats();
      } else {
        // 일반 모드: canvas 그리기
        try {
          const ptr = getBufferPtr();
          const size = getBufferSize();
          if (!ptr || !size) return;

          const heap = getMemoryBuffer();
          const buffer = new Uint8Array(heap, ptr, size);

          const expected = ASCII_WIDTH * ASCII_HEIGHT * 4;
          if (buffer.length < expected) return;

          ctx.fillStyle = '#000';
          ctx.fillRect(0, 0, canvas.width, canvas.height);

          let i = 0;
          for (let y = 0; y < ASCII_HEIGHT; y++) {
            const yPos = y * yAdvance;
            for (let x = 0; x < ASCII_WIDTH; x++) {
              const charCode = buffer[i];
              const r = buffer[i + 1];
              const g = buffer[i + 2];
              const b = buffer[i + 3];
              i += 4;

              if (charCode > 32 || r > 10 || g > 10 || b > 10) {
                ctx.fillStyle = `rgb(${r},${g},${b})`;
                ctx.fillText(String.fromCharCode(charCode), x * xAdvance, yPos);
              }
            }
          }
        } catch (e) {
          console.error("Error during canvas rendering:", e);
        }
      }
    } else {
      // JS 모드
      try {
        const rgbaPtr = getRgbaBufferPtr();
        if (!rgbaPtr) return;

        const heap = getMemoryBuffer();
        const srcW = getRgbaWidth();
        const srcH = getRgbaHeight();
        if (!srcW || !srcH) return;

        const rgbaSize = srcW * srcH;
        const rgbaBuffer = new Uint32Array(heap, rgbaPtr, rgbaSize);

        // 벤치마크 모드면 시간 측정
        let startTime = 0;
        if (benchmarkMode) {
          startTime = performance.now();
        }

        // JS로 ASCII 변환
        const buffer = convertRGBAtoASCII_JS(rgbaBuffer, srcW, srcH, ASCII_WIDTH, ASCII_HEIGHT);

        // 벤치마크 통계 업데이트
        if (benchmarkMode) {
          const elapsed = performance.now() - startTime;
          updateJsBenchmarkStats(elapsed);
          updateBenchmarkStats();
        }

        // Canvas 렌더링
        ctx.fillStyle = '#000';
        ctx.fillRect(0, 0, canvas.width, canvas.height);

        let i = 0;
        for (let y = 0; y < ASCII_HEIGHT; y++) {
          const yPos = y * yAdvance;
          for (let x = 0; x < ASCII_WIDTH; x++) {
            const charCode = buffer[i];
            const r = buffer[i + 1];
            const g = buffer[i + 2];
            const b = buffer[i + 3];
            i += 4;

            if (charCode > 32 || r > 10 || g > 10 || b > 10) {
              ctx.fillStyle = `rgb(${r},${g},${b})`;
              ctx.fillText(String.fromCharCode(charCode), x * xAdvance, yPos);
            }
          }
        }
      } catch (e) {
        console.error("Error during JS canvas rendering:", e);
      }
    }
  }

  function loop() {
    renderFrame();
    requestAnimationFrame(loop);
  }

  // 시작
  callMain(commonArgs);
  requestAnimationFrame(loop);
}
