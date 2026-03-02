// ==========================================
// Bob Advanced Eye Expressions
// More emotions, animations, and effects
// ==========================================

#ifndef BOB_EXPRESSIONS_H
#define BOB_EXPRESSIONS_H

// Define EyeShape if not already defined
#ifndef EYESHAPE_DEFINED
#define EYESHAPE_DEFINED
struct EyeShape {
  int width, height, radius;
};
#endif

// ==========================================
// Extended Personality System
// ==========================================

enum class PersonalityExtended : uint8_t {
  Neutral = 0,
  Happy = 1,
  Sad = 2,
  Thinking = 3,
  Excited = 4,
  Confused = 5,
  Angry = 6,
  Scared = 7,
  Sleepy = 8,
  Wink = 9,
  Love = 10,
  Surprised = 11,
  Suspicious = 12,
  Dizzy = 13,
  Bored = 14,
  Proud = 15,
  Shy = 16,
  Flirty = 17
};

// ==========================================
// Extended Eye Shapes
// ==========================================

// Original shapes (maintained for compatibility)
extern const EyeShape neutralShape;
extern const EyeShape happyShape;
extern const EyeShape sadShape;
extern const EyeShape thinkingShape;
extern const EyeShape excitedShape;
extern const EyeShape confusedShape;

// New advanced shapes
extern const EyeShape angryShape;
extern const EyeShape scaredShape;
extern const EyeShape sleepyShape;
extern const EyeShape winkShape;
extern const EyeShape loveShape;
extern const EyeShape surprisedShape;
extern const EyeShape suspiciousShape;
extern const EyeShape dizzyShape;
extern const EyeShape boredShape;
extern const EyeShape proudShape;
extern const EyeShape shyShape;
extern const EyeShape flirtyShape;

// ==========================================
// Animation Sequences
// ==========================================

enum class AnimationSequence : uint8_t {
  None = 0,
  LookAround = 1,
  FallAsleep = 2,
  WakeUp = 3,
  Curious = 4,
  Blink = 5,
  DoubleTake = 6,
  HeartBeat = 7,
  Spiral = 8,
  Bounce = 9
};

struct AnimationState {
  AnimationSequence current = AnimationSequence::None;
  uint32_t startTime = 0;
  uint32_t duration = 0;
  int step = 0;
  float progress = 0.0f;
  bool active = false;
  bool loop = false;
};

// ==========================================
// Expression Helper Functions
// ==========================================

inline EyeShape getExtendedPersonalityShape(PersonalityExtended p) {
  switch (p) {
    case PersonalityExtended::Neutral:     return neutralShape;
    case PersonalityExtended::Happy:       return happyShape;
    case PersonalityExtended::Sad:         return sadShape;
    case PersonalityExtended::Thinking:    return thinkingShape;
    case PersonalityExtended::Excited:     return excitedShape;
    case PersonalityExtended::Confused:    return confusedShape;
    case PersonalityExtended::Angry:       return angryShape;
    case PersonalityExtended::Scared:      return scaredShape;
    case PersonalityExtended::Sleepy:      return sleepyShape;
    case PersonalityExtended::Wink:        return winkShape;
    case PersonalityExtended::Love:        return loveShape;
    case PersonalityExtended::Surprised:   return surprisedShape;
    case PersonalityExtended::Suspicious:  return suspiciousShape;
    case PersonalityExtended::Dizzy:       return dizzyShape;
    case PersonalityExtended::Bored:       return boredShape;
    case PersonalityExtended::Proud:       return proudShape;
    case PersonalityExtended::Shy:         return shyShape;
    case PersonalityExtended::Flirty:      return flirtyShape;
    default:                               return neutralShape;
  }
}

inline const char* getExtendedPersonalityName(PersonalityExtended p) {
  switch (p) {
    case PersonalityExtended::Neutral:     return "neutral";
    case PersonalityExtended::Happy:       return "happy";
    case PersonalityExtended::Sad:         return "sad";
    case PersonalityExtended::Thinking:    return "thinking";
    case PersonalityExtended::Excited:     return "excited";
    case PersonalityExtended::Confused:    return "confused";
    case PersonalityExtended::Angry:       return "angry";
    case PersonalityExtended::Scared:      return "scared";
    case PersonalityExtended::Sleepy:      return "sleepy";
    case PersonalityExtended::Wink:        return "wink";
    case PersonalityExtended::Love:        return "love";
    case PersonalityExtended::Surprised:   return "surprised";
    case PersonalityExtended::Suspicious:  return "suspicious";
    case PersonalityExtended::Dizzy:       return "dizzy";
    case PersonalityExtended::Bored:       return "bored";
    case PersonalityExtended::Proud:       return "proud";
    case PersonalityExtended::Shy:         return "shy";
    case PersonalityExtended::Flirty:      return "flirty";
    default:                               return "neutral";
  }
}

