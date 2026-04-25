// pongping — CAN PONG responder for M5Stack ATOM Lite (ESP32-PICO-D4)
//
// Spec: pongping/specification.md
//
// Receives PING frames (ID 0x010, DLC 2, [seq, 0x50]) from the Nucleo and
// immediately replies with a PONG (ID 0x011, DLC 2, [seq, 0x50]).
//
// CAN transceiver wiring (e.g. TJA1050 or SN65HVD230):
//   Transceiver TXD  ←  ATOM GPIO 22  (bottom 6-pin connector, pin 3)
//   Transceiver RXD  →  ATOM GPIO 19  (bottom 6-pin connector, pin 4)
//   Transceiver VCC  =  3.3 V or 5 V (check transceiver datasheet)
//   120 Ω termination must be fitted at each bus end.

#include <Arduino.h>
#include <driver/twai.h>
#include <FastLED.h>

// ---------------------------------------------------------------------------
// CAN IDs
// ---------------------------------------------------------------------------
static constexpr uint32_t PING_ID = 0x010;
static constexpr uint32_t PONG_ID = 0x011;

// ---------------------------------------------------------------------------
// GPIO assignments
// ---------------------------------------------------------------------------
static constexpr gpio_num_t CAN_TX_PIN = GPIO_NUM_22;  // to transceiver TXD
static constexpr gpio_num_t CAN_RX_PIN = GPIO_NUM_19;  // from transceiver RXD

static constexpr int LED_PIN  = 27;  // ATOM Lite built-in WS2812B
static constexpr int BTN_PIN  = 39;  // ATOM Lite built-in button (active-low)

// ---------------------------------------------------------------------------
// Status LED
// ---------------------------------------------------------------------------
static CRGB leds[1];

static void led_set(CRGB colour) {
    leds[0] = colour;
    FastLED.show();
}

// ---------------------------------------------------------------------------
// TWAI (ESP32 CAN) initialisation
// ---------------------------------------------------------------------------
static void can_init() {
    // Normal operation mode
    twai_general_config_t g_config =
        TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);

    // 500 kbps
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();

    // Acceptance filter: pass only standard ID 0x010.
    // SJA1000 single-filter layout for standard frames:
    //   bits [31:21] = ID[10:0], bit 20 = RTR, bits [19:0] = don't care.
    //   acceptance_mask bit = 0 → must match, 1 → don't care.
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_ERROR_CHECK(twai_start());
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(100);

    // LED
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, 1);
    FastLED.setBrightness(50);
    led_set(CRGB::Red);   // initialising

    // Button (input-only pin, external pull-up on ATOM hardware)
    pinMode(BTN_PIN, INPUT);

    can_init();

    Serial.println("pongping ready — 500 kbps, waiting for PING (0x010)");
    led_set(CRGB::Blue);  // idle / ready
}

void loop() {
    twai_message_t rx;

    // Block up to 500 ms for a frame (filters guarantee ID = 0x010)
    if (twai_receive(&rx, pdMS_TO_TICKS(500)) != ESP_OK) {
        // No frame within the window — stay idle (Nucleo sends every 500 ms)
        return;
    }

    // Sanity-check the frame even though the hardware filter already narrows it
    if (rx.identifier != PING_ID || rx.data_length_code < 2) {
        return;
    }

    uint8_t seq = rx.data[0];
    Serial.printf("PING seq=%03u  ->  PONG\n", seq);

    // Build PONG reply
    twai_message_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.identifier       = PONG_ID;
    tx.data_length_code = 2;
    tx.data[0]          = seq;   // echo sequence number
    tx.data[1]          = 0x50;  // fixed marker 'P'
    tx.extd             = 0;     // standard (11-bit) ID
    tx.rtr              = 0;     // data frame

    esp_err_t err = twai_transmit(&tx, pdMS_TO_TICKS(50));
    if (err == ESP_OK) {
        led_set(CRGB::Green);
        delay(40);
        led_set(CRGB::Blue);
    } else {
        Serial.printf("PONG TX error: %d\n", (int)err);
        led_set(CRGB::Red);
        delay(100);
        led_set(CRGB::Blue);
    }
}
