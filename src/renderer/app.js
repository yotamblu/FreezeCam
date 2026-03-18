(function () {
  'use strict';

  function log(msg, ...args) {
    console.log(`[FreezeCam][Renderer] ${msg}`, ...args);
  }
  function logErr(msg, ...args) {
    console.error(`[FreezeCam][Renderer][ERROR] ${msg}`, ...args);
  }
  function logWarn(msg, ...args) {
    console.warn(`[FreezeCam][Renderer][WARN] ${msg}`, ...args);
  }

  // ─── i18n ───
  const i18n = {
    he: {
      selectCamera: 'בחר מצלמה כדי להתחיל',
      frozen: 'מוקפא',
      camera: 'מצלמה',
      selectCameraDropdown: 'בחר מצלמה...',
      bgVideo: 'סרטון רקע',
      chooseFile: 'בחר קובץ',
      hotkey: 'קיצור מקשים',
      clickToChange: 'לחץ לשינוי',
      pressAnyKey: 'לחץ על מקש כלשהו',
      freeze: 'הקפאה',
      on: 'פעיל',
      off: 'כבוי',
      initializing: 'מאתחל...',
      loadingModel: 'טוען מודל סגמנטציה...',
      selectCameraStatus: 'בחר מצלמה כדי להתחיל',
      cameraActive: 'מצלמה פעילה',
      cameraFrozen: 'מצלמה מוקפאת',
      cameraAccessDenied: 'גישה למצלמה נדחתה',
      cameraError: 'שגיאת מצלמה: ',
      infoTitle: 'איך להשתמש',
      infoHtml: `<ol>
        <li><strong>בחר מצלמה</strong> — בחר את מצלמת הרשת שלך מהתפריט הנפתח.</li>
        <li><strong>בחר סרטון רקע</strong> (אופציונלי) — בחר קובץ וידאו שישמש כרקע וירטואלי. הסגמנטציה תפריד אותך מהרקע אוטומטית.</li>
        <li><strong>פתח את Zoom</strong> — בהגדרות הווידאו, בחר <strong>"FreezeCam Pro Virtual Camera"</strong> כמצלמה.</li>
        <li><strong>הקפא!</strong> — לחץ על <strong>Ctrl+Shift+F</strong> (או קיצור המקשים שהגדרת) כדי להקפיא את התמונה. הרקע ימשיך לנוע כרגיל בזמן שאתה מוקפא.</li>
        <li><strong>שחרר הקפאה</strong> — לחץ שוב על אותו קיצור מקשים כדי לחזור לשידור חי.</li>
      </ol>
      <div class="info-tip">💡 טיפ: ניתן לשנות את קיצור המקשים בכל עת על ידי לחיצה על כפתור קיצור המקשים והקשה על שילוב חדש.</div>`,
    },
    en: {
      selectCamera: 'Select a camera to start',
      frozen: 'FROZEN',
      camera: 'Camera',
      selectCameraDropdown: 'Select camera...',
      bgVideo: 'Background Video',
      chooseFile: 'Choose File',
      hotkey: 'Hotkey',
      clickToChange: 'Click to change',
      pressAnyKey: 'Press any key',
      freeze: 'Freeze',
      on: 'ON',
      off: 'OFF',
      initializing: 'Initializing...',
      loadingModel: 'Loading segmentation model...',
      selectCameraStatus: 'Select a camera to begin',
      cameraActive: 'Camera active',
      cameraFrozen: 'Camera frozen',
      cameraAccessDenied: 'Camera access denied',
      cameraError: 'Camera error: ',
      infoTitle: 'How to Use',
      infoHtml: `<ol>
        <li><strong>Select a camera</strong> — pick your webcam from the dropdown.</li>
        <li><strong>Choose a background video</strong> (optional) — select a video file to use as a virtual background. Segmentation will separate you from the background automatically.</li>
        <li><strong>Open Zoom</strong> — in video settings, select <strong>"FreezeCam Pro Virtual Camera"</strong> as your camera.</li>
        <li><strong>Freeze!</strong> — press <strong>Ctrl+Shift+F</strong> (or your custom hotkey) to freeze your image. The background keeps playing naturally while you're frozen.</li>
        <li><strong>Unfreeze</strong> — press the same hotkey again to return to live video.</li>
      </ol>
      <div class="info-tip">💡 Tip: You can change the hotkey at any time by clicking the hotkey button and pressing a new key combination.</div>`,
    },
  };

  let currentLang = 'he';

  function t(key) {
    return (i18n[currentLang] && i18n[currentLang][key]) || (i18n.en[key]) || key;
  }

  function applyLanguage(lang) {
    currentLang = lang;
    const isRTL = lang === 'he';
    document.documentElement.lang = lang;
    document.documentElement.dir = isRTL ? 'rtl' : 'ltr';

    document.querySelectorAll('[data-i18n]').forEach(el => {
      const key = el.getAttribute('data-i18n');
      if (i18n[lang] && i18n[lang][key]) {
        el.textContent = i18n[lang][key];
      }
    });

    const langBtn = document.getElementById('lang-label');
    if (langBtn) langBtn.textContent = lang === 'he' ? 'EN' : 'עב';

    // Update dynamic text that isn't in data-i18n attributes
    if (!isFrozen) {
      freezeLabel.textContent = t('off');
    } else {
      freezeLabel.textContent = t('on');
    }
  }

  // ─── DOM Elements ───
  const webcamVideo = document.getElementById('webcam-video');
  const bgVideo = document.getElementById('bg-video');
  const outputCanvas = document.getElementById('output-canvas');
  const previewOverlay = document.getElementById('preview-overlay');
  const freezeBadge = document.getElementById('freeze-badge');
  const cameraSelect = document.getElementById('camera-select');
  const btnBgVideo = document.getElementById('btn-bg-video');
  const bgVideoLabel = document.getElementById('bg-video-label');
  const btnBgRemove = document.getElementById('btn-bg-remove');
  const btnHotkey = document.getElementById('btn-hotkey');
  const hotkeyLabel = document.getElementById('hotkey-label');
  const hotkeyHint = document.getElementById('hotkey-hint');
  const btnFreeze = document.getElementById('btn-freeze');
  const freezeLabel = document.getElementById('freeze-label');
  const statusDot = document.getElementById('status-dot');
  const statusText = document.getElementById('status-text');

  const ctx = outputCanvas.getContext('2d', { willReadFrequently: true });

  // ─── State ───
  let isFrozen = false;
  let isListeningForHotkey = false;
  let currentStream = null;
  let selfieSegmentation = null;
  let animFrameId = null;
  let frozenPersonData = null;
  let hasBgVideo = false;
  let segmentationReady = false;

  const tempCanvas = document.createElement('canvas');
  const tempCtx = tempCanvas.getContext('2d', { willReadFrequently: true });
  const maskCanvas = document.createElement('canvas');
  const maskCtx = maskCanvas.getContext('2d', { willReadFrequently: true });
  const personCanvas = document.createElement('canvas');
  const personCtx = personCanvas.getContext('2d', { willReadFrequently: true });

  // ─── Init ───
  async function init() {
    log('=== FreezeCam Pro Renderer Init ===');
    setStatus(t('loadingModel'), 'inactive');

    log('Step 1: Populating cameras...');
    await populateCameras();

    log('Step 2: Loading hotkey...');
    await loadHotkey();

    log('Step 3: Initializing segmentation...');
    await initSegmentation();

    log('Step 4: Setting up event listeners...');
    setupEventListeners();

    log('Step 5: Initializing native addon...');
    const nativeOk = await window.freezecam.initNative();
    log('Native addon init result:', nativeOk);

    setStatus(t('selectCameraStatus'), 'inactive');
    log('=== Init Complete ===');
  }

  // ─── Camera Enumeration ───
  async function populateCameras() {
    try {
      log('Requesting temp camera permission for enumeration...');
      const tempStream = await navigator.mediaDevices.getUserMedia({ video: true });
      tempStream.getTracks().forEach(t => t.stop());
      log('Got temp permission OK');

      const devices = await navigator.mediaDevices.enumerateDevices();
      const videoDevices = devices.filter(d => d.kind === 'videoinput');
      log('Found video devices:', videoDevices.length);
      videoDevices.forEach((d, i) => log(`  Camera[${i}]: "${d.label}" id=${d.deviceId.substring(0, 16)}...`));

      cameraSelect.innerHTML = `<option value="" data-i18n="selectCameraDropdown">${t('selectCameraDropdown')}</option>`;
      videoDevices.forEach(device => {
        if (device.label.includes('FreezeCam')) {
          log('  Skipping our own virtual camera:', device.label);
          return;
        }
        const opt = document.createElement('option');
        opt.value = device.deviceId;
        opt.textContent = device.label || `Camera ${cameraSelect.options.length}`;
        cameraSelect.appendChild(opt);
      });
      log('Camera dropdown populated with', cameraSelect.options.length - 1, 'cameras');
    } catch (err) {
      logErr('Camera enumeration FAILED:', err.message);
      logErr('Stack:', err.stack);
      setStatus(t('cameraAccessDenied'), 'error');
    }
  }

  // ─── Camera Start ───
  async function startCamera(deviceId) {
    log('startCamera called, deviceId:', deviceId ? deviceId.substring(0, 16) + '...' : 'none');
    if (currentStream) {
      currentStream.getTracks().forEach(t => t.stop());
      currentStream = null;
      log('Stopped previous stream');
    }
    if (!deviceId) {
      previewOverlay.classList.remove('hidden');
      setStatus(t('selectCameraStatus'), 'inactive');
      return;
    }

    try {
      log('getUserMedia with constraints...');
      const stream = await navigator.mediaDevices.getUserMedia({
        video: {
          deviceId: { exact: deviceId },
          width: { ideal: 1280 },
          height: { ideal: 720 },
          frameRate: { ideal: 30 },
        },
        audio: false,
      });
      log('getUserMedia OK');

      currentStream = stream;
      webcamVideo.srcObject = stream;
      await webcamVideo.play();
      log('Webcam video playing');

      const track = stream.getVideoTracks()[0];
      const settings = track.getSettings();
      const w = settings.width || 1280;
      const h = settings.height || 720;
      log('Camera resolution:', w, 'x', h, 'fps:', settings.frameRate);

      outputCanvas.width = w;
      outputCanvas.height = h;
      tempCanvas.width = w;
      tempCanvas.height = h;
      maskCanvas.width = w;
      maskCanvas.height = h;
      personCanvas.width = w;
      personCanvas.height = h;
      log('Canvases sized to', w, 'x', h);

      previewOverlay.classList.add('hidden');
      setStatus(t('cameraActive'), 'active');
      startProcessingLoop();
    } catch (err) {
      logErr('Camera start FAILED:', err.message);
      logErr('Stack:', err.stack);
      setStatus(t('cameraError') + err.message, 'error');
    }
  }

  // ─── Segmentation ───
  async function initSegmentation() {
    log('Checking for SelfieSegmentation global...');
    log('typeof SelfieSegmentation:', typeof SelfieSegmentation);

    /* global SelfieSegmentation */
    if (typeof SelfieSegmentation === 'undefined') {
      logWarn('SelfieSegmentation is UNDEFINED - CDN script failed to load');
      logWarn('Check CSP and network connectivity');
      return;
    }

    try {
      log('Creating SelfieSegmentation instance...');
      selfieSegmentation = new SelfieSegmentation({
        locateFile: (file) => {
          const url = `../../node_modules/@mediapipe/selfie_segmentation/${file}`;
          log('MediaPipe locateFile:', file, '->', url);
          return url;
        },
      });
      log('SelfieSegmentation instance created');

      selfieSegmentation.setOptions({
        modelSelection: 1,
        selfieMode: false,
      });
      log('Segmentation options set');

      selfieSegmentation.onResults(onSegmentationResults);
      log('onResults callback set');

      log('Warming up segmentation model (loading WASM + model)...');
      const warmup = document.createElement('canvas');
      warmup.width = 2;
      warmup.height = 2;
      const warmupCtx = warmup.getContext('2d');
      warmupCtx.fillStyle = 'black';
      warmupCtx.fillRect(0, 0, 2, 2);
      await selfieSegmentation.send({ image: warmup });

      segmentationReady = true;
      log('Segmentation READY');
    } catch (err) {
      logErr('Segmentation init FAILED:', err.message);
      logErr('Stack:', err.stack);
      selfieSegmentation = null;
    }
  }

  let latestSegMask = null;
  let segResultCount = 0;

  function onSegmentationResults(results) {
    latestSegMask = results.segmentationMask;
    segResultCount++;
    if (segResultCount <= 3 || segResultCount % 100 === 0) {
      log('Segmentation result #' + segResultCount,
        'mask:', latestSegMask ? `${latestSegMask.width}x${latestSegMask.height}` : 'null');
    }
  }

  // ─── Processing Loop ───
  let lastSegTime = 0;
  const SEG_INTERVAL = 33;
  let loopCount = 0;
  let lastLoopLogTime = 0;

  let loopTimerId = null;
  const LOOP_INTERVAL_MS = 33; // ~30fps

  function startProcessingLoop() {
    stopProcessingLoop();
    log('Starting processing loop (timer-based, ~30fps)');
    loopCount = 0;

    function loop() {
      loopTimerId = setTimeout(loop, LOOP_INTERVAL_MS);
      loopCount++;
      const timestamp = performance.now();

      if (!currentStream || webcamVideo.readyState < 2) {
        if (loopCount <= 5) log('Loop: webcam not ready, readyState:', webcamVideo.readyState);
        return;
      }

      const w = outputCanvas.width;
      const h = outputCanvas.height;

      if (segmentationReady && selfieSegmentation && timestamp - lastSegTime > SEG_INTERVAL) {
        lastSegTime = timestamp;
        selfieSegmentation.send({ image: webcamVideo }).catch((e) => {
          if (loopCount <= 10) logErr('Segmentation send error:', e);
        });
      }

      if (isFrozen && frozenPersonData) {
        composeFrozenFrame(w, h);
      } else {
        composeLiveFrame(w, h);
      }

      sendFrameToVirtualCamera(w, h);

      if (timestamp - lastLoopLogTime > 5000) {
        lastLoopLogTime = timestamp;
        log(`Loop stats: frame=${loopCount}, segReady=${segmentationReady}, hasMask=${!!latestSegMask}, hasBgVideo=${hasBgVideo}, bgReady=${bgVideo.readyState}, frozen=${isFrozen}`);
      }
    }

    loopTimerId = setTimeout(loop, 0);
  }

  function stopProcessingLoop() {
    if (animFrameId) { cancelAnimationFrame(animFrameId); animFrameId = null; }
    if (loopTimerId) { clearTimeout(loopTimerId); loopTimerId = null; }
  }

  function composeLiveFrame(w, h) {
    if (hasBgVideo && bgVideo.readyState >= 2 && latestSegMask) {
      ctx.drawImage(bgVideo, 0, 0, w, h);

      maskCtx.clearRect(0, 0, w, h);
      maskCtx.drawImage(latestSegMask, 0, 0, w, h);

      personCtx.clearRect(0, 0, w, h);
      personCtx.drawImage(webcamVideo, 0, 0, w, h);
      personCtx.globalCompositeOperation = 'destination-in';
      personCtx.drawImage(maskCanvas, 0, 0, w, h);
      personCtx.globalCompositeOperation = 'source-over';

      ctx.drawImage(personCanvas, 0, 0, w, h);
    } else if (hasBgVideo && bgVideo.readyState >= 2) {
      ctx.drawImage(webcamVideo, 0, 0, w, h);
    } else {
      ctx.drawImage(webcamVideo, 0, 0, w, h);
    }
  }

  function composeFrozenFrame(w, h) {
    if (hasBgVideo && bgVideo.readyState >= 2) {
      ctx.drawImage(bgVideo, 0, 0, w, h);
      ctx.drawImage(frozenPersonData.personCanvas, 0, 0, w, h);
    } else {
      ctx.putImageData(frozenPersonData.fullFrame, 0, 0);
    }
  }

  // ─── Freeze Logic ───
  function setFreezeState(frozen) {
    log('setFreezeState:', frozen);
    const wasChanged = isFrozen !== frozen;
    isFrozen = frozen;

    if (frozen && wasChanged) {
      captureFreeze();
    }

    if (!frozen) {
      frozenPersonData = null;
    }

    freezeBadge.classList.toggle('visible', frozen);
    btnFreeze.classList.toggle('active', frozen);
    freezeLabel.textContent = frozen ? t('on') : t('off');
    freezeLabel.setAttribute('data-i18n', frozen ? 'on' : 'off');
    setStatus(frozen ? t('cameraFrozen') : t('cameraActive'), 'active');
  }

  function captureFreeze() {
    const w = outputCanvas.width;
    const h = outputCanvas.height;
    log('Capturing freeze frame:', w, 'x', h, 'hasMask:', !!latestSegMask);

    const fullFrame = ctx.getImageData(0, 0, w, h);

    const frozenPerson = document.createElement('canvas');
    frozenPerson.width = w;
    frozenPerson.height = h;
    const fpCtx = frozenPerson.getContext('2d');

    if (latestSegMask) {
      fpCtx.drawImage(webcamVideo, 0, 0, w, h);
      fpCtx.globalCompositeOperation = 'destination-in';

      const mCanvas = document.createElement('canvas');
      mCanvas.width = w;
      mCanvas.height = h;
      const mCtx = mCanvas.getContext('2d');
      mCtx.drawImage(latestSegMask, 0, 0, w, h);
      fpCtx.drawImage(mCanvas, 0, 0, w, h);
      fpCtx.globalCompositeOperation = 'source-over';
      log('Freeze captured WITH segmentation mask');
    } else {
      fpCtx.drawImage(webcamVideo, 0, 0, w, h);
      log('Freeze captured WITHOUT segmentation mask (full frame)');
    }

    frozenPersonData = {
      fullFrame: fullFrame,
      personCanvas: frozenPerson,
    };
  }

  // ─── Virtual Camera Output ───
  let frameCounter = 0;
  let lastSendTime = 0;
  const SEND_INTERVAL = 33;
  let sendErrorCount = 0;

  function sendFrameToVirtualCamera(w, h) {
    const now = performance.now();
    if (now - lastSendTime < SEND_INTERVAL) return;
    lastSendTime = now;

    try {
      const imageData = ctx.getImageData(0, 0, w, h);
      window.freezecam.sendFrame(imageData.data.buffer, w, h);
      frameCounter++;
    } catch (e) {
      sendErrorCount++;
      if (sendErrorCount <= 5) {
        logErr('sendFrame error #' + sendErrorCount + ':', e.message);
      }
    }
  }

  // ─── Status ───
  function setStatus(text, state) {
    statusText.textContent = text;
    statusDot.className = 'status-dot';
    if (state === 'active') statusDot.classList.add('active');
    else if (state === 'error') statusDot.classList.add('error');
  }

  // ─── Hotkey ───
  async function loadHotkey() {
    const key = await window.freezecam.getHotkey();
    log('Current hotkey:', key);
    hotkeyLabel.textContent = key;
  }

  function startHotkeyListening() {
    isListeningForHotkey = true;
    btnHotkey.classList.add('listening');
    hotkeyLabel.textContent = '...';
    hotkeyHint.textContent = t('pressAnyKey');
  }

  function stopHotkeyListening() {
    isListeningForHotkey = false;
    btnHotkey.classList.remove('listening');
    hotkeyHint.textContent = t('clickToChange');
  }

  // ─── Event Listeners ───
  function setupEventListeners() {
    const infoOverlay = document.getElementById('info-overlay');
    const infoBody = document.getElementById('info-body');

    document.getElementById('btn-info').addEventListener('click', () => {
      infoBody.innerHTML = t('infoHtml');
      infoOverlay.classList.add('visible');
    });

    document.getElementById('btn-info-close').addEventListener('click', () => {
      infoOverlay.classList.remove('visible');
    });

    infoOverlay.addEventListener('click', (e) => {
      if (e.target === infoOverlay) infoOverlay.classList.remove('visible');
    });

    document.getElementById('btn-lang').addEventListener('click', () => {
      const newLang = currentLang === 'he' ? 'en' : 'he';
      applyLanguage(newLang);
    });

    document.getElementById('btn-minimize').addEventListener('click', () => window.freezecam.minimize());
    document.getElementById('btn-maximize').addEventListener('click', () => window.freezecam.maximize());
    document.getElementById('btn-close').addEventListener('click', () => window.freezecam.close());

    cameraSelect.addEventListener('change', () => {
      log('Camera selected:', cameraSelect.value ? 'device ' + cameraSelect.value.substring(0, 16) + '...' : 'none');
      startCamera(cameraSelect.value);
    });

    btnBgVideo.addEventListener('click', async () => {
      log('Opening bg video dialog...');
      const filePath = await window.freezecam.openVideoDialog();
      log('Bg video dialog result:', filePath);
      if (filePath) {
        const fileUrl = 'file:///' + filePath.replace(/\\/g, '/');
        log('Setting bg video src:', fileUrl);
        bgVideo.src = fileUrl;
        bgVideo.style.display = 'none';

        bgVideo.onerror = (e) => {
          logErr('BG video error event:', bgVideo.error?.message || 'unknown', 'code:', bgVideo.error?.code);
        };
        bgVideo.onloadeddata = () => {
          log('BG video loaded! readyState:', bgVideo.readyState, 'duration:', bgVideo.duration, 'size:', bgVideo.videoWidth, 'x', bgVideo.videoHeight);
        };
        bgVideo.onplay = () => log('BG video playing');
        bgVideo.oncanplay = () => log('BG video canplay event');

        bgVideo.load();
        bgVideo.play().catch((e) => logErr('BG video play() rejected:', e.message));
        hasBgVideo = true;
        const name = filePath.split(/[\\/]/).pop();
        bgVideoLabel.textContent = name.length > 15 ? name.slice(0, 12) + '...' : name;
        bgVideoLabel.title = name;
        bgVideoLabel.removeAttribute('data-i18n');
        btnBgRemove.style.display = '';
      }
    });

    btnBgRemove.addEventListener('click', () => {
      log('Removing background video');
      bgVideo.pause();
      bgVideo.removeAttribute('src');
      bgVideo.load();
      hasBgVideo = false;
      bgVideoLabel.textContent = t('chooseFile');
      bgVideoLabel.setAttribute('data-i18n', 'chooseFile');
      bgVideoLabel.title = '';
      btnBgRemove.style.display = 'none';
    });

    btnHotkey.addEventListener('click', () => {
      if (isListeningForHotkey) {
        stopHotkeyListening();
      } else {
        startHotkeyListening();
      }
    });

    document.addEventListener('keydown', async (e) => {
      if (!isListeningForHotkey) return;
      e.preventDefault();
      e.stopPropagation();

      const parts = [];
      if (e.ctrlKey) parts.push('Ctrl');
      if (e.altKey) parts.push('Alt');
      if (e.shiftKey) parts.push('Shift');

      const key = e.key;
      if (!['Control', 'Alt', 'Shift', 'Meta'].includes(key)) {
        const keyName = key.length === 1 ? key.toUpperCase() : key;
        parts.push(keyName);
      }

      if (parts.length === 0) return;
      if (['Control', 'Alt', 'Shift', 'Meta'].includes(key)) {
        hotkeyLabel.textContent = parts.join('+') + '+...';
        return;
      }

      const combo = parts.join('+');
      log('Hotkey combo captured:', combo);
      const result = await window.freezecam.setHotkey(combo);
      log('Hotkey set result:', result);
      hotkeyLabel.textContent = result.key;
      stopHotkeyListening();
    });

    btnFreeze.addEventListener('click', async () => {
      log('Freeze button clicked');
      const frozen = await window.freezecam.toggleFreeze();
      setFreezeState(frozen);
    });

    window.freezecam.onToggleFreeze((frozen) => {
      log('Freeze toggled via hotkey, frozen:', frozen);
      setFreezeState(frozen);
    });

    navigator.mediaDevices.addEventListener('devicechange', () => {
      log('Device change detected, re-populating cameras');
      populateCameras();
    });
  }

  // ─── Global error handlers ───
  window.addEventListener('error', (e) => {
    logErr('Uncaught error:', e.message, 'at', e.filename + ':' + e.lineno);
  });
  window.addEventListener('unhandledrejection', (e) => {
    logErr('Unhandled rejection:', e.reason);
  });

  // ─── Start ───
  log('app.js loaded, starting init...');
  init();
})();
