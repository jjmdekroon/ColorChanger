#include "Gripper.h"
#include "../include/Config.h"

#ifdef SLAVE_BUILD
#include <Arduino.h>
#include <ESP32Servo.h>

// TODO: Configure servo pin for XIAO ESP32-C3
#define SERVO_PIN 13

static ESP32Servo g_servo;

#else
// Stubs
#endif

namespace Gripper {

// State
static Position g_target_position = Position::OPEN;
static Position g_current_position = Position::OPEN;
static uint32_t g_travel_start_ms = 0;
static bool g_traveling = false;

void init() {
#ifdef SLAVE_BUILD
    g_servo.setPeriodHertz(50);  // Standard servo frequency
    g_servo.attach(SERVO_PIN, SERVO_OPEN_US, SERVO_GRIP_US);
    
    // Move to OPEN position immediately
    g_servo.writeMicroseconds(SERVO_OPEN_US);
    g_current_position = Position::OPEN;
    g_target_position = Position::OPEN;
    g_traveling = false;
#endif
}

void moveTo(Position pos) {
#ifdef SLAVE_BUILD
    if (pos == g_current_position && pos == g_target_position) {
        // Already there, no need to move
        return;
    }
    
    g_target_position = pos;
    g_travel_start_ms = millis();
    g_traveling = true;
#endif
}

bool isMoving() {
    return g_traveling;
}

Position getCurrentPosition() {
    return g_current_position;
}

void tick() {
#ifdef SLAVE_BUILD
    if (!g_traveling) {
        return;
    }
    
    uint32_t now_ms = millis();
    uint32_t elapsed_ms = now_ms - g_travel_start_ms;
    
    if (elapsed_ms >= SERVO_TRAVEL_MS) {
        // Travel complete
        g_current_position = g_target_position;
        g_traveling = false;
        uint16_t target_us = (g_target_position == Position::OPEN) ? SERVO_OPEN_US : SERVO_GRIP_US;
        g_servo.writeMicroseconds(target_us);
    } else {
        // Interpolate position during travel
        float progress = static_cast<float>(elapsed_ms) / SERVO_TRAVEL_MS;
        
        uint16_t start_us = (g_current_position == Position::OPEN) ? SERVO_OPEN_US : SERVO_GRIP_US;
        uint16_t end_us = (g_target_position == Position::OPEN) ? SERVO_OPEN_US : SERVO_GRIP_US;
        
        uint16_t current_us = static_cast<uint16_t>(start_us + (end_us - start_us) * progress);
        g_servo.writeMicroseconds(current_us);
    }
#endif
}

}  // namespace Gripper
