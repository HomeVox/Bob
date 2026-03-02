// bob_personality.h - Bob's karakter en reacties
// Combineert ogen + geluid + beweging tot expressies

#ifndef BOB_PERSONALITY_H
#define BOB_PERSONALITY_H

struct EyeShape;

void setTarget(float x, float y);
void startEmotionTween(const EyeShape& to, uint32_t dur = 220);
void setEmotionInstant(const EyeShape& s);

extern const EyeShape baseShape;
extern const EyeShape wakeShape;
extern const EyeShape sleepShape;
extern const EyeShape angryShape;

namespace BobMove {
  inline void lookAt(float x, float y) { setTarget(x, y); }
  inline void alertUp() { startEmotionTween(wakeShape, 200); setTarget(0, -10); }
  inline void droop() { startEmotionTween(sleepShape, 200); setTarget(0, 10); }
  inline void tiltCurious() { setTarget(10, -6); }
  inline void shake() { startEmotionTween(angryShape, 120); setTarget(0, 0); }
  inline void lookDown() { setTarget(0, 12); }
  inline void lookCenter() { setTarget(0, 0); startEmotionTween(baseShape, 200); }
}

// ============================================
// EXPRESSIE COMBINATIES
// ============================================
namespace BobExpress {

// --- BEGROETINGEN ---

// "Oh, jij bent er" - subtiele begroeting
void greetSubtle() {
  BobMove::alertUp();
}

// Blije begroeting (zeldzaam, hoge affection)
void greetHappy() {
  BobMove::alertUp();
  // Ogen: groot en blij
}

// --- REACTIES OP AANRAKING ---

// Aangeraakt - reageert op mood
void onTouch() {
  BobMove::alertUp();
}

// Geschud worden - altijd boos!
void onShake() {
  BobMove::shake();
  // Ogen: boos/vierkant
}

// --- REACTIES OP GELUID ---

// Geluid gehoord - alert
void onSoundDetected() {
  BobMove::alertUp();
  // Ogen: groot, kijkt richting geluid
}

// --- LUISTEREN / DENKEN ---

// Start luisteren (voice assistant actief)
void onListenStart() {
  BobMove::alertUp();
  // Ogen: groot, alert
}

// Aan het nadenken
void onThinking() {
  // Ogen: bewegende dots of kijkt omhoog
}

// Klaar met antwoord
void onResponseReady() {
  // Ogen: terug naar normaal
}

// --- SLAPEN / WAKKER ---

// Gaat slapen
void goToSleep() {
  BobMove::droop();
  delay(500);
  // Ogen: langzaam dicht
}

// Wordt wakker
void wakeUp() {
  BobMove::alertUp();
  // Ogen: langzaam open
}

// --- AUTONOME ACTIES ---

// Nieuwsgierig rondkijken
void randomLookAround() {
  int dir = random(-60, 60);
  BobMove::lookAt(dir, random(-20, 10));
}

// --- SPECIALE EVENTS ---

// Startup
void onStartup() {
  delay(500);
  BobMove::lookCenter();
}

// Error/probleem
void onError() {
  BobMove::shake();
}

// Iets gezien (proximity)
void onProximity() {
  BobMove::alertUp();
}

// Owner komt thuis
void onOwnerHome() {
  BobMove::alertUp();
}

// Owner gaat weg
void onOwnerLeave() {
  BobMove::lookAt(30, 0);  // Kijkt naar deur
  delay(1000);
  BobMove::lookDown();
}

}  // namespace BobExpress

#endif // BOB_PERSONALITY_H
