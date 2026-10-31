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

// --- REACTIES OP GELUID ---

// Geluid gehoord - alert
void onSoundDetected() {
  BobMove::alertUp();
  // Ogen: groot, kijkt richting geluid
}

// --- SPECIALE EVENTS ---

// Iets gezien (proximity)
void onProximity() {
  BobMove::alertUp();
}

}  // namespace BobExpress

#endif // BOB_PERSONALITY_H
