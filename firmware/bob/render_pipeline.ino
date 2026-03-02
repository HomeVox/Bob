// Extracted from bob.ino for modular structure

// Render pipeline: eye tracking render helpers, matrix/confetti and draw loop

#include "mail_notify.xbm"
#include "alarm_notify.xbm"
#include "alarm_clock_notify.xbm"
#include "washing_machine_notify.xbm"

// ── Wit oogpalet ───────────────────────────────────────────────
static const uint16_t EYE_DARK  = 0x39E7; // donkergrijs rand
static const uint16_t EYE_MID   = 0xFFFF; // wit (TFT_WHITE)
static const uint16_t EYE_SPEC  = 0xBDF7; // lichtgrijs specular highlight
static const uint16_t EYE_BG    = TFT_BLACK;
// ──────────────────────────────────────────────────────────────

// Tekent één oog in blauw-glas stijl op de canvas sprite.
// cx/cy = middelpunt, w/h = halve breedte/hoogte, r = afronding
static void drawEyeGlass(int cx, int cy, int w, int h, int r) {
  // 1. Donkere rand (glow)
  int glow = max(4, w / 12);
  canvas.fillRoundRect(cx - w/2 - glow, cy - h/2 - glow,
                       w + 2*glow, h + 2*glow,
                       r + glow, EYE_DARK);
  // 2. Lichtblauwe vulling
  canvas.fillRoundRect(cx - w/2, cy - h/2, w, h, r, EYE_MID);
  // 3. Specular highlight (bovenste kwart, licht ellips)
  int sw = max(4, w / 4);
  int sh = max(2, h / 6);
  canvas.fillEllipse(cx - w/6, cy - h/4, sw, sh, EYE_SPEC);
}

void updateProximityEyeTracking(uint32_t now) {
  if (!proximityDetected || read_ps_value <= (255 - proximityThreshold)) {
    // No proximity - langzaam return naar centrum (of idle positie)
    if (proximityEyeTracking) {
      proximityEyeX = smoothTo(proximityEyeX, 0, 30, 800);  // Langzamere return (was 400)
      proximityEyeY = smoothTo(proximityEyeY, 0, 30, 800);
      if (abs(proximityEyeX) < 0.5 && abs(proximityEyeY) < 0.5) {
        proximityEyeTracking = false;
        proximityEyeX = 0;
        proximityEyeY = 0;
        DEBUG_VERBOSE_PRINTLN("Proximity tracking stopped - idle look");
      }
    }
    return;
  }

  // Proximity detected - DIRECTE, SNELLE tracking
  proximityEyeTracking = true;

  // Veel snellere update rate voor responsiveness
  uint32_t updateInterval = 15;  // Was 30-100ms, nu 15ms = ~66 FPS tracking!

  if (now - proximityEyeChangeTime > updateInterval) {
    proximityEyeChangeTime = now;

    // Bereken intensity factor (hoger proximity value = dichterbij = sterker effect)
    // Verhoogde gevoeligheid voor groter detectiebereik
    float intensity = clampf((read_ps_value - (255 - proximityThreshold)) / 20.0f, 0.3f, 1.3f);

    // Static variabelen voor beweging tracking
    static uint16_t lastProxValue = 0;
    static float targetX = 0, targetY = 0;
    static uint32_t lastChangeTime = 0;

    // Detecteer proximity verandering (beweging!)
    int proxDelta = (int)read_ps_value - (int)lastProxValue;

    // Bij beweging: BLIJF KIJKEN NAAR WAAR DE VINGER IS
    if (abs(proxDelta) > 2) {  // Gevoelig voor beweging
      lastChangeTime = now;

      // Map proximity waarde naar positie
      // Hoge waarde (dichtbij sensor) = kijk naar beneden/centrum
      // Lage waarde (verder weg) = kijk rond
      float proxNormalized = (read_ps_value - (255 - proximityThreshold)) / (float)proximityThreshold;
      proxNormalized = clampf(proxNormalized, 0.0f, 1.0f);

      if (proxDelta > 0) {
        // Dichterbij = kijk naar vinger toe (naar sensor = beneden)
        targetX = 0; // Centrum horizontaal
        targetY = -MAX_LOOK_Y * 0.3f; // Kijk licht naar beneden waar sensor zit
      } else {
        // Verder weg = kijk vinger na (iets omhoog)
        targetX = 0;
        targetY = MAX_LOOK_Y * 0.2f; // Kijk licht omhoog
      }
    } else if (now - lastChangeTime > 500) {
      // Geen beweging = blijf naar vinger kijken op huidige positie
      // Alleen heel langzaam terug naar centrum als lang geen beweging
      if (now - lastChangeTime > 3000) {
        targetX *= 0.95f;
        targetY *= 0.95f;
      }
    }

    lastProxValue = read_ps_value;

    // Zeer snelle smoothing voor directe response
    float smoothFactor = 0.7f;  // Was 0.55f, nu 0.7f voor nog snellere tracking!

    proximityEyeX += (targetX - proximityEyeX) * smoothFactor;
    proximityEyeY += (targetY - proximityEyeY) * smoothFactor;

    // Scale by intensity
    proximityEyeX = clampf(proximityEyeX * intensity, -MAX_LOOK_X, MAX_LOOK_X);
    proximityEyeY = clampf(proximityEyeY * intensity, -MAX_LOOK_Y, MAX_LOOK_Y);

    // Debug output (minder vaak om performance te verbeteren)
    static uint32_t lastDebugPrint = 0;
    if (now - lastDebugPrint > 200) {  // Elke 200ms i.p.v. elke update
      DEBUG_VERBOSE_PRINTF("Tracking: prox=%d, delta=%d, target=(%.1f,%.1f), eye=(%.1f,%.1f)\n",
                           read_ps_value, proxDelta, targetX, targetY, proximityEyeX, proximityEyeY);
      lastDebugPrint = now;
    }
  }
}


