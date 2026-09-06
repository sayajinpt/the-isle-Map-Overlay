// ==UserScript==
// @name         The Isle Companion — Voice Island Bridge
// @namespace    local.the-isle-companion
// @version      1.4.1
// @description  Reads Voice Island livemap player positions and posts your pin to The Isle Companion (default: raw API coords).
// @match        *://voice-island.com/*
// @match        *://*.voice-island.com/*
// @match        *://voiceisland.com/*
// @match        *://*.voiceisland.com/*
// @include      *://voice-island.com/*
// @include      *://*.voice-island.com/*
// @include      *://voice-island.com/dashboard/livemap*
// @include      *://voice-island.com/livemap*
// @connect      127.0.0.1
// @connect      localhost
// @connect      api.voice-island.com
// @grant        GM_xmlhttpRequest
// @grant        GM_getValue
// @grant        GM_setValue
// @run-at       document-start
// ==/UserScript==

(function () {
  "use strict";

  // Voice Island's api/players values are NOT the same axis space as Tab Copy
  // Location / our calibration. Their fan-made map applies its own transform.
  // Primary path: read YOUR marker position on their map image (Bosch-style
  // map_x/map_y). API is only used to list names / fall back.

  if (window.top !== window) {
    return;
  }

  const PLAYERS_API = "https://api.voice-island.com/api/players";
  const POLL_MS = 7000;
  const HEALTH_MS = 8000;
  const RELOAD_MS = 5 * 60 * 1000;
  const BADGE_ID = "isle-companion-voice-island-bridge-badge";
  const SCRIPT_VERSION = "1.4.1";
  const PROVIDER = "voice_island";
  const STORAGE_NAME = "isleCompanionVoiceIslandTrackName";
  const STORAGE_PORT = "isleCompanionBridgePort";
  const STORAGE_MODE = "isleCompanionVoiceIslandCoordMode";
  const CANDIDATE_PORTS = [8765, 8770, 8766, 8767, 8768, 8769, 8771, 8775, 8780];
  // Gateway bounds from map/calibration.json (API fallback only).
  const WORLD_Y_MIN = -505000;
  const WORLD_Y_MAX = 607000;
  const MAP_SIZE = 750;
  const COORD_MODES = [
    "api",
    "apiSwap",
    "apiFlipY",
    "apiSwapFlipY",
    "apiXZ",
    "visual",
  ];

  const state = {
    bridgePort: 0,
    lastPayload: "",
    failures: 0,
    lastSentAt: 0,
    inFlight: false,
    lastPlayers: [],
    lastUpdatedAt: "",
    lastSource: "none",
    lastRaw: "",
    trackName: "",
    coordMode: "api",
    reloadAt: Date.now() + RELOAD_MS,
    lastBadgeBase: "",
    lastBadgeOk: true,
    companionOnline: false,
  };

  function storageGet(key, fallback) {
    try {
      if (typeof GM_getValue === "function") {
        return GM_getValue(key, fallback);
      }
      const raw = localStorage.getItem(key);
      return raw == null ? fallback : raw;
    } catch (_e) {
      return fallback;
    }
  }

  function storageSet(key, value) {
    try {
      if (typeof GM_setValue === "function") {
        GM_setValue(key, value);
      } else {
        localStorage.setItem(key, String(value));
      }
    } catch (_e) {}
  }

  state.trackName = String(storageGet(STORAGE_NAME, "") || "");
  const rememberedPort = Number(storageGet(STORAGE_PORT, 0));
  if (Number.isFinite(rememberedPort) && rememberedPort >= 1024 && rememberedPort <= 65535) {
    state.bridgePort = rememberedPort;
  }
  const rememberedMode = String(storageGet(STORAGE_MODE, "api") || "api");
  // Older builds defaulted to visual (wrong direction). Prefer api unless the
  // user explicitly picked another mode after v1.4.1.
  if (rememberedMode === "visual" && !storageGet(STORAGE_MODE + "_locked", "")) {
    state.coordMode = "api";
    storageSet(STORAGE_MODE, "api");
  } else if (COORD_MODES.indexOf(rememberedMode) >= 0) {
    state.coordMode = rememberedMode;
  }

  function saveTrackName(name) {
    state.trackName = String(name || "").trim();
    storageSet(STORAGE_NAME, state.trackName);
  }

  function saveCoordMode(mode) {
    if (COORD_MODES.indexOf(mode) < 0) {
      return;
    }
    state.coordMode = mode;
    storageSet(STORAGE_MODE, mode);
    storageSet(STORAGE_MODE + "_locked", "1");
  }

  function cycleCoordMode() {
    const index = COORD_MODES.indexOf(state.coordMode);
    saveCoordMode(COORD_MODES[(index + 1) % COORD_MODES.length]);
    setBadge("Coord mode: " + state.coordMode, true);
    tick();
  }

  function endpoint(path) {
    return "http://127.0.0.1:" + (state.bridgePort || 8765) + path;
  }

  function gmRequest(options) {
    return new Promise(function (resolve, reject) {
      const req =
        typeof GM_xmlhttpRequest === "function"
          ? GM_xmlhttpRequest
          : typeof GM !== "undefined" && GM && typeof GM.xmlHttpRequest === "function"
            ? GM.xmlHttpRequest.bind(GM)
            : null;
      if (!req) {
        fetch(options.url, {
          method: options.method || "GET",
          headers: options.headers || {},
          body: options.data,
        })
          .then(function (response) {
            return response.text().then(function (text) {
              resolve({ status: response.status, responseText: text });
            });
          })
          .catch(reject);
        return;
      }
      req({
        method: options.method || "GET",
        url: options.url,
        headers: options.headers || {},
        data: options.data,
        timeout: options.timeout || 4000,
        onload: resolve,
        onerror: function () {
          reject(new Error("network error"));
        },
        ontimeout: function () {
          reject(new Error("timeout"));
        },
      });
    });
  }

  function probePort(port) {
    return gmRequest({
      method: "GET",
      url: "http://127.0.0.1:" + port + "/health",
      timeout: 1500,
    })
      .then(function (response) {
        return response.status >= 200 && response.status < 300;
      })
      .catch(function () {
        return false;
      });
  }

  async function discoverBridgePort(force) {
    if (!force && state.bridgePort && state.companionOnline) {
      return state.bridgePort;
    }
    const ordered = [];
    if (state.bridgePort) {
      ordered.push(state.bridgePort);
    }
    for (let i = 0; i < CANDIDATE_PORTS.length; i += 1) {
      if (ordered.indexOf(CANDIDATE_PORTS[i]) < 0) {
        ordered.push(CANDIDATE_PORTS[i]);
      }
    }
    for (let i = 0; i < ordered.length; i += 1) {
      const port = ordered[i];
      if (await probePort(port)) {
        state.bridgePort = port;
        state.companionOnline = true;
        storageSet(STORAGE_PORT, port);
        return port;
      }
    }
    state.companionOnline = false;
    return 0;
  }

  function asNumber(value) {
    if (typeof value === "number" && Number.isFinite(value)) {
      return value;
    }
    if (typeof value === "string") {
      const cleaned = value.replace(/,/g, "").trim();
      const n = Number(cleaned);
      return Number.isFinite(n) ? n : null;
    }
    return null;
  }

  function playerName(player) {
    if (!player || typeof player !== "object") {
      return "";
    }
    const keys = [
      "name",
      "playerName",
      "characterName",
      "dinoName",
      "displayName",
      "username",
      "steamName",
      "discordName",
      "ign",
      "id",
    ];
    for (let i = 0; i < keys.length; i += 1) {
      const value = player[keys[i]];
      if (typeof value === "string" && value.trim()) {
        return value.trim();
      }
      if (typeof value === "number" && Number.isFinite(value)) {
        return String(value);
      }
    }
    return "";
  }

  function summarizePlayer(player) {
    if (!player || typeof player !== "object") {
      return "";
    }
    const bits = [];
    Object.keys(player).forEach(function (key) {
      const value = player[key];
      if (typeof value === "number" && Number.isFinite(value)) {
        bits.push(key + "=" + Number(value).toFixed(1));
      } else if (value && typeof value === "object" && !Array.isArray(value)) {
        ["x", "y", "z", "X", "Y", "Z"].forEach(function (sub) {
          const n = asNumber(value[sub]);
          if (n !== null) {
            bits.push(key + "." + sub + "=" + n.toFixed(1));
          }
        });
      }
    });
    return bits.slice(0, 12).join(" ");
  }

  function pickApiCoords(player, mode) {
    if (!player || typeof player !== "object") {
      return null;
    }
    function fromObj(obj, xk, yk, zk) {
      const x = asNumber(obj[xk]);
      const y = asNumber(obj[yk]);
      if (x === null || y === null) {
        return null;
      }
      const z = asNumber(obj[zk]) ?? asNumber(obj.altitude) ?? 0;
      return { x: x, y: y, z: z === null ? 0 : z };
    }

    const roots = [player];
    ["position", "pos", "location", "loc", "coords", "coordinate"].forEach(function (key) {
      if (player[key] && typeof player[key] === "object") {
        roots.push(player[key]);
      }
    });

    let coords = null;
    for (let i = 0; i < roots.length && !coords; i += 1) {
      const obj = roots[i];
      if (mode === "apiXZ") {
        coords =
          fromObj(obj, "x", "z", "y") ||
          fromObj(obj, "X", "Z", "Y") ||
          fromObj(obj, "locX", "locZ", "locY");
      } else {
        coords =
          fromObj(obj, "x", "y", "z") ||
          fromObj(obj, "X", "Y", "Z") ||
          fromObj(obj, "mapX", "mapY", "z") ||
          fromObj(obj, "worldX", "worldY", "worldZ") ||
          fromObj(obj, "locX", "locY", "locZ");
      }
      if (!coords && Array.isArray(obj) && obj.length >= 2) {
        const x = asNumber(obj[0]);
        const y = asNumber(obj[1]);
        const z = asNumber(obj[2]) ?? 0;
        if (x !== null && y !== null) {
          coords = { x: x, y: y, z: z === null ? 0 : z };
        }
      }
    }
    if (!coords) {
      return null;
    }

    let x = coords.x;
    let y = coords.y;
    if (mode === "apiSwap" || mode === "apiSwapFlipY") {
      const tmp = x;
      x = y;
      y = tmp;
    }
    if (mode === "apiFlipY" || mode === "apiSwapFlipY") {
      if (Math.abs(x) > 1500 || Math.abs(y) > 1500) {
        y = WORLD_Y_MIN + WORLD_Y_MAX - y;
      } else {
        y = MAP_SIZE - y;
      }
    }
    return { x: x, y: y, z: coords.z };
  }

  function isSelfPlayer(player) {
    if (!player || typeof player !== "object") {
      return false;
    }
    const flags = [
      "isSelf",
      "self",
      "me",
      "isMe",
      "isLocal",
      "local",
      "you",
      "isYou",
      "highlighted",
      "selected",
      "tracking",
    ];
    for (let i = 0; i < flags.length; i += 1) {
      if (player[flags[i]] === true) {
        return true;
      }
    }
    const name = playerName(player).toLowerCase();
    if (state.trackName && name && name === state.trackName.toLowerCase()) {
      return true;
    }
    return false;
  }

  function choosePlayer(players) {
    if (!players.length) {
      return null;
    }
    const self = players.find(isSelfPlayer);
    if (self) {
      return self;
    }
    if (state.trackName) {
      const wanted = state.trackName.toLowerCase();
      const named = players.find(function (player) {
        return playerName(player).toLowerCase() === wanted;
      });
      if (named) {
        return named;
      }
    }
    if (players.length === 1) {
      return players[0];
    }
    return null;
  }

  function normalizePlayersPayload(data) {
    if (!data) {
      return [];
    }
    if (Array.isArray(data)) {
      return data;
    }
    if (Array.isArray(data.players)) {
      return data.players;
    }
    if (Array.isArray(data.data)) {
      return data.data;
    }
    if (Array.isArray(data.entities)) {
      return data.entities;
    }
    return [];
  }

  function looksLikeWorldCoords(x, y) {
    return Math.abs(x) > 1500 || Math.abs(y) > 1500;
  }

  function scoreMapCandidate(el) {
    if (!el || !el.getBoundingClientRect) {
      return -1;
    }
    const rect = el.getBoundingClientRect();
    if (rect.width < 180 || rect.height < 180) {
      return -1;
    }
    const ratio = rect.width / rect.height;
    if (ratio < 0.7 || ratio > 1.4) {
      return rect.width * rect.height * 0.25;
    }
    let score = rect.width * rect.height;
    const cls = String(el.className || "").toLowerCase();
    const id = String(el.id || "").toLowerCase();
    if (/map|live|leaflet|canvas/.test(cls + " " + id)) {
      score *= 3;
    }
    if (el.tagName === "CANVAS" || el.tagName === "IMG" || el.tagName === "SVG") {
      score *= 1.5;
    }
    return score;
  }

  function findMapRoot() {
    const selectors = [
      ".leaflet-container",
      "[class*='leaflet']",
      "[class*='livemap']",
      "[class*='live-map']",
      "[class*='LiveMap']",
      "[class*='map-container']",
      "[class*='mapContainer']",
      "main canvas",
      "main img",
      "canvas",
      "svg",
      "img",
    ];
    let best = null;
    let bestScore = 0;
    selectors.forEach(function (selector) {
      document.querySelectorAll(selector).forEach(function (el) {
        const score = scoreMapCandidate(el);
        if (score > bestScore) {
          best = el;
          bestScore = score;
        }
      });
    });
    if (best && best.tagName === "IMG") {
      return best.parentElement || best;
    }
    return best;
  }

  function markerCenter(el) {
    const rect = el.getBoundingClientRect();
    return {
      x: rect.left + rect.width / 2,
      y: rect.top + rect.height / 2,
      area: Math.max(1, rect.width * rect.height),
      el: el,
      label: (
        el.getAttribute("aria-label") ||
        el.getAttribute("title") ||
        el.textContent ||
        ""
      )
        .trim()
        .slice(0, 80),
    };
  }

  function collectMarkerCandidates(mapRoot) {
    const root = mapRoot || document.body;
    const nodes = root.querySelectorAll(
      [
        ".leaflet-marker-icon",
        "[class*='marker']",
        "[class*='Marker']",
        "[class*='player']",
        "[class*='Player']",
        "[class*='pin']",
        "[class*='Pin']",
        "[data-player]",
        "[data-name]",
        "button",
        "div[style*='translate']",
        "div[style*='left']",
      ].join(",")
    );
    const mapRect = (mapRoot || root).getBoundingClientRect();
    const out = [];
    nodes.forEach(function (el) {
      if (el === mapRoot) {
        return;
      }
      const rect = el.getBoundingClientRect();
      if (rect.width < 2 || rect.height < 2 || rect.width > 120 || rect.height > 120) {
        return;
      }
      const cx = rect.left + rect.width / 2;
      const cy = rect.top + rect.height / 2;
      if (
        cx < mapRect.left - 4 ||
        cy < mapRect.top - 4 ||
        cx > mapRect.right + 4 ||
        cy > mapRect.bottom + 4
      ) {
        return;
      }
      out.push(markerCenter(el));
    });
    return out;
  }

  function pickVisualMarker(markers) {
    if (!markers.length) {
      return null;
    }
    const wanted = state.trackName.trim().toLowerCase();
    if (wanted) {
      const named = markers.find(function (marker) {
        return marker.label.toLowerCase().indexOf(wanted) >= 0;
      });
      if (named) {
        return named;
      }
    }
    const flagged = markers.find(function (marker) {
      const cls = String(marker.el.className || "").toLowerCase();
      return /self|you|me|local|selected|active|current|own/.test(cls);
    });
    if (flagged) {
      return flagged;
    }
    if (markers.length === 1) {
      return markers[0];
    }
    // Prefer smaller icons near the visual center only when tracking name is unset
    // and multiple markers exist — require explicit name instead.
    return null;
  }

  function readVisualMapCoords() {
    const mapRoot = findMapRoot();
    if (!mapRoot) {
      return null;
    }
    const mapRect = mapRoot.getBoundingClientRect();
    if (mapRect.width < 180 || mapRect.height < 180) {
      return null;
    }
    const markers = collectMarkerCandidates(mapRoot);
    const marker = pickVisualMarker(markers);
    if (!marker) {
      return {
        error:
          markers.length > 1
            ? "visual: " + markers.length + " pins — set YOUR name"
            : "visual: no player pin found on map",
        markerCount: markers.length,
      };
    }
    const mapX = ((marker.x - mapRect.left) / mapRect.width) * MAP_SIZE;
    // Screen Y increases downward; Bosch map_y uses the same image space.
    const mapY = ((marker.y - mapRect.top) / mapRect.height) * MAP_SIZE;
    if (!Number.isFinite(mapX) || !Number.isFinite(mapY)) {
      return null;
    }
    return {
      map_x: mapX,
      map_y: mapY,
      altitude: 0,
      map_size: MAP_SIZE,
      provider: PROVIDER,
      source: "visual-marker",
      player_name: state.trackName || marker.label || "",
      marker_count: markers.length,
    };
  }

  function payloadFromApiPlayer(player, mode) {
    const coords = pickApiCoords(player, mode);
    if (!coords) {
      return null;
    }
    const name = playerName(player);
    state.lastRaw = summarizePlayer(player);
    if (looksLikeWorldCoords(coords.x, coords.y)) {
      return {
        x: coords.x,
        y: coords.y,
        z: coords.z,
        provider: PROVIDER,
        source: "api:" + mode,
        player_name: name,
      };
    }
    return {
      map_x: coords.x,
      map_y: coords.y,
      altitude: coords.z,
      map_size: MAP_SIZE,
      provider: PROVIDER,
      source: "api:" + mode,
      player_name: name,
    };
  }

  function ensureBadge() {
    if (!document.documentElement) {
      return null;
    }
    let badge = document.getElementById(BADGE_ID);
    if (badge) {
      return badge;
    }
    badge = document.createElement("div");
    badge.id = BADGE_ID;
    badge.style.cssText = [
      "position:fixed",
      "right:12px",
      "bottom:12px",
      "z-index:2147483647",
      "padding:8px 10px",
      "border-radius:8px",
      "background:rgba(12,24,31,0.94)",
      "color:#dff8fc",
      "font:12px/1.35 Segoe UI,sans-serif",
      "border:1px solid rgba(105,188,207,0.7)",
      "box-shadow:0 6px 18px rgba(0,0,0,0.35)",
      "max-width:360px",
      "white-space:pre-line",
      "pointer-events:auto",
    ].join(";");
    const text = document.createElement("div");
    text.id = BADGE_ID + "-text";
    badge.appendChild(text);
    const row = document.createElement("div");
    row.style.cssText = "margin-top:6px;display:flex;gap:6px;align-items:center;flex-wrap:wrap;";
    const input = document.createElement("input");
    input.id = BADGE_ID + "-name";
    input.type = "text";
    input.placeholder = "Your map / player name";
    input.value = state.trackName;
    input.style.cssText =
      "flex:1;min-width:120px;padding:4px 6px;border-radius:4px;border:1px solid #4a7a86;background:#0b171d;color:#e8fbff;";
    const button = document.createElement("button");
    button.textContent = "Track";
    button.style.cssText =
      "padding:4px 8px;border-radius:4px;border:1px solid #6ad8ea;background:#1a6b7c;color:#e8fbff;cursor:pointer;";
    button.addEventListener("click", function () {
      saveTrackName(input.value);
      setBadge("Tracking \"" + state.trackName + "\"", true);
      tick();
    });
    const modeBtn = document.createElement("button");
    modeBtn.textContent = "Mode";
    modeBtn.title = "Cycle visual / API axis modes";
    modeBtn.style.cssText =
      "padding:4px 8px;border-radius:4px;border:1px solid #6ad8ea;background:#1a6b7c;color:#e8fbff;cursor:pointer;";
    modeBtn.addEventListener("click", cycleCoordMode);
    const portInput = document.createElement("input");
    portInput.id = BADGE_ID + "-port";
    portInput.type = "number";
    portInput.min = "1024";
    portInput.max = "65535";
    portInput.placeholder = "port";
    portInput.value = state.bridgePort ? String(state.bridgePort) : "";
    portInput.style.cssText =
      "width:72px;padding:4px 6px;border-radius:4px;border:1px solid #4a7a86;background:#0b171d;color:#e8fbff;";
    const portBtn = document.createElement("button");
    portBtn.textContent = "Port";
    portBtn.style.cssText =
      "padding:4px 8px;border-radius:4px;border:1px solid #6ad8ea;background:#1a6b7c;color:#e8fbff;cursor:pointer;";
    portBtn.addEventListener("click", async function () {
      const port = Number(portInput.value);
      if (!Number.isFinite(port) || port < 1024 || port > 65535) {
        setBadge("Enter companion port from Options", false);
        return;
      }
      state.bridgePort = port;
      storageSet(STORAGE_PORT, port);
      const ok = await probePort(port);
      state.companionOnline = ok;
      setBadge(ok ? "Companion online on " + port : "No companion on " + port, ok);
      if (ok) {
        tick();
      }
    });
    row.appendChild(input);
    row.appendChild(button);
    row.appendChild(modeBtn);
    row.appendChild(portInput);
    row.appendChild(portBtn);
    badge.appendChild(row);
    document.documentElement.appendChild(badge);
    return badge;
  }

  function reloadCountdownText() {
    const seconds = Math.max(0, Math.ceil((state.reloadAt - Date.now()) / 1000));
    return "soft-reload in " + seconds + "s";
  }

  function setBadge(text, ok) {
    state.lastBadgeBase = text;
    state.lastBadgeOk = !!ok;
    const badge = ensureBadge();
    if (!badge) {
      return;
    }
    const names = state.lastPlayers
      .map(playerName)
      .filter(Boolean)
      .slice(0, 6)
      .join(", ");
    const textNode = document.getElementById(BADGE_ID + "-text");
    if (textNode) {
      textNode.textContent =
        text +
        "\n" +
        reloadCountdownText() +
        "\nMode: " +
        state.coordMode +
        (state.bridgePort ? " · port " + state.bridgePort : "") +
        (names ? "\nOnline: " + names : "") +
        (state.trackName ? "\nTracking: " + state.trackName : "") +
        (state.lastRaw ? "\nRaw: " + state.lastRaw : "");
    }
    const portInput = document.getElementById(BADGE_ID + "-port");
    if (portInput && state.bridgePort && document.activeElement !== portInput) {
      portInput.value = String(state.bridgePort);
    }
    badge.style.borderColor = ok
      ? "rgba(106,216,234,0.9)"
      : "rgba(255,125,125,0.85)";
  }

  function refreshBadgeCountdown() {
    if (!state.lastBadgeBase) {
      return;
    }
    setBadge(state.lastBadgeBase, state.lastBadgeOk);
  }

  function softReload() {
    setBadge(
      "Isle Companion Voice Island bridge v" + SCRIPT_VERSION + ": refreshing…",
      true
    );
    window.location.reload();
  }

  async function postJson(payload) {
    const port = await discoverBridgePort(false);
    if (!port) {
      throw new Error("no bridge");
    }
    const response = await gmRequest({
      method: "POST",
      url: endpoint("/position"),
      headers: { "Content-Type": "application/json" },
      data: JSON.stringify(payload),
      timeout: 4000,
    });
    if (response.status >= 200 && response.status < 300) {
      state.companionOnline = true;
      return response;
    }
    throw new Error("HTTP " + response.status);
  }

  function formatCoords(payload) {
    const x = payload.map_x != null ? payload.map_x : payload.x;
    const y = payload.map_y != null ? payload.map_y : payload.y;
    return Number(x).toFixed(1) + ", " + Number(y).toFixed(1);
  }

  async function sendPayload(payload) {
    if (!payload || state.inFlight) {
      return;
    }
    const encoded = JSON.stringify(payload);
    if (encoded === state.lastPayload && state.failures === 0 && state.companionOnline) {
      setBadge(
        "Voice Island: live (" +
          formatCoords(payload) +
          ") via " +
          (payload.source || state.coordMode),
        true
      );
      return;
    }
    state.inFlight = true;
    try {
      await postJson(payload);
      state.lastPayload = encoded;
      state.failures = 0;
      state.lastSentAt = Date.now();
      setBadge(
        "Voice Island: sent (" +
          formatCoords(payload) +
          ") via " +
          (payload.source || state.coordMode),
        true
      );
    } catch (_error) {
      state.failures += 1;
      state.companionOnline = false;
      await discoverBridgePort(true);
      setBadge(
        state.companionOnline
          ? "Send failed on port " + state.bridgePort
          : "Companion offline — enable Live map bridge",
        false
      );
    } finally {
      state.inFlight = false;
    }
  }

  async function refreshPlayersFromApi() {
    try {
      const response = await fetch(PLAYERS_API, {
        method: "GET",
        credentials: "include",
        headers: { Accept: "application/json" },
        cache: "no-store",
      });
      if (!response.ok) {
        return null;
      }
      const data = await response.json();
      state.lastPlayers = normalizePlayersPayload(data);
      state.lastUpdatedAt = (data && data.updated_at) || "";
      state.lastSource = "api/players";
      const player = choosePlayer(state.lastPlayers);
      if (player) {
        state.lastRaw = summarizePlayer(player);
      }
      return player;
    } catch (_e) {
      return null;
    }
  }

  async function tick() {
    ensureBadge();
    const mode = state.coordMode;

    if (mode === "visual") {
      await refreshPlayersFromApi();
      const visual = readVisualMapCoords();
      if (visual && visual.map_x != null) {
        await sendPayload(visual);
        return;
      }
      // Fallback: API normal while visual is unavailable, but keep mode label.
      const player = choosePlayer(state.lastPlayers);
      if (player) {
        const payload = payloadFromApiPlayer(player, "api");
        if (payload) {
          payload.source = "api-fallback";
          await sendPayload(payload);
          setBadge(
            "Visual pin not found — using API fallback (click Mode if direction is wrong)",
            false
          );
          return;
        }
      }
      setBadge(
        (visual && visual.error) ||
          "Visual mode: open the livemap and set your player name",
        false
      );
      return;
    }

    const player = (await refreshPlayersFromApi()) || choosePlayer(state.lastPlayers);
    if (!player) {
      setBadge(
        state.lastPlayers.length
          ? "API: " + state.lastPlayers.length + " players — set YOUR name"
          : "API: 0 players / stay signed in on livemap",
        !state.lastPlayers.length
      );
      return;
    }
    const payload = payloadFromApiPlayer(player, mode);
    if (!payload) {
      setBadge("API player has no readable x/y — raw: " + state.lastRaw, false);
      return;
    }
    await sendPayload(payload);
  }

  function installNetworkSpies() {
    const NativeFetch = window.fetch;
    if (typeof NativeFetch === "function") {
      window.fetch = function () {
        const args = arguments;
        return NativeFetch.apply(this, args).then(function (response) {
          try {
            const url = String(response && response.url ? response.url : args[0] || "");
            if (/api\.voice-island\.com\/api\/players/i.test(url) && response.ok) {
              response
                .clone()
                .json()
                .then(function (data) {
                  state.lastPlayers = normalizePlayersPayload(data);
                  state.lastUpdatedAt = (data && data.updated_at) || "";
                  const player = choosePlayer(state.lastPlayers);
                  if (player) {
                    state.lastRaw = summarizePlayer(player);
                  }
                })
                .catch(function () {});
            }
          } catch (_e) {}
          return response;
        });
      };
    }
  }

  installNetworkSpies();

  function boot() {
    ensureBadge();
    setBadge(
      "Isle Companion Voice Island bridge v" + SCRIPT_VERSION + " running…",
      true
    );
    discoverBridgePort(true).then(function () {
      tick();
    });
    setInterval(tick, POLL_MS);
    setInterval(function () {
      discoverBridgePort(true);
    }, HEALTH_MS);
    setInterval(refreshBadgeCountdown, 1000);
    setTimeout(softReload, RELOAD_MS);
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", boot);
  } else {
    boot();
  }
})();
