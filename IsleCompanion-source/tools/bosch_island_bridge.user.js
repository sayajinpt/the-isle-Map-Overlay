// ==UserScript==
// @name         The Isle Companion — Bosch Island Bridge
// @namespace    local.the-isle-companion
// @version      1.5.0
// @description  Sends Bosch Island Tracker Live Position to The Isle Companion Mini Map over localhost. Auto-reloads the tracker tab every 90s so Bosch live updates keep flowing.
// @match        *://bosch-island.com/*
// @match        *://*.bosch-island.com/*
// @match        *://boschisland.com/*
// @match        *://*.boschisland.com/*
// @include      *://bosch-island.com/*
// @include      *://*.bosch-island.com/*
// @connect      127.0.0.1
// @connect      localhost
// @grant        GM_xmlhttpRequest
// @grant        GM_getValue
// @grant        GM_setValue
// @run-at       document-idle
// ==/UserScript==

(function () {
  "use strict";

  // v1.2 hooked fetch/XHR/WebSocket and broke Bosch's own live socket after a
  // few updates (needed Ctrl+F5). This build only reads the Live Position DOM
  // and posts to the companion — it never touches page networking.
  //
  // Bosch's tracker can still freeze its Live Position after a couple of
  // updates. v1.4 soft-reloads this tab every 90s so the live feed restarts
  // without the user having to refresh manually.
  //
  // v1.5 auto-detects companion port (8765–8780). Your Options port may be 8770.

  if (window.top !== window) {
    return;
  }

  const POLL_MS = 750;
  const RELOAD_MS = 90000;
  const BADGE_ID = "isle-companion-bosch-bridge-badge";
  const SCRIPT_VERSION = "1.5.0";
  const STORAGE_PORT = "isleCompanionBridgePort";
  const CANDIDATE_PORTS = [8765, 8770, 8766, 8767, 8768, 8769, 8771, 8775, 8780];
  const state = {
    bridgePort: 0,
    companionOnline: false,
    lastPayload: "",
    failures: 0,
    lastSentAt: 0,
    lastSeenAt: 0,
    inFlight: false,
    observer: null,
    startedAt: Date.now(),
    reloadAt: Date.now() + RELOAD_MS,
    lastBadgeBase: "",
    lastBadgeOk: true,
  };

  try {
    const remembered = Number(
      typeof GM_getValue === "function" ? GM_getValue(STORAGE_PORT, 0) : 0
    );
    if (Number.isFinite(remembered) && remembered >= 1024 && remembered <= 65535) {
      state.bridgePort = remembered;
    }
  } catch (_e) {}

  function endpoint(path) {
    return "http://127.0.0.1:" + (state.bridgePort || 8765) + path;
  }

  function gmRequest(options) {
    return new Promise(function (resolve, reject) {
      if (typeof GM_xmlhttpRequest !== "function") {
        reject(new Error("GM_xmlhttpRequest missing"));
        return;
      }
      GM_xmlhttpRequest({
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
        try {
          if (typeof GM_setValue === "function") {
            GM_setValue(STORAGE_PORT, port);
          }
        } catch (_e) {}
        return port;
      }
    }
    state.companionOnline = false;
    return 0;
  }
  function parseLocaleNumber(raw) {
    if (typeof raw === "number" && Number.isFinite(raw)) {
      return raw;
    }
    let text = String(raw || "")
      .replace(/\u00a0/g, " ")
      .trim();
    if (!text) {
      return null;
    }
    text = text.replace(/\s+/g, "");
    if (text.includes(",") && text.includes(".")) {
      if (text.lastIndexOf(",") > text.lastIndexOf(".")) {
        text = text.replace(/\./g, "").replace(",", ".");
      } else {
        text = text.replace(/,/g, "");
      }
    } else if (text.includes(",")) {
      const parts = text.split(",");
      if (parts.length === 2 && parts[1].length <= 3) {
        text = parts[0] + "." + parts[1];
      } else {
        text = text.replace(/,/g, "");
      }
    }
    const value = Number(text);
    return Number.isFinite(value) ? value : null;
  }

  function valueAfterLabel(text, labels) {
    const source = String(text || "");
    for (const label of labels) {
      const pattern = new RegExp(
        label + "\\s*[:：]?\\s*([+-]?\\d[\\d\\s.,]*)",
        "i"
      );
      const match = source.match(pattern);
      if (!match) {
        continue;
      }
      const value = parseLocaleNumber(match[1]);
      if (value !== null) {
        return value;
      }
    }
    return null;
  }

  function readMapSize(text) {
    const match = String(text || "").match(/(\d{2,4})\s*[x×]\s*(\d{2,4})/i);
    if (!match) {
      return 750;
    }
    const width = Number(match[1]);
    const height = Number(match[2]);
    if (
      Number.isFinite(width) &&
      Number.isFinite(height) &&
      Math.abs(width - height) < 2
    ) {
      return width;
    }
    return 750;
  }

  function extractPositionFromText(text) {
    const mapX = valueAfterLabel(text, ["map\\s*x", "mapx"]);
    const mapY = valueAfterLabel(text, ["map\\s*y", "mapy"]);
    const altitude = valueAfterLabel(text, ["altitude", "\\balt\\b"]);
    if (mapX === null || mapY === null) {
      return null;
    }
    return {
      map_x: mapX,
      map_y: mapY,
      altitude: altitude === null ? 0 : altitude,
      map_size: readMapSize(text),
      source: "bosch-island-tracker-dom",
      provider: "bosch",
    };
  }

  function collectCandidateTexts() {
    const texts = [];
    if (!document.body) {
      return texts;
    }

    const labeled = document.evaluate(
      "//*[contains(translate(normalize-space(.), 'MAPX', 'mapx'), 'map x') or contains(translate(normalize-space(.), 'MAPX', 'mapx'), 'mapx')]",
      document.body,
      null,
      XPathResult.ORDERED_NODE_SNAPSHOT_TYPE,
      null
    );
    for (let index = 0; index < labeled.snapshotLength; index += 1) {
      const node = labeled.snapshotItem(index);
      if (!node) {
        continue;
      }
      // Prefer a small ancestor that still holds Map Y / Altitude.
      let current = node;
      for (let depth = 0; depth < 6 && current; depth += 1) {
        const text = (current.innerText || current.textContent || "").trim();
        if (
          text &&
          text.length < 1200 &&
          /map\s*x/i.test(text) &&
          /map\s*y/i.test(text)
        ) {
          texts.push(text);
          break;
        }
        current = current.parentElement;
      }
    }

    // Fallback: full page text (avoid when possible — expensive).
    if (!texts.length) {
      const full = (document.body.innerText || document.body.textContent || "").trim();
      if (full) {
        texts.push(full.slice(0, 20000));
      }
    }
    return texts;
  }

  function readLivePositionFromDom() {
    for (const text of collectCandidateTexts()) {
      const found = extractPositionFromText(text);
      if (found) {
        return found;
      }
    }
    return null;
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
    badge.textContent = "Isle Companion bridge: starting…";
    badge.style.cssText = [
      "position:fixed",
      "right:12px",
      "bottom:12px",
      "z-index:2147483647",
      "padding:8px 10px",
      "border-radius:8px",
      "background:rgba(12,24,31,0.92)",
      "color:#dff8fc",
      "font:12px/1.35 Segoe UI,sans-serif",
      "border:1px solid rgba(105,188,207,0.7)",
      "box-shadow:0 6px 18px rgba(0,0,0,0.35)",
      "pointer-events:none",
      "white-space:pre-line",
      "max-width:280px",
    ].join(";");
    document.documentElement.appendChild(badge);
    return badge;
  }

  function reloadCountdownText() {
    const seconds = Math.max(0, Math.ceil((state.reloadAt - Date.now()) / 1000));
    return "auto-reload in " + seconds + "s";
  }

  function setBadge(text, ok) {
    state.lastBadgeBase = text;
    state.lastBadgeOk = !!ok;
    const badge = ensureBadge();
    if (!badge) {
      return;
    }
    badge.textContent = text + "\n" + reloadCountdownText();
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

  function softReloadTrackerTab() {
    setBadge(
      "Isle Companion bridge v" + SCRIPT_VERSION + ": refreshing Bosch tab…",
      true
    );
    // Soft reload is enough to restart Bosch live updates; avoid cache-bust
    // hard refresh so the tab comes back quicker.
    window.location.reload();
  }

  function postJson(payload) {
    return discoverBridgePort(false).then(function (port) {
      if (!port) {
        throw new Error("no bridge");
      }
      return gmRequest({
        method: "POST",
        url: endpoint("/position"),
        headers: { "Content-Type": "application/json" },
        data: JSON.stringify(payload),
        timeout: 4000,
      }).then(function (response) {
        if (response.status >= 200 && response.status < 300) {
          state.companionOnline = true;
          return response;
        }
        throw new Error("HTTP " + response.status);
      });
    });
  }

  function formatCoords(position) {
    const x = position.map_x != null ? position.map_x : position.x;
    const y = position.map_y != null ? position.map_y : position.y;
    return Number(x).toFixed(2) + ", " + Number(y).toFixed(2);
  }

  async function pushPosition() {
    if (state.inFlight) {
      return;
    }
    ensureBadge();
    const position = readLivePositionFromDom();
    if (!position) {
      setBadge(
        "Isle Companion bridge v" +
          SCRIPT_VERSION +
          ": waiting for Live Position (Map X / Map Y)…",
        false
      );
      return;
    }
    state.lastSeenAt = Date.now();
    const encoded = JSON.stringify({
      map_x: position.map_x,
      map_y: position.map_y,
      altitude: position.altitude,
      map_size: position.map_size,
      provider: "bosch",
    });
    if (encoded === state.lastPayload && state.failures === 0) {
      setBadge(
        "Isle Companion bridge: live (" + formatCoords(position) + ")",
        true
      );
      return;
    }

    state.inFlight = true;
    try {
      await postJson(position);
      state.lastPayload = encoded;
      state.failures = 0;
      state.lastSentAt = Date.now();
      setBadge(
        "Isle Companion bridge: sent (" + formatCoords(position) + ")",
        true
      );
    } catch (_error) {
      state.failures += 1;
      state.companionOnline = false;
      setBadge(
        "Isle Companion bridge: offline — enable Live map bridge (port often 8770)",
        false
      );
    } finally {
      state.inFlight = false;
    }
  }

  function installDomObserver() {
    if (!document.body || state.observer) {
      return;
    }
    let scheduled = false;
    state.observer = new MutationObserver(function () {
      if (scheduled) {
        return;
      }
      scheduled = true;
      setTimeout(function () {
        scheduled = false;
        pushPosition();
      }, 120);
    });
    state.observer.observe(document.body, {
      childList: true,
      subtree: true,
      characterData: true,
    });
  }

  setBadge("Isle Companion bridge v" + SCRIPT_VERSION + " running…", true);
  installDomObserver();
  setInterval(pushPosition, POLL_MS);
  setInterval(refreshBadgeCountdown, 1000);
  setTimeout(softReloadTrackerTab, RELOAD_MS);
  pushPosition();
})();