// ---------------- Tekenen ----------------


EyeDims computeEyeDims(const EyeShape& shape, float baseScale){
  EyeDims d;
  d.w = shape.width  * baseScale * screenScale;
  d.h = shape.height * baseScale * screenScale;
  d.r = shape.radius * baseScale * screenScale;
  return d;
}

void initMatrix() {
  // Initialiseer VERTICALE KOLOMMEN met continu vallende cijfers
  for (int i = 0; i < MATRIX_COLS; i++) {
    matrixCols[i].speed = random(15, 80) / 10.0f;  // Verschillende snelheden per kolom: 1.5 tot 8.0 pixels/frame

    // Initialiseer alle cijfers in deze kolom met random posities
    for (int j = 0; j < MATRIX_DIGITS_PER_COL; j++) {
      matrixCols[i].digits[j].digit = random(0, 10) + '0';
      matrixCols[i].digits[j].y = random(-240, 240) + (j * 20);  // Spread cijfers verticaal
      matrixCols[i].digits[j].brightness = random(180, 255);
    }
  }
  DEBUG_PRINTLN("Matrix mode initialized");
}

// Confetti kleuren palette
const uint16_t confettiColors[] = {
  TFT_RED, TFT_BLUE, TFT_GREEN, TFT_YELLOW,
  TFT_MAGENTA, TFT_CYAN, TFT_ORANGE, TFT_PINK,
  TFT_PURPLE, TFT_GREENYELLOW, TFT_GOLD, TFT_SKYBLUE
};
const int NUM_CONFETTI_COLORS = sizeof(confettiColors) / sizeof(confettiColors[0]);
const uint16_t snowColors[] = {
  TFT_WHITE, 0xE71C, 0xDEFB, 0xD6BA
};
const int NUM_SNOW_COLORS = sizeof(snowColors) / sizeof(snowColors[0]);

void initConfetti() {
  // Initialize ALLE particles als INACTIEF - we spawnen ze geleidelijk
  for (int i = 0; i < MAX_CONFETTI; i++) {
    confetti[i].active = false;
    confetti[i].settled = false;
  }

  confettiStartTime = millis();
  lastConfettiSpawn = millis();
  activeConfettiCount = 0;
  avgFillHeight = 240.0f;  // Start onderaan

  DEBUG_PRINTLN("Confetti mode initialized");
}

// Spawn een nieuwe confetti particle (SNOW mode - van boven)
void spawnConfettiSnow(int index) {
  confetti[index].x = random(0, 320);  // Start positie over hele breedte
  confetti[index].y = random(-80, -20);  // Start net boven scherm
  confetti[index].vx = random(-12, 13) / 10.0f;  // Langzame drift: -1.2 tot +1.2
  confetti[index].vy = random(5, 16) / 10.0f;    // Zachte val: 0.5 tot 1.5
  confetti[index].rotation = random(0, 360);
  confetti[index].rotationSpeed = random(-10, 11) / 10.0f;
  confetti[index].color = snowColors[random(0, NUM_SNOW_COLORS)];
  confetti[index].size = random(3, 8);
  confetti[index].shape = random(0, 4);
  confetti[index].active = true;
  confetti[index].settled = false;
  confetti[index].landHeight = avgFillHeight + random(-40, 40);
  // NOTE: activeConfettiCount incremented in calling code to avoid double-increment
}

