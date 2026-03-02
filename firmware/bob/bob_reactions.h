// ==========================================
// Bob Smart Reactions
// Context-aware personality changes
// ==========================================

#ifndef BOB_REACTIONS_H
#define BOB_REACTIONS_H

class SmartReactions {
private:
  uint32_t lastReactionTime = 0;
  const uint32_t REACTION_COOLDOWN = 3000;  // 3 seconds between reactions
  
  PersonalityExtended lastProximityPersonality = PersonalityExtended::Neutral;
  bool proximityReactionActive = false;
  
public:
  // React to proximity detection
  PersonalityExtended reactToProximity(int proximity, bool detected) {
    uint32_t now = millis();
    
    // Don't react if in cooldown OR if we should be sleeping
    if (now - lastReactionTime < REACTION_COOLDOWN) {
      return PersonalityExtended::Neutral;  // No change, cooldown active
    }
    
    // Only react to significant proximity (>30%) to avoid sleep interruption
    if (detected && proximity > 50) {
      // Very close - surprised!
      lastReactionTime = now;
      proximityReactionActive = true;
      lastProximityPersonality = PersonalityExtended::Surprised;
      return PersonalityExtended::Surprised;
      
    } else if (detected && proximity > 40) {  // Increased threshold from 30
      // Moderately close - curious
      lastReactionTime = now;
      proximityReactionActive = true;
      lastProximityPersonality = PersonalityExtended::Thinking;
      return PersonalityExtended::Thinking;
      
    } else if (proximityReactionActive && !detected) {
      // Object left - return to neutral
      proximityReactionActive = false;
      return PersonalityExtended::Neutral;
    }
    
    return PersonalityExtended::Neutral;  // No reaction
  }
  
  // React to battery level
  PersonalityExtended reactToBattery(int batteryPercent, bool wasLow, bool isLow) {
    if (!wasLow && isLow) {
      // Just became low - tired/sleepy
      return PersonalityExtended::Sleepy;
    } else if (wasLow && !isLow) {
      // Charging completed - happy!
      return PersonalityExtended::Happy;
    }
    return PersonalityExtended::Neutral;
  }
  
  // React to time of day (if RTC available)
  PersonalityExtended reactToTimeOfDay(uint8_t hour) {
    if (hour >= 22 || hour <= 5) {
      // Night time (10 PM - 5 AM) - sleepy
      return PersonalityExtended::Sleepy;
    } else if (hour >= 6 && hour <= 8) {
      // Morning (6 AM - 8 AM) - waking up, confused
      return PersonalityExtended::Confused;
    } else if (hour >= 12 && hour <= 13) {
      // Lunch time - happy
      return PersonalityExtended::Happy;
    }
    return PersonalityExtended::Neutral;
  }
  
  // React to temperature
  PersonalityExtended reactToTemperature(float tempC) {
    if (tempC > 45.0f) {
      // Hot - uncomfortable/sad
      return PersonalityExtended::Sad;
    } else if (tempC < 10.0f) {
      // Cold - sad
      return PersonalityExtended::Sad;
    }
    return PersonalityExtended::Neutral;
  }
  
  // Periodic random reactions (for personality)
  PersonalityExtended randomReaction(uint32_t now) {
    static uint32_t lastRandomReaction = 0;
    const uint32_t RANDOM_INTERVAL_MIN = 60000;  // 1 minute
    const uint32_t RANDOM_INTERVAL_MAX = 180000; // 3 minutes
    static uint32_t nextRandomReaction = now + random(RANDOM_INTERVAL_MIN, RANDOM_INTERVAL_MAX);
    
    if (now >= nextRandomReaction) {
      // Random personality for variety
      int roll = random(100);
      nextRandomReaction = now + random(RANDOM_INTERVAL_MIN, RANDOM_INTERVAL_MAX);
      
      if (roll < 20) {
        return PersonalityExtended::Thinking;  // 20% - thoughtful
      } else if (roll < 35) {
        return PersonalityExtended::Happy;     // 15% - happy
      } else if (roll < 45) {
        return PersonalityExtended::Confused;  // 10% - confused
      } else if (roll < 50) {
        return PersonalityExtended::Love;      // 5% - loving
      }
      // 50% - no change
    }
    
    return PersonalityExtended::Neutral;
  }
  
  bool isProximityReactionActive() {
    return proximityReactionActive;
  }
};

#endif // BOB_REACTIONS_H
