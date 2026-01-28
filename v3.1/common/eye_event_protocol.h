/**
 * Spider Robot v3.1 - Eye Event Protocol
 * 
 * Brain → Eye Service communication via Unix socket.
 * JSON event format for simplicity and debuggability.
 */

#ifndef EYE_EVENT_PROTOCOL_H
#define EYE_EVENT_PROTOCOL_H

#define EYE_SOCKET_PATH "/tmp/spider_eye.sock"

/**
 * Event Types (JSON "type" field):
 * 
 * 1. mood     - Set eye mood
 *    {"type":"mood","mood":"normal|angry|happy|sleepy"}
 * 
 * 2. look     - Set pupil position
 *    {"type":"look","x":0.0,"y":0.0}
 *    x,y in range [-1.0, 1.0], 0 = center
 * 
 * 3. blink    - Trigger blink
 *    {"type":"blink"}
 * 
 * 4. wink     - Trigger wink on one eye
 *    {"type":"wink","eye":"left|right"}
 * 
 * 5. color    - Set iris color (RGB565)
 *    {"type":"color","rgb565":0x001F}
 * 
 * 6. idle     - Enable/disable idle animation
 *    {"type":"idle","enabled":true|false}
 * 
 * 7. estop    - Emergency stop (close eyes)
 *    {"type":"estop"}
 * 
 * 8. status   - Request status (response: {"status":"ok","mood":"...","idle":true})
 *    {"type":"status"}
 */

// Mood string constants
#define EYE_MOOD_NORMAL   "normal"
#define EYE_MOOD_ANGRY    "angry"
#define EYE_MOOD_HAPPY    "happy"
#define EYE_MOOD_SLEEPY   "sleepy"

// Eye identifiers
#define EYE_LEFT   "left"
#define EYE_RIGHT  "right"

// Maximum event JSON size
#define EYE_EVENT_MAX_SIZE 256

#endif // EYE_EVENT_PROTOCOL_H