// Spawn vanuit KANONNEN (linksonder + rechtsonder) - EXPLOSIEF!
void spawnConfettiCannon(int index) {
  // Kies willekeurig links of rechts kanon
  bool leftCannon = random(0, 2) == 0;

  if (leftCannon) {
    // LINKS KANON (linksonder: x=0, y=240) - BOOOOM! ðŸ’¥
    confetti[index].x = random(0, 40);  // Start linksonder hoek
    confetti[index].y = 240;  // Precies onderaan

    // EXPLOSIE - Moet HELE scherm bereiken (320px breed, 240px hoog)
    // Sommige confetti gaan recht omhoog, andere diagonaal naar rechts
    confetti[index].vx = random(-20, 220) / 10.0f;   // -2 tot 22 (ook naar links voor spreiding!)
    confetti[index].vy = random(-250, -70) / 10.0f;  // -25 tot -7 omhoog (SUPER EXPLOSIEF!)

  } else {
    // RECHTS KANON (rechtsonder: x=320, y=240) - BOOOOM! ðŸ’¥
    confetti[index].x = random(280, 320);  // Start rechtsonder hoek
    confetti[index].y = 240;  // Precies onderaan

    // EXPLOSIE - Moet HELE scherm bereiken
    // Sommige confetti gaan recht omhoog, andere diagonaal naar links
    confetti[index].vx = random(-220, 20) / 10.0f;   // -22 tot 2 (ook naar rechts voor spreiding!)
    confetti[index].vy = random(-250, -70) / 10.0f;  // -25 tot -7 omhoog (SUPER EXPLOSIEF!)
  }

  confetti[index].rotation = random(0, 360);
  confetti[index].rotationSpeed = random(-80, 80) / 10.0f;  // Wilde rotatie!
  confetti[index].color = confettiColors[random(0, NUM_CONFETTI_COLORS)];
  confetti[index].size = random(5, 14);  // Iets groter voor zichtbaarheid bij hoge snelheid
  confetti[index].shape = random(0, 4);
  confetti[index].active = true;
  confetti[index].settled = false;
  confetti[index].landHeight = random(0, 240);  // Kan overal landen
  // NOTE: activeConfettiCount incremented in calling code to avoid double-increment
}

