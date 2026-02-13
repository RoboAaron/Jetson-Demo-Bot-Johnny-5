/*
 * ps3_bridge.ino — ESP32 PS3 controller → Jetson USB serial bridge
 *
 * Pairs with a PS3 DualShock controller over Bluetooth and streams joystick
 * state as newline-delimited JSON to the Jetson via USB serial at 115200 baud.
 *
 * The companion ROS 2 node (jetson/ros2/balance_bridge/esp32_joy_node.py) reads
 * this JSON and publishes sensor_msgs/Joy on /joy.
 *
 * ── Setup ──────────────────────────────────────────────────────────────────
 * Library: ps3Controller by jvpernis
 *   Arduino IDE: Sketch → Include Library → Manage Libraries → search "ps3Controller"
 *   PlatformIO:  lib_deps = jvpernis/PS3 Controller Host@^1.1.0
 *
 * Pairing (one-time):
 *   1. Flash this sketch to the ESP32.
 *   2. Open Serial Monitor at 115200. Note the ESP32 Bluetooth MAC printed on boot.
 *   3. On a Linux machine with sixaxis/sixpair:
 *        sudo sixpair <ESP32_MAC>        # sets PS3 host address
 *      OR use the Android app "SixaxisPairTool".
 *   4. Power-cycle the PS3 controller; it should connect within a few seconds.
 *      LED 1 on the controller will stay solid when paired.
 *
 * ── Output format ──────────────────────────────────────────────────────────
 * JSON line sent at OUTPUT_HZ (default 20 Hz):
 *   {"lx":<-1..1>,"ly":<-1..1>,"rx":<-1..1>,"ry":<-1..1>,
 *    "b_x":<0|1>,"b_o":<0|1>,"b_sq":<0|1>,"b_tr":<0|1>,
 *    "b_l1":<0|1>,"b_r1":<0|1>,"b_l2":<0|1>,"b_r2":<0|1>,
 *    "b_ps":<0|1>,"b_start":<0|1>,"b_sel":<0|1>,"connected":<0|1>}
 *
 * Axis convention (matches ROS sensor_msgs/Joy defaults):
 *   lx  Left stick horizontal  — positive = right
 *   ly  Left stick vertical    — positive = up   (forward drive → ROS linear.x)
 *   rx  Right stick horizontal — positive = right (steer → ROS angular.z)
 *   ry  Right stick vertical   — positive = up
 *
 * The esp32_joy_node maps:  linear.x = ly,  angular.z = -rx
 * (negating rx because right-stick-right = turn-right = negative angular.z in ROS)
 */

#include <Ps3Controller.h>

// ── Configuration ─────────────────────────────────────────────────────────
static const int   OUTPUT_HZ        = 20;     // JSON output rate
static const float DEADZONE         = 0.08f;  // joystick centre deadband (normalised)
static const int   LED_PIN          = 2;      // built-in LED (most ESP32 dev boards)

// ── Globals ───────────────────────────────────────────────────────────────
static bool        g_connected      = false;
static unsigned long g_lastOutput   = 0;

// ── Helpers ───────────────────────────────────────────────────────────────

// Normalise PS3 raw axis byte (0–255, centre ~128) to -1.0 … +1.0 with deadzone.
static float normalise(int raw) {
    float v = (raw - 128) / 127.0f;
    if (v > 1.0f)  v = 1.0f;
    if (v < -1.0f) v = -1.0f;
    if (fabsf(v) < DEADZONE) v = 0.0f;
    return v;
}

// ── PS3 callbacks ─────────────────────────────────────────────────────────

void onConnect() {
    g_connected = true;
    digitalWrite(LED_PIN, HIGH);
    Serial.println("{\"event\":\"connected\"}");
}

void onDisconnect() {
    g_connected = false;
    digitalWrite(LED_PIN, LOW);
    Serial.println("{\"event\":\"disconnected\"}");
}

// ── Arduino lifecycle ─────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    Ps3.attach(nullptr);          // no per-event callback — we poll in loop()
    Ps3.attachOnConnect(onConnect);
    Ps3.attachOnDisconnect(onDisconnect);
    Ps3.begin();                  // start Bluetooth; MAC printed to Serial

    Serial.print("{\"event\":\"boot\",\"mac\":\"");
    Serial.print(Ps3.getAddress());
    Serial.println("\"}");
    Serial.println("# Waiting for PS3 controller...");
    Serial.println("# Run: sudo sixpair <MAC> on host to pair.");
}

void loop() {
    unsigned long now = millis();
    if (now - g_lastOutput < (1000 / OUTPUT_HZ)) return;
    g_lastOutput = now;

    if (!g_connected || !Ps3.isConnected()) {
        // Send heartbeat so the ROS node knows we're alive but unpaired
        Serial.println("{\"connected\":0}");
        return;
    }

    // Read axes
    float lx =  normalise(Ps3.data.analog.stick.lx);
    float ly = -normalise(Ps3.data.analog.stick.ly); // PS3 Y: up = low byte value
    float rx =  normalise(Ps3.data.analog.stick.rx);
    float ry = -normalise(Ps3.data.analog.stick.ry);

    // Read buttons (1 = pressed)
    int b_x     = Ps3.data.button.cross    ? 1 : 0;
    int b_o     = Ps3.data.button.circle   ? 1 : 0;
    int b_sq    = Ps3.data.button.square   ? 1 : 0;
    int b_tr    = Ps3.data.button.triangle ? 1 : 0;
    int b_l1    = Ps3.data.button.l1       ? 1 : 0;
    int b_r1    = Ps3.data.button.r1       ? 1 : 0;
    int b_l2    = Ps3.data.button.l2       ? 1 : 0;
    int b_r2    = Ps3.data.button.r2       ? 1 : 0;
    int b_ps    = Ps3.data.button.ps       ? 1 : 0;
    int b_start = Ps3.data.button.start    ? 1 : 0;
    int b_sel   = Ps3.data.button.select   ? 1 : 0;

    // Emit JSON (fixed 3 decimal places for axes)
    Serial.printf(
        "{\"lx\":%.3f,\"ly\":%.3f,\"rx\":%.3f,\"ry\":%.3f,"
        "\"b_x\":%d,\"b_o\":%d,\"b_sq\":%d,\"b_tr\":%d,"
        "\"b_l1\":%d,\"b_r1\":%d,\"b_l2\":%d,\"b_r2\":%d,"
        "\"b_ps\":%d,\"b_start\":%d,\"b_sel\":%d,\"connected\":1}\n",
        lx, ly, rx, ry,
        b_x, b_o, b_sq, b_tr,
        b_l1, b_r1, b_l2, b_r2,
        b_ps, b_start, b_sel
    );
}
