#include "LedIndicator.h"

#ifdef SLAVE_BUILD
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// TODO: Configure LED pin for XIAO ESP32-C3
#define LED_PIN         14
#define LED_COUNT       1
#define LED_BRIGHTNESS  200

static Adafruit_NeoPixel g_pixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

#else
// Stubs
#endif

namespace LedIndicator {

// State
static State g_current_state = State::IDLE_EMPTY;
static uint32_t g_animation_deadline_ms = 0;
static bool g_pulse_on = false;

// Color definitions (RGB)
static const uint32_t COLOR_OFF = 0x000000;
static const uint32_t COLOR_RED = 0xFF0000;
static const uint32_t COLOR_GREEN = 0x00FF00;
static const uint32_t COLOR_BLUE = 0x0000FF;
static const uint32_t COLOR_CYAN = 0x00FFFF;
static const uint32_t COLOR_YELLOW = 0xFFFF00;

void init() {
#ifdef SLAVE_BUILD
    g_pixel.begin();
    g_pixel.setBrightness(LED_BRIGHTNESS);
    g_pixel.setPixelColor(0, COLOR_OFF);
    g_pixel.show();
#endif
}

void setState(State state) {
#ifdef SLAVE_BUILD
    g_current_state = state;
    g_pulse_on = false;
    g_animation_deadline_ms = millis();
    
    uint32_t color = COLOR_OFF;
    switch (state) {
        case State::IDLE_EMPTY:
            color = COLOR_OFF;
            break;
        case State::IDLE_READY:
            color = COLOR_GREEN;
            break;
        case State::GRIP_PENDING:
            color = COLOR_BLUE;
            break;
        case State::LOAD_FEED:
            color = COLOR_CYAN;
            break;
        case State::FEED_STUCK:
            color = COLOR_RED;
            break;
        case State::IDLE_ERROR:
            color = COLOR_RED;
            break;
        case State::ENUMERATE:
            color = COLOR_YELLOW;
            break;
    }
    
    g_pixel.setPixelColor(0, color);
    g_pixel.show();
#endif
}

void tick() {
#ifdef SLAVE_BUILD
    uint32_t now_ms = millis();
    
    // Handle pulsing for error states
    if (g_current_state == State::FEED_STUCK || g_current_state == State::IDLE_ERROR) {
        if (now_ms >= g_animation_deadline_ms) {
            g_pulse_on = !g_pulse_on;
            g_animation_deadline_ms = now_ms + 500;  // 500 ms pulse period
            
            uint32_t color = g_pulse_on ? COLOR_RED : COLOR_OFF;
            g_pixel.setPixelColor(0, color);
            g_pixel.show();
        }
    }
#endif
}

}  // namespace LedIndicator