void updateConfetti() {
  uint32_t now = millis();
  uint32_t elapsed = now - confettiStartTime;

  // Confetti stopt NIET automatisch - alleen door schudden!
  // Wel stoppen met spawnen als alle particles actief zijn
  if (activeConfettiCount >= MAX_CONFETTI) {
    // Scherm is VOL - stop met spawnen maar blijf draaien
    if (elapsed % 10000 == 0) {  // Elke 10 seconden
      DEBUG_VERBOSE_PRINTLN("Confetti pool full");
    }
  }

  // ===== CONTINUOUS SPAWNING =====
  // Spawn rate verschilt per mode
  int spawnInterval = 100;

  if (confettiMode == CONFETTI_CANNONS) {
    // CANNON MODE - 10x MEER! MEGA VUURKRACHT! ðŸ’¥ðŸ’¥ðŸ’¥
    spawnInterval = 1;  // 1ms = ~1000 particles/sec! WAANZIN!
  } else {
    // SNOW MODE - Geleidelijke opbouw
    if (elapsed < 5000) {
      spawnInterval = 120;  // Langzaam begin (0-5 sec)
    } else if (elapsed < 15000) {
      spawnInterval = 60;   // Sneller (5-15 sec)
    } else if (elapsed < 30000) {
      spawnInterval = 40;   // Veel sneller (15-30 sec)
    } else if (elapsed < 60000) {
      spawnInterval = 25;   // Maximum snelheid (30-60 sec)
    } else {
      spawnInterval = 20;   // TURBO mode voor volledig bedekt scherm!
    }
  }

  // Spawn nieuwe particles continu (tot MAX bereikt)
  if (activeConfettiCount < MAX_CONFETTI && (now - lastConfettiSpawn > spawnInterval)) {
    // Vind een inactive particle en spawn het
    for (int i = 0; i < MAX_CONFETTI; i++) {
      if (!confetti[i].active) {
        // Spawn op basis van mode
        if (confettiMode == CONFETTI_SNOW) {
          spawnConfettiSnow(i);
        } else if (confettiMode == CONFETTI_CANNONS) {
          spawnConfettiCannon(i);
        }
        // CRITICAL FIX: Increment counter only in calling code (not in spawn functions)
        activeConfettiCount++;
        lastConfettiSpawn = now;

        // Debug elke 50 spawns
        if (activeConfettiCount % 50 == 0) {
          DEBUG_VERBOSE_PRINTF("Confetti: %d/%d\n", activeConfettiCount, MAX_CONFETTI);
        }
        break;
      }
    }
  }

  // Update elke 20ms (~50 FPS)
  if (now - lastConfettiUpdate < 20) return;
  lastConfettiUpdate = now;

  // Update alle ACTIEVE particles
  for (int i = 0; i < MAX_CONFETTI; i++) {
    if (!confetti[i].active) continue;

    // Als particle al is "settled" (ligt op de grond), skip physics
    if (confetti[i].settled) continue;

    // Update positie
    confetti[i].x += confetti[i].vx;
    confetti[i].y += confetti[i].vy;

    // Update rotatie (alleen als nog aan het vallen)
    confetti[i].rotation += confetti[i].rotationSpeed;
    if (confetti[i].rotation > 360) confetti[i].rotation -= 360;
    if (confetti[i].rotation < 0) confetti[i].rotation += 360;

    // Zwaartekracht effect (sterker voor cannon mode)
    if (confettiMode == CONFETTI_CANNONS) {
      confetti[i].vy += 0.4f;  // Sterkere zwaartekracht voor mooie boog
    } else if (confettiMode == CONFETTI_SNOW) {
      confetti[i].vy += 0.03f;  // Zachte versnelling voor sneeuw
      confetti[i].vy = clampf(confetti[i].vy, 0.4f, 2.2f);
    } else {
      confetti[i].vy += 0.2f;  // Normale zwaartekracht voor snow
    }

    // Luchtweerstand op horizontale beweging (minder voor cannons = verder vliegen)
    if (confettiMode == CONFETTI_CANNONS) {
      confetti[i].vx *= 0.98f;  // Minder weerstand = blijft verder vliegen
    } else if (confettiMode == CONFETTI_SNOW) {
      confetti[i].vx += random(-4, 5) / 100.0f;  // Lichte windjitter
      confetti[i].vx = clampf(confetti[i].vx, -1.4f, 1.4f);
      confetti[i].vx *= 0.995f;
    } else {
      confetti[i].vx *= 0.97f;  // Normale weerstand voor snow
    }

    // CANNON MODE: Laat confetti vrij vliegen over hele scherm (geen bounce!)
    if (confettiMode == CONFETTI_CANNONS) {
      // Check of confetti buiten zichtbaar gebied is - deactiveer
      if (confetti[i].y > 260 || confetti[i].y < -50 ||
          confetti[i].x < -100 || confetti[i].x > 420) {
        // Te ver buiten scherm - recycle deze confetti
        confetti[i].active = false;
        activeConfettiCount--;
        continue;
      }
      // GEEN BOUNCE - laat confetti vrij door scherm vliegen!

    } else if (confetti[i].vy < -8.0f) {
      // CLEAR MODE: Snel omhoog = weggeblazen
      // Als ver buiten scherm: deactiveer
      if (confetti[i].y < -100 || confetti[i].y > 300 ||
          confetti[i].x < -50 || confetti[i].x > 370) {
        confetti[i].active = false;
        activeConfettiCount--;
        continue;
      }
      // Weggeblazen confetti bounced NIET

    } else {
      // SNOW MODE: Normale vallende confetti bounced aan zijkanten
      if (confetti[i].x < 0) {
        confetti[i].x = 0;
        confetti[i].vx = abs(confetti[i].vx) * 0.5f;  // Bounce met energy loss
      }
      if (confetti[i].x > 320) {
        confetti[i].x = 320;
        confetti[i].vx = -abs(confetti[i].vx) * 0.5f;
      }
    }

    // Landing logica - ALLEEN voor SNOW mode!
    if (confettiMode == CONFETTI_SNOW) {
      // Check of confetti zijn land-hoogte bereikt heeft
      if (confetti[i].y >= confetti[i].landHeight) {
        confetti[i].y = confetti[i].landHeight;  // Zet precies op land hoogte
        confetti[i].settled = true;
        confetti[i].vx = 0;
        confetti[i].vy = 0;
        confetti[i].rotationSpeed = 0;

        // Verhoog de algemene fill height (confetti stapelt zich op!)
        avgFillHeight -= 0.3f;  // Ga omhoog - kleiner getal = sneller vol!
        if (avgFillHeight < -20) avgFillHeight = -20;  // Mag iets boven scherm voor volle bedekking

        // Debug logging elke 20 particles
        static int settledCount = 0;
        settledCount++;
        if (settledCount % 20 == 0) {
          int fillPercent = ((240.0f - avgFillHeight) / 240.0f) * 100;
          DEBUG_VERBOSE_PRINTF("%d settled | Fill: %d%% | Height: %.1fpx\n",
                               settledCount, fillPercent, avgFillHeight);
        }
      }
    }
    // CANNON mode: confetti blijft vallen tot ze buiten scherm zijn (geen landing)
  }
}

