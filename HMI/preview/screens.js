/* Maqueta del HMI estilo Nothing (modo oscuro). Dibuja cada pagina en un div de
   480x272 con posiciones y colores exactos segun pantallas.md de la feature 007.
   build(page, {pressed, bg, lang}) devuelve el div; bg=true omite el contenido dinamico
   (estado, valores y mensaje) para usarlo como imagen de fondo de la Nextion. */
(function () {
  "use strict";

  var COL = {
    black: "#000000",
    surface: "#111111",
    surfaceRaised: "#1A1A1A",
    border: "#222222",
    borderVisible: "#333333",
    textDisabled: "#666666",
    textSecondary: "#999999",
    textPrimary: "#E8E8E8",
    textDisplay: "#FFFFFF",
    success: "#4A9E5C",
    warning: "#D4A843",
    accent: "#D71921"
  };

  var F_DOTO = '"Doto", monospace';

  function el(tag, style) {
    var e = document.createElement(tag);
    if (style) {
      for (var k in style) { e.style[k] = style[k]; }
    }
    return e;
  }

  function abs(x, y, w, h) {
    return {
      position: "absolute", left: x + "px", top: y + "px",
      width: w + "px", height: h + "px",
      boxSizing: "border-box", margin: "0", padding: "0"
    };
  }

  /* Texto absoluto de una sola linea. lh=alto para centrar verticalmente. */
  function text(o) {
    var e = el("div", Object.assign(abs(o.x, o.y, o.w || 0, o.h || 0), {
      color: o.color, fontFamily: o.font, fontSize: (o.size || 12) + "px",
      fontWeight: o.weight || "900", letterSpacing: o.ls || "0",
      whiteSpace: "nowrap", textAlign: o.align || "left", lineHeight: o.lh || "1"
    }));
    e.textContent = o.content;
    return e;
  }

  function span(content, font, size, color, ls) {
    var s = el("span", {
      fontFamily: font, fontSize: size + "px", fontWeight: "900",
      color: color, letterSpacing: ls || "0", lineHeight: "1", whiteSpace: "nowrap"
    });
    s.textContent = content;
    return s;
  }

  function poly(points, color) {
    var p = document.createElementNS("http://www.w3.org/2000/svg", "polyline");
    p.setAttribute("fill", "none");
    p.setAttribute("stroke", color);
    p.setAttribute("stroke-width", "1.5");
    p.setAttribute("stroke-linecap", "round");
    p.setAttribute("stroke-linejoin", "round");
    p.setAttribute("points", points);
    return p;
  }

  function chevron(dir, color) {
    var svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    svg.setAttribute("width", "12");
    svg.setAttribute("height", "12");
    svg.setAttribute("viewBox", "0 0 12 12");
    svg.appendChild(poly(dir === "<" ? "7,2 3,6 7,10" : "5,2 9,6 5,10", color));
    return svg;
  }

  function arrow(dir, color) {
    var svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    svg.setAttribute("width", "32");
    svg.setAttribute("height", "32");
    svg.setAttribute("viewBox", "0 0 32 32");
    if (dir === "left") {
      svg.appendChild(poly("12,9 6,16 12,23", color));
      svg.appendChild(poly("6,16 26,16", color));
    } else {
      svg.appendChild(poly("20,9 26,16 20,23", color));
      svg.appendChild(poly("26,16 6,16", color));
    }
    return svg;
  }

  var LOGO = '<ellipse cx="13.5" cy="14.5" rx="7" ry="4.5"/>' +
    '<circle cx="4.5" cy="13.5" r="2.5"/>' +
    '<path d="M11.5 10.3 V18.7 M15.5 10.3 V18.7"/>' +
    '<path d="M11 10.5 C9 4.5 13.5 4 13.5 10 M14 10 C14.5 4 19.5 4.5 17 10.8"/>' +
    '<path d="M3.5 11.2 L2 8 M5.5 11.2 L6.5 8"/>' +
    '<path d="M20.5 14.5 H22.5"/>';

  function logo(color) {
    var wrap = el("div", Object.assign(abs(16, 16, 24, 24), { color: color }));
    wrap.innerHTML = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">' + LOGO + '</svg>';
    return wrap;
  }

  function screenRoot(height) {
    return el("div", {
      position: "relative", overflow: "hidden",
      width: "480px", height: (height || 272) + "px",
      background: COL.black
    });
  }

  function backButton(pressed) {
    var b = el("div", Object.assign(abs(16, 16, 44, 44), {
      background: pressed ? COL.textDisplay : COL.surface,
      border: "1px solid " + COL.borderVisible,
      borderRadius: "22px",
      display: "flex", alignItems: "center", justifyContent: "center"
    }));
    b.appendChild(chevron("<", pressed ? COL.black : COL.textPrimary));
    return b;
  }

  function title(content) {
    return text({
      x: 76, y: 16, w: 388, h: 44, lh: "44px",
      content: content, font: F_DOTO, size: 24, color: COL.textDisplay
    });
  }

  function primary(x, y, w, h, content, pressed) {
    var b = el("div", Object.assign(abs(x, y, w, h), {
      background: pressed ? COL.textSecondary : COL.textDisplay,
      borderRadius: (h / 2) + "px",
      display: "flex", alignItems: "center", justifyContent: "center"
    }));
    b.appendChild(span(content, F_DOTO, 16, COL.black, "0.06em"));
    return b;
  }

  function secondary(x, y, w, h, content, pressed) {
    var b = el("div", Object.assign(abs(x, y, w, h), {
      background: pressed ? COL.surfaceRaised : "transparent",
      border: "1px solid " + (pressed ? COL.textPrimary : COL.borderVisible),
      borderRadius: (h / 2) + "px",
      display: "flex", alignItems: "center", justifyContent: "center"
    }));
    b.appendChild(span(content, F_DOTO, 16, pressed ? COL.textDisplay : COL.textPrimary, "0.06em"));
    return b;
  }

  function label(x, y, content, color) {
    return text({
      x: x, y: y, h: 12, lh: "12px",
      content: content, font: F_DOTO, size: 12, color: color || COL.textSecondary, ls: "0.08em"
    });
  }

  /* Textos de la interfaz por idioma; PRINTMAQ, AUTO-KIT y JOG no cambian. */
  var TXT = {
    en: {
      status: "STATUS",
      home: "HOME",
      settings: "SETTINGS",
      menuTitle: "Settings",
      menuRows: ["Home speeds", "Print speeds", "Jog axes"],
      jogTitle: "Move stroke",
      holdToMove: "HOLD TO MOVE",
      homeSpeedTitle: "Home speeds",
      printSpeedTitle: "Print speeds",
      fast: "FAST",
      slow: "SLOW",
      stroke: "STROKE",
      squeegee: "SQUEEGEE",
      stepsPerSec: "STEPS/S",
      save: "SAVE",
      saved: "[SAVED]",
      del: "DELETE",
      ok: "OK",
      states: [
        { name: "NOT HOMED", text: "NOT HOMED" },
        { name: "HOMING", text: "HOMING" },
        { name: "HOMED", text: "HOMED" },
        { name: "MOVING", text: "MOVING" },
        { name: "PRINTING", text: "PRINTING" },
        { name: "ERROR", text: "ERROR" }
      ]
    },
    es: {
      status: "ESTADO",
      home: "HOME",
      settings: "AJUSTES",
      menuTitle: "Ajustes",
      menuRows: ["Velocidades de home", "Velocidades de impresión", "Mover ejes"],
      jogTitle: "Mover stroke",
      holdToMove: "MANTENER PRESIONADO PARA MOVER",
      homeSpeedTitle: "Velocidades de home",
      printSpeedTitle: "Velocidades de impresión",
      fast: "RÁPIDA",
      slow: "LENTA",
      stroke: "STROKE",
      squeegee: "SQUEEGEE",
      stepsPerSec: "PASOS/S",
      save: "GUARDAR",
      saved: "[GUARDADO]",
      del: "BORRAR",
      ok: "OK",
      states: [
        { name: "SIN HOME", text: "SIN HOME" },
        { name: "HACIENDO HOME", text: "HACIENDO HOME" },
        { name: "EN HOME", text: "EN HOME" },
        { name: "MOVIENDO", text: "MOVIENDO" },
        { name: "IMPRIMIENDO", text: "IMPRIMIENDO" },
        { name: "ERROR", text: "ERROR" }
      ]
    }
  };

  function buildMain(pressed, bg, lang) {
    var t = TXT[lang];
    var root = screenRoot();
    root.appendChild(logo(COL.textPrimary));
    root.appendChild(text({ x: 48, y: 16, h: 24, lh: "24px", content: "PRINTMAQ", font: F_DOTO, size: 12, color: COL.textSecondary, ls: "0.08em" }));
    root.appendChild(text({ x: 16, y: 16, w: 448, h: 24, lh: "24px", content: "AUTO-KIT", font: F_DOTO, size: 12, color: COL.textDisabled, align: "right", ls: "0.08em" }));
    root.appendChild(label(16, 74, t.status));
    if (!bg) {
      root.appendChild(text({ x: 16, y: 92, w: 448, h: 64, lh: "64px", content: t.states[2].text, font: F_DOTO, size: 48, weight: "700", color: COL.success }));
    }
    root.appendChild(primary(16, 192, 288, 64, t.home, pressed));
    root.appendChild(secondary(320, 192, 144, 64, t.settings, pressed));
    return root;
  }

  function langSegment(x, label, active) {
    var s = el("div", Object.assign(abs(x, 0, 60, 44), {
      background: active ? COL.textDisplay : "transparent",
      display: "flex", alignItems: "center", justifyContent: "center"
    }));
    s.appendChild(span(label, F_DOTO, 16, active ? COL.black : COL.textSecondary, "0"));
    return s;
  }

  function langSelector(lang, pressed) {
    var wrap = el("div", Object.assign(abs(344, 16, 120, 44), {
      border: "1px solid " + COL.borderVisible,
      borderRadius: "8px",
      overflow: "hidden",
      display: "flex"
    }));
    wrap.appendChild(langSegment(0, "EN", pressed || lang === "en"));
    wrap.appendChild(langSegment(60, "ES", pressed || lang === "es"));
    return wrap;
  }

  function buildMenu(pressed, bg, lang) {
    var t = TXT[lang];
    var root = screenRoot();
    var rows = t.menuRows;
    var ys = [84, 136, 188];
    root.appendChild(backButton(pressed));
    root.appendChild(title(t.menuTitle));
    root.appendChild(langSelector(lang, pressed));
    rows.forEach(function (content, i) {
      var y = ys[i];
      var row = el("div", Object.assign(abs(0, y, 480, 52), {
        background: pressed ? COL.surfaceRaised : "transparent",
        display: "flex", alignItems: "center"
      }));
      row.appendChild(text({
        x: 16, y: 0, h: 52, lh: "52px",
        content: content, font: F_DOTO, size: 20,
        color: pressed ? COL.textDisplay : COL.textPrimary
      }));
      var ch = chevron(">", pressed ? COL.textPrimary : COL.textDisabled);
      ch.style.position = "absolute";
      ch.style.left = "452px";
      ch.style.top = "50%";
      ch.style.transform = "translateY(-50%)";
      row.appendChild(ch);
      root.appendChild(row);
    });
    [84, 136, 188, 240].forEach(function (y) {
      root.appendChild(el("div", Object.assign(abs(16, y, 448, 1), { background: COL.border })));
    });
    return root;
  }

  function jogButton(x, y, dir, content, pressed) {
    var b = el("div", Object.assign(abs(x, y, 216, 128), {
      background: pressed ? COL.textDisplay : "transparent",
      border: "1px solid " + COL.borderVisible,
      borderRadius: "8px",
      display: "flex", flexDirection: "column",
      alignItems: "center", justifyContent: "center", gap: "8px"
    }));
    var color = pressed ? COL.black : COL.textPrimary;
    b.appendChild(arrow(dir, color));
    b.appendChild(span(content, F_DOTO, 16, pressed ? COL.black : COL.textSecondary, "0.06em"));
    return b;
  }

  function buildJog(pressed, bg, lang) {
    var t = TXT[lang];
    var root = screenRoot();
    root.appendChild(backButton(pressed));
    root.appendChild(title(t.jogTitle));
    root.appendChild(jogButton(16, 80, "left", "JOG \u2212", pressed));
    root.appendChild(jogButton(248, 80, "right", "JOG +", pressed));
    root.appendChild(label(16, 232, t.holdToMove, COL.textDisabled));
    return root;
  }

  function buildSpeed(o) {
    var t = TXT[o.lang];
    var root = screenRoot();
    root.appendChild(backButton(o.pressed));
    root.appendChild(title(o.title));

    root.appendChild(label(16, 86, o.leftLabel));
    if (!o.bg) {
      root.appendChild(text({ x: 16, y: 104, w: 140, h: 40, content: String(o.leftVal), font: F_DOTO, size: 24, color: COL.textDisplay }));
    }
    root.appendChild(label(160, 116, t.stepsPerSec, COL.textDisabled));
    root.appendChild(el("div", Object.assign(abs(16, 146, 200, 1), { background: COL.borderVisible })));

    root.appendChild(label(248, 86, o.rightLabel));
    if (!o.bg) {
      root.appendChild(text({ x: 248, y: 104, w: 140, h: 40, content: String(o.rightVal), font: F_DOTO, size: 24, color: COL.textDisplay }));
    }
    root.appendChild(label(392, 116, t.stepsPerSec, COL.textDisabled));
    root.appendChild(el("div", Object.assign(abs(248, 146, 200, 1), { background: COL.borderVisible })));

    if (!o.bg) {
      root.appendChild(text({ x: 16, y: 164, w: 200, h: 20, lh: "20px", content: t.saved, font: F_DOTO, size: 12, color: COL.success }));
    }
    root.appendChild(primary(248, 200, 216, 56, t.save, o.pressed));
    return root;
  }

  var STATE_COLORS = [
    COL.textSecondary,
    COL.warning,
    COL.success,
    COL.textPrimary,
    COL.textDisplay,
    COL.accent
  ];

  function buildEstados(lang) {
    var root = screenRoot(6 * 80 + 16);
    var y = 16;
    TXT[lang].states.forEach(function (s, i) {
      root.appendChild(label(16, y, s.name));
      root.appendChild(text({ x: 16, y: y + 18, w: 448, h: 56, lh: "56px", content: s.text, font: F_DOTO, size: 48, weight: "700", color: STATE_COLORS[i] }));
      y += 80;
    });
    return root;
  }

  function digitKey(x, y, content, pressed) {
    var b = el("div", Object.assign(abs(x, y, 58, 44), {
      background: pressed ? COL.textDisplay : COL.surface,
      border: "1px solid " + COL.borderVisible,
      borderRadius: "8px",
      display: "flex", alignItems: "center", justifyContent: "center"
    }));
    b.appendChild(span(content, F_DOTO, 20, pressed ? COL.black : COL.textPrimary));
    return b;
  }

  function backspaceIcon(color) {
    var svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    svg.setAttribute("width", "20");
    svg.setAttribute("height", "20");
    svg.setAttribute("viewBox", "0 0 20 20");
    svg.appendChild(poly("7,4 3,10 7,16", color));
    svg.appendChild(poly("3,10 17,10", color));
    return svg;
  }

  function backspaceKey(x, y, pressed) {
    var b = el("div", Object.assign(abs(x, y, 58, 44), {
      background: pressed ? COL.textDisplay : COL.surface,
      border: "1px solid " + COL.borderVisible,
      borderRadius: "8px",
      display: "flex", alignItems: "center", justifyContent: "center"
    }));
    b.appendChild(backspaceIcon(pressed ? COL.black : COL.textPrimary));
    return b;
  }

  function okKey(x, y, content, pressed) {
    var b = el("div", Object.assign(abs(x, y, 58, 44), {
      background: pressed ? COL.textSecondary : COL.textDisplay,
      borderRadius: "8px",
      display: "flex", alignItems: "center", justifyContent: "center"
    }));
    b.appendChild(span(content, F_DOTO, 20, COL.black));
    return b;
  }

  function closeIcon(color) {
    var svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    svg.setAttribute("width", "12");
    svg.setAttribute("height", "12");
    svg.setAttribute("viewBox", "0 0 12 12");
    svg.appendChild(poly("2,2 10,10", color));
    svg.appendChild(poly("10,2 2,10", color));
    return svg;
  }

  function closeButton(x, y, pressed) {
    var b = el("div", Object.assign(abs(x, y, 40, 40), {
      background: pressed ? COL.textDisplay : COL.surfaceRaised,
      border: "1px solid " + COL.borderVisible,
      borderRadius: "20px",
      display: "flex", alignItems: "center", justifyContent: "center"
    }));
    b.appendChild(closeIcon(pressed ? COL.black : COL.textPrimary));
    return b;
  }

  function buildTeclado(pressed, bg, lang, overlay) {
    var root = (overlay && !bg)
      ? buildSpeed({ title: TXT[lang].homeSpeedTitle, leftLabel: TXT[lang].fast, rightLabel: TXT[lang].slow, leftVal: 800, rightVal: 100, pressed: false, bg: false, lang: lang })
      : screenRoot();
    var keys = [
      ["1", 148, 66], ["2", 211, 66], ["3", 274, 66],
      ["4", 148, 114], ["5", 211, 114], ["6", 274, 114],
      ["7", 148, 162], ["8", 211, 162], ["9", 274, 162],
      ["0", 211, 210]
    ];
    root.appendChild(el("div", Object.assign(abs(140, 8, 200, 256), {
      background: COL.surface,
      border: "1px solid " + COL.borderVisible,
      borderRadius: "12px"
    })));
    if (!bg) {
      root.appendChild(text({ x: 152, y: 18, w: 132, h: 40, lh: "40px", content: "1200", font: F_DOTO, size: 40, weight: "700", color: COL.textDisplay, align: "right" }));
    }
    root.appendChild(el("div", Object.assign(abs(152, 60, 132, 1), { background: COL.borderVisible })));
    root.appendChild(closeButton(290, 18, pressed));
    keys.forEach(function (k) {
      root.appendChild(digitKey(k[1], k[2], k[0], pressed));
    });
    root.appendChild(backspaceKey(148, 210, pressed));
    root.appendChild(okKey(274, 210, "OK", pressed));
    return root;
  }

  function build(page, opts) {
    opts = opts || {};
    var pressed = !!opts.pressed;
    var bg = !!opts.bg;
    var lang = opts.lang === "es" ? "es" : "en";
    switch (page) {
      case "main": return buildMain(pressed, bg, lang);
      case "menu": return buildMenu(pressed, bg, lang);
      case "jog": return buildJog(pressed, bg, lang);
      case "ajustes": return buildSpeed({ title: TXT[lang].homeSpeedTitle, leftLabel: TXT[lang].fast, rightLabel: TXT[lang].slow, leftVal: 800, rightVal: 100, pressed: pressed, bg: bg, lang: lang });
      case "impresion": return buildSpeed({ title: TXT[lang].printSpeedTitle, leftLabel: TXT[lang].stroke, rightLabel: TXT[lang].squeegee, leftVal: 400, rightVal: 400, pressed: pressed, bg: bg, lang: lang });
      case "teclado": return buildTeclado(pressed, bg, lang, opts.overlay);
      case "estados": return buildEstados(lang);
      default: return buildMain(pressed, bg, lang);
    }
  }

  window.PM_SCREENS = { build: build };
})();