inline PersonalityExtended findExtendedPersonalityByName(const char* name) {
  if (strcmp(name, "happy") == 0)       return PersonalityExtended::Happy;
  if (strcmp(name, "sad") == 0)         return PersonalityExtended::Sad;
  if (strcmp(name, "thinking") == 0)    return PersonalityExtended::Thinking;
  if (strcmp(name, "excited") == 0)     return PersonalityExtended::Excited;
  if (strcmp(name, "confused") == 0)    return PersonalityExtended::Confused;
  if (strcmp(name, "angry") == 0)       return PersonalityExtended::Angry;
  if (strcmp(name, "scared") == 0)      return PersonalityExtended::Scared;
  if (strcmp(name, "sleepy") == 0)      return PersonalityExtended::Sleepy;
  if (strcmp(name, "wink") == 0)        return PersonalityExtended::Wink;
  if (strcmp(name, "love") == 0)        return PersonalityExtended::Love;
  if (strcmp(name, "surprised") == 0)   return PersonalityExtended::Surprised;
  if (strcmp(name, "suspicious") == 0)  return PersonalityExtended::Suspicious;
  if (strcmp(name, "dizzy") == 0)       return PersonalityExtended::Dizzy;
  if (strcmp(name, "bored") == 0)       return PersonalityExtended::Bored;
  if (strcmp(name, "proud") == 0)       return PersonalityExtended::Proud;
  if (strcmp(name, "shy") == 0)         return PersonalityExtended::Shy;
  if (strcmp(name, "flirty") == 0)      return PersonalityExtended::Flirty;
  return PersonalityExtended::Neutral;
}

// ==========================================
// Blink Frequency by Personality
// ==========================================

inline float getBlinkFrequency(PersonalityExtended p) {
  switch (p) {
    case PersonalityExtended::Happy:       return 0.8f;
    case PersonalityExtended::Sad:         return 0.5f;
    case PersonalityExtended::Thinking:    return 1.2f;
    case PersonalityExtended::Excited:     return 0.0f;
    case PersonalityExtended::Confused:    return 1.8f;
    case PersonalityExtended::Angry:       return 0.3f;
    case PersonalityExtended::Scared:      return 0.0f;
    case PersonalityExtended::Sleepy:      return 0.2f;
    case PersonalityExtended::Love:        return 0.5f;
    case PersonalityExtended::Surprised:   return 0.1f;
    case PersonalityExtended::Suspicious:  return 1.5f;
    case PersonalityExtended::Dizzy:       return 2.5f;
    case PersonalityExtended::Bored:       return 0.15f;
    case PersonalityExtended::Proud:       return 0.4f;
    case PersonalityExtended::Shy:         return 0.6f;
    case PersonalityExtended::Flirty:      return 1.8f;
    default:                               return 1.0f;
  }
}

// ==========================================
// Animation Controller
// ==========================================

class AnimationController {
private:
  AnimationState state;
  
public:
  void start(AnimationSequence seq, uint32_t duration_ms = 3000, bool loop = false) {
    state.current = seq;
    state.startTime = millis();
    state.duration = duration_ms;
    state.step = 0;
    state.progress = 0.0f;
    state.active = true;
    state.loop = loop;
  }
  
  void stop() {
    state.active = false;
    state.current = AnimationSequence::None;
  }
  
  bool isActive() {
    return state.active;
  }
  
  AnimationSequence getCurrentAnimation() {
    return state.current;
  }
  
  void update(float* lookX, float* lookY, float* sizeMultiplier, float* rotation) {
    if (!state.active) return;
    
    uint32_t elapsed = millis() - state.startTime;
    state.progress = (state.duration > 0) ? (float)elapsed / state.duration : 1.0f;
    
    if (state.progress >= 1.0f) {
      if (state.loop) {
        state.startTime = millis();
        state.progress = 0.0f;
        state.step = 0;
      } else {
        stop();
        return;
      }
    }
    
    // Apply animation effects with SMALLER movements to stay in bounds
    switch (state.current) {
      case AnimationSequence::LookAround:
        *lookX = sin(state.progress * M_PI * 4) * 15.0f;  // Reduced from 30
        *lookY = cos(state.progress * M_PI * 2) * 10.0f;  // Reduced from 15
        break;
        
      case AnimationSequence::Bounce:
        *lookY = abs(sin(state.progress * M_PI * 4)) * -15.0f;  // Reduced from -30
        *sizeMultiplier = 1.0f + abs(sin(state.progress * M_PI * 8)) * 0.15f;
        break;
        
      case AnimationSequence::Spiral:
        {
          float angle = state.progress * M_PI * 6;
          float radius = 10.0f * (1.0f - state.progress);  // Reduced from 20
          *lookX = cos(angle) * radius;
          *lookY = sin(angle) * radius;
        }
        break;
        
      case AnimationSequence::HeartBeat:
        {
          float beat = abs(sin(state.progress * M_PI * 8));
          *sizeMultiplier = 1.0f + beat * 0.2f;
        }
        break;
        
      case AnimationSequence::DoubleTake:
        if (state.step == 0 && state.progress > 0.3f) {
          *lookX = -20.0f;  // Reduced from -35
          state.step = 1;
        } else if (state.step == 1 && state.progress > 0.5f) {
          *lookX = 20.0f;  // Reduced from 35
          state.step = 2;
        } else if (state.step == 2 && state.progress > 0.8f) {
          *lookX = 0.0f;
          state.step = 3;
        }
        break;
        
      default:
        break;
    }
  }
};

#endif // BOB_EXPRESSIONS_H