void drawConfetti() {
  // Teken alle confetti - laat ze gewoon over elkaar heen vallen
  for (int i = 0; i < MAX_CONFETTI; i++) {
    if (!confetti[i].active) continue;

    int cx = (int)confetti[i].x;
    int cy = (int)confetti[i].y;
    int size = confetti[i].size;
    uint16_t color = confetti[i].color;

    // SNOW mode: teken echte sneeuwvlokken
    if (confettiMode == CONFETTI_SNOW) {
      int r = max(1, size / 2);
      canvas.fillCircle(cx, cy, r, color);
      canvas.drawFastHLine(cx - r - 1, cy, (r + 1) * 2 + 1, color);
      canvas.drawFastVLine(cx, cy - r - 1, (r + 1) * 2 + 1, color);

      // Kleine variatie in vlokvorm
      if (size >= 5 || confetti[i].shape % 2 == 0) {
        canvas.drawLine(cx - r, cy - r, cx + r, cy + r, color);
        canvas.drawLine(cx - r, cy + r, cx + r, cy - r, color);
      }
      continue;
    }

    // Teken confetti met verschillende shapes (zoals in demo)
    switch (confetti[i].shape) {
      case 0: // Vierkant
        canvas.fillRect(cx - size/2, cy - size/2, size, size, color);
        break;

      case 1: // Cirkel
        canvas.fillCircle(cx, cy, size/2, color);
        break;

      case 2: // Driehoek
        canvas.fillTriangle(
          cx, cy - size/2,
          cx - size/2, cy + size/2,
          cx + size/2, cy + size/2,
          color
        );
        break;

      case 3: // Ster (klein kruis)
        canvas.fillRect(cx - size/2, cy - 1, size, 2, color);  // Horizontaal
        canvas.fillRect(cx - 1, cy - size/2, 2, size, color);  // Verticaal
        break;
    }
  }
}

static bool drawNotifyFullScreen() {
  if (eyeNotifyVisual == EyeNotifyVisual::None) return false;
  uint32_t now = millis();
  if (now >= eyeNotifyUntil) {
    eyeNotifyVisual = EyeNotifyVisual::None;
    return false;
  }

  canvas.fillScreen(TFT_BLACK);

  auto drawXbm = [&](const unsigned char* bits, int w, int h, uint16_t col) {
    int stride = (w + 7) / 8;
    int ox = (M5.Display.width() - w) / 2;
    int oy = (M5.Display.height() - h) / 2;
    for (int y = 0; y < h; ++y) {
      const unsigned char* row = bits + (y * stride);
      for (int x = 0; x < w; ++x) {
        if (row[x >> 3] & (1 << (x & 7))) {
          canvas.drawPixel(ox + x, oy + y, col);
        }
      }
    }
  };

  switch (eyeNotifyVisual) {
    case EyeNotifyVisual::Mail: {
      // Exact user SVG: mail-alt-svgrepo-com.svg
      drawXbm((const unsigned char*)mail_notify_bits, mail_notify_width, mail_notify_height, TFT_WHITE);
      break;
    }
    case EyeNotifyVisual::Alarm: {
      int blink = ((now / 180) % 2) ? 1 : 0;
      uint16_t col = blink ? TFT_RED : TFT_WHITE;
      // Exact user SVG: alarm-o-svgrepo-com.svg
      drawXbm((const unsigned char*)alarm_notify_bits, alarm_notify_width, alarm_notify_height, col);
      break;
    }
    case EyeNotifyVisual::AlarmClock: {
      int blink = ((now / 250) % 2) ? 1 : 0;
      uint16_t col = blink ? TFT_YELLOW : TFT_WHITE;
      drawXbm((const unsigned char*)alarm_clock_notify_bits,
              alarm_clock_notify_width, alarm_clock_notify_height, col);
      break;
    }
    case EyeNotifyVisual::WashingMachine: {
      drawXbm((const unsigned char*)washing_machine_notify_bits,
              washing_machine_notify_width, washing_machine_notify_height, TFT_WHITE);
      break;
    }
    default:
      break;
  }
  canvas.pushSprite(0,0);
  return true;
}

static void drawSegmentDigit(int x, int y, int w, int h, int d) {
  static const uint8_t SEG[10] = {
    0b1111110, // 0
    0b0110000, // 1
    0b1101101, // 2
    0b1111001, // 3
    0b0110011, // 4
    0b1011011, // 5
    0b1011111, // 6
    0b1110000, // 7
    0b1111111, // 8
    0b1111011  // 9
  };
  if (d < 0 || d > 9) return;

  uint8_t s = SEG[d];
  int t = max(5, w / 5);   // Dikker en ronder voor Bob-stijl
  int r = max(3, t / 2);
  int mid = y + (h / 2) - (t / 2);
  uint16_t colOuter = EYE_DARK;
  uint16_t colInner = 0x3186; // warm donkergrijs i.p.v. hard zwart
  int inset = 1;

  auto seg = [&](int sx, int sy, int sw, int sh) {
    canvas.fillRoundRect(sx, sy, sw, sh, r, colOuter);
    canvas.fillRoundRect(sx + inset, sy + inset,
                         max(1, sw - 2 * inset), max(1, sh - 2 * inset),
                         max(1, r - 1), colInner);
  };

  if (s & 0b1000000) seg(x + t, y, w - 2 * t, t);               // a
  if (s & 0b0100000) seg(x + w - t, y + t, t, (h / 2) - t);     // b
  if (s & 0b0010000) seg(x + w - t, y + h / 2, t, (h / 2) - t); // c
  if (s & 0b0001000) seg(x + t, y + h - t, w - 2 * t, t);       // d
  if (s & 0b0000100) seg(x, y + h / 2, t, (h / 2) - t);         // e
  if (s & 0b0000010) seg(x, y + t, t, (h / 2) - t);             // f
  if (s & 0b0000001) seg(x + t, mid, w - 2 * t, t);             // g
}

static void drawClockModeScreen(float lookX, float lookY) {
  canvas.fillScreen(TFT_BLACK);
  (void)lookX;
  (void)lookY;

  // Tijd ophalen
  time_t now = time(nullptr);
  struct tm tmNow = {};
  if (now > 1700000000UL) {
    localtime_r(&now, &tmNow);
  } else {
    uint32_t s = millis() / 1000;
    tmNow.tm_hour = (s / 3600) % 24;
    tmNow.tm_min  = (s / 60) % 60;
  }
  int hh = tmNow.tm_hour;
  int mm = tmNow.tm_min;

  // Vaste klok-layout: duidelijk grotere ogen, geen beweging.
  EyeDims eye = computeEyeDims(neutralShape, 1.2f);
  EyeDims L = eye;
  EyeDims R = eye;
  int gap = 14;
  int cxL = centerX - (iround(L.w) / 2) - (gap / 2);
  int cyL = centerY;
  int cxR = centerX + (iround(R.w) / 2) + (gap / 2);
  int cyR = centerY;

  // Teken neutrale Bob-ogen ZONDER flare/specular.
  int panelWL = iround(L.w), panelHL = iround(L.h), rrL = iround(L.r);
  int panelWR = iround(R.w), panelHR = iround(R.h), rrR = iround(R.r);
  canvas.fillRoundRect(cxL - panelWL/2 - glowOffset/2, cyL - panelHL/2 - glowOffset/2,
                       panelWL + glowOffset, panelHL + glowOffset, rrL + glowOffset/2, TFT_DARKGREY);
  canvas.fillRoundRect(cxR - panelWR/2 - glowOffset/2, cyR - panelHR/2 - glowOffset/2,
                       panelWR + glowOffset, panelHR + glowOffset, rrR + glowOffset/2, TFT_DARKGREY);
  canvas.fillRoundRect(cxL - panelWL/2, cyL - panelHL/2, panelWL, panelHL, rrL, TFT_WHITE);
  canvas.fillRoundRect(cxR - panelWR/2, cyR - panelHR/2, panelWR, panelHR, rrR, TFT_WHITE);

  // Tijd in de ogen als gewone cijfers (geen digital/segment look).
  char hBuf[3];
  char mBuf[3];
  snprintf(hBuf, sizeof(hBuf), "%02d", hh);
  snprintf(mBuf, sizeof(mBuf), "%02d", mm);
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextColor(TFT_DARKGREY, TFT_WHITE); // zelfde lijnkleur als Bob-glow
  canvas.setFont(&fonts::Font6); // grote, smooth, niet-digitale font
  canvas.setTextSize(1);
  canvas.drawString(hBuf, cxL, cyL + 2);
  canvas.drawString(mBuf, cxR, cyR + 2);

  drawNotificationBar();
  canvas.pushSprite(0, 0);
}



void drawEyes(float lookX, float lookY){
  // Skip all rendering if camera streaming is active (including matrix mode)
  if (cameraStreaming) {
    return; // Let camera handle the display
  }

  if (clockMode) {
    drawClockModeScreen(lookX, lookY);
    return;
  }

  if (drawNotifyFullScreen()) {
    return;
  }

  canvas.fillScreen(TFT_BLACK);

  // Alle emoties via shape-systeem (currentShape tweening)

  // CONFETTI MODE - Vallende gekleurde confetti over de ogen heen!
  if (confettiMode != CONFETTI_NONE) {
    updateConfetti();
    // Teken eerst de ogen normaal
    // (confetti wordt later over de ogen heen getekend)
  }

  // MATRIX MODE - Vallende HELE RIJEN met witte cijfers
  if (matrixMode) {
    uint32_t now = millis();

    // Update matrix elke 50ms (alleen als niet aan het streamen)
    if (now - lastMatrixUpdate > 50) {
      lastMatrixUpdate = now;

      // Update ALLE CIJFERS in elke kolom (continu vallend)
      for (int i = 0; i < MATRIX_COLS; i++) {
        // Update elk cijfer in deze kolom
        for (int j = 0; j < MATRIX_DIGITS_PER_COL; j++) {
          matrixCols[i].digits[j].y += matrixCols[i].speed;  // Val met kolom snelheid

          // Als cijfer buiten scherm valt, reset naar boven
          if (matrixCols[i].digits[j].y > 260) {
            matrixCols[i].digits[j].y = -20;  // Reset naar net boven scherm
            matrixCols[i].digits[j].digit = random(0, 10) + '0';  // Nieuw random cijfer
            matrixCols[i].digits[j].brightness = random(180, 255);  // Nieuwe brightness
          }
        }
      }
    }

    // Teken VERTICALE KOLOMMEN over volledige schermbreedte.
    // Forceer kleine bitmapfont; drawClockModeScreen() gebruikt een grotere Font4.
    canvas.setTextFont(1);
    canvas.setTextSize(1);
    canvas.setTextDatum(TL_DATUM);

    for (int i = 0; i < MATRIX_COLS; i++) {
      int x = i * 8;  // X positie van deze kolom (40 kolommen Ã— 8px = 320px)

      // Teken alle cijfers in deze kolom (elk met eigen Y positie en brightness)
      for (int j = 0; j < MATRIX_DIGITS_PER_COL; j++) {
        int y = (int)matrixCols[i].digits[j].y;

        if (y >= 0 && y < 240) {  // Alleen tekenen als op scherm
          uint16_t color = canvas.color565(
            0,
            matrixCols[i].digits[j].brightness,
            0
          );
          canvas.setTextColor(color);
          canvas.drawChar(matrixCols[i].digits[j].digit, x, y);
        }
      }
    }

    // Teken ZWARTE OGEN als maskers over de matrix (met animatie)
    int centerX = 160;
    int centerY = 120;

    // Bereken 3D parallax effect (zoals in normale rendering) maar met 0.6 scale
    float matrixScale = 0.6f;  // Ogen zijn 60% van normale grootte in matrix mode
    float par = (lookX / MAX_LOOK_X) * PARALLAX_RATIO;
    float parallaxMultiplier = 2.5f;
    float vScale = 1.0f - (fabsf(lookY) / MAX_LOOK_Y) * 0.30f;
    vScale = clampf(vScale, 0.65f, 1.0f);
    float lBase = depthScale * (1.0f - asym3D) * (1.0f + par * parallaxMultiplier) * matrixScale * vScale;
    float rBase = depthScale * (1.0f + asym3D) * (1.0f - par * parallaxMultiplier) * matrixScale * vScale;
    lBase = clampf(lBase, 0.3f, 1.1f);  // Aangepaste limieten voor kleinere ogen
    rBase = clampf(rBase, 0.3f, 1.1f);

    // Bereken oog dimensies met huidige shape animatie
    EyeDims L = computeEyeDims(currentShape, lBase);
    EyeDims R = computeEyeDims(currentShape, rBase);

    // Apply sound reaction scaling (zoals normaal)
    if (soundReactionActive) {
      L.w *= SOUND_EYE_SCALE;
      L.h *= SOUND_EYE_SCALE;
      R.w *= SOUND_EYE_SCALE;
      R.h *= SOUND_EYE_SCALE;
    }

    int leftEyeX = centerX - eyeSpacing/2;
    int rightEyeX = centerX + eyeSpacing/2;

    // Teken zwarte gevulde ellipsen voor de ogen (inverse masker)
    canvas.fillEllipse(leftEyeX, centerY, L.w, L.h, TFT_BLACK);
    canvas.fillEllipse(rightEyeX, centerY, R.w, R.h, TFT_BLACK);

    drawNotificationBar();
    canvas.pushSprite(0,0);
    return;
  }

  // Check if sound reaction is active and should expire
  if (soundReactionActive && (millis() - soundReactionStartTime > SOUND_REACTION_DURATION)) {
    soundReactionActive = false;
    DEBUG_VERBOSE_PRINTLN("Sound reaction ended");
  }

  // STERKER 3D EFFECT - ene oog wordt kleiner, andere groter
  float par   = (lookX / MAX_LOOK_X) * PARALLAX_RATIO;

  // Verticaal: beide ogen samen kleiner bij omhoog/omlaag kijken
  float vScale = 1.0f - (fabsf(lookY) / MAX_LOOK_Y) * 0.30f;
  vScale = clampf(vScale, 0.65f, 1.0f);

  // Verhoog het verschil tussen ogen dramatisch
  float parallaxMultiplier = 2.5f; // Extra multiplier voor sterker effect
  float lBase = depthScale * (1.0f - asym3D) * (1.0f + par * parallaxMultiplier) * vScale;
  float rBase = depthScale * (1.0f + asym3D) * (1.0f - par * parallaxMultiplier) * vScale;

  // Clamp zodat ogen niet te klein of groot worden
  lBase = clampf(lBase, 0.5f, 1.8f);
  rBase = clampf(rBase, 0.5f, 1.8f);

  EyeDims L = computeEyeDims(currentShape, lBase);
  EyeDims R = computeEyeDims(currentShape, rBase);

  // Neutraal: ogen als geheel iets groter.
  if (currentPersonality == PersonalityExtended::Neutral) {
    L.w *= 1.08f; L.h *= 1.08f; L.r *= 1.08f;
    R.w *= 1.08f; R.h *= 1.08f; R.r *= 1.08f;
  }

  // Echte knipoog: één oog dicht, ander open tijdens Wink personality.
  if (currentPersonality == PersonalityExtended::Wink) {
    L.h = max(6.0f, L.h * 0.18f);
    L.r = max(4.0f, L.r * 0.35f);
    L.w *= 1.04f;
  }

  // Love: grotere ogen met zachte hartslag-puls.
  if (currentPersonality == PersonalityExtended::Love) {
    float t = millis() / 1000.0f;
    float beat = 1.0f + 0.08f * fabsf(sinf(t * 3.6f));
    float base = 1.14f;
    float s = base * beat;
    L.w *= s; L.h *= s; L.r *= s;
    R.w *= s; R.h *= s; R.r *= s;
  }

  // Surprised: neutrale vorm behouden, maar groter.
  if (currentPersonality == PersonalityExtended::Surprised) {
    L = computeEyeDims(neutralShape, lBase);
    R = computeEyeDims(neutralShape, rBase);
    const float s = 1.18f;
    L.w *= s; L.h *= s; L.r *= s;
    R.w *= s; R.h *= s; R.r *= s;
  }

  // Apply sound reaction scaling (20% bigger)
  if (soundReactionActive) {
    L.w *= SOUND_EYE_SCALE;
    L.h *= SOUND_EYE_SCALE;
    L.r *= SOUND_EYE_SCALE;
    R.w *= SOUND_EYE_SCALE;
    R.h *= SOUND_EYE_SCALE;
    R.r *= SOUND_EYE_SCALE;
  }

  float glowL = max(4.0f, L.w / 12.0f);
  float glowR = max(4.0f, R.w / 12.0f);
  float allowed = (float)eyeSpacing - glowL - glowR - GAP_MARGIN_PX;
  float currentHalfSum = 0.5f * (L.w + R.w);
  float kfit = (currentHalfSum <= allowed) ? 1.0f : clampf(allowed / currentHalfSum, 0.6f, 1.0f);
  L.w*=kfit; L.h*=kfit; L.r*=kfit;
  R.w*=kfit; R.h*=kfit; R.r*=kfit;

  int lx = centerX - eyeSpacing/2 + iround(skewXL);
  int ly = centerY + iround(tiltOffsetL);
  int rx = centerX + eyeSpacing/2 + iround(skewXR);
  int ry = centerY + iround(tiltOffsetR);


  // Eye rendering - wit met grijze glow (originele stijl)
  canvas.fillRoundRect(iround(lx - L.w/2 - glowOffset/2 + lookX), iround(ly - L.h/2 - glowOffset/2 + lookY),
                       iround(L.w + glowOffset), iround(L.h + glowOffset), iround(L.r + glowOffset/2), TFT_DARKGREY);
  canvas.fillRoundRect(iround(rx - R.w/2 - glowOffset/2 + lookX), iround(ry - R.h/2 - glowOffset/2 + lookY),
                       iround(R.w + glowOffset), iround(R.h + glowOffset), iround(R.r + glowOffset/2), TFT_DARKGREY);
  canvas.fillRoundRect(iround(lx - L.w/2 + lookX), iround(ly - L.h/2 + lookY), iround(L.w), iround(L.h), iround(L.r), TFT_WHITE);
  canvas.fillRoundRect(iround(rx - R.w/2 + lookX), iround(ry - R.h/2 + lookY), iround(R.w), iround(R.h), iround(R.r), TFT_WHITE);

  // Status text display - ENABLED
  // Tekst weggehaald - alleen ogen tonen

  // Teken confetti OVER de ogen heen (als confetti mode actief is)
  if (confettiMode != CONFETTI_NONE) {
    drawConfetti();
  }

  drawNotificationBar();
  canvas.pushSprite(0,0);
}


