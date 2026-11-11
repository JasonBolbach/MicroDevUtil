#pragma once
#include <Arduino.h>

// SuperTouch
// Single-touch gesture recognizer for any touch display.
// Gestures: Short / Medium / Long click (finger stays within Radius), Double-click and Drag.
//
// You supply the hardware read as a plain function:
//
//     bool MyGetTouch(int16_t &X, int16_t &Y) {
//         if (!ts.touched()) return false;
//         auto p = ts.getPoint();  X = p.x;  Y = p.y;   // map to screen coords here
//         return true;
//     }
//     SuperTouch Touch(MyGetTouch);
//
// Call Poll() from loop(). It is NOT timer-driven on purpose: touch controllers
// usually sit on the same SPI/I2C bus as the display, and reading them from a
// timer/IRQ would collide with drawing done in loop().
//
// Events (read with GetEvent):
//   ST_SHORT_CLICK / ST_MEDIUM_CLICK / ST_LONG_CLICK
//       X,Y = where the touch started, Duration = time held (ms). Sent on release.
//       Double = true if a second click followed the first within ClickD_MAX ms and
//       within DoubleRadius pixels. Type/X/Y/Duration describe the FIRST click.
//       Because a click might turn into a double, it is held back for ClickD_MAX ms
//       before being delivered. Set ClickD_MAX = 0 to disable double-click and get
//       clicks immediately on release.
//   ST_DRAG with Phase:
//       ST_DRAG_START : finger left the Radius circle. StartX/StartY = touch-down point
//       ST_DRAG_MOVE  : position changed (unread moves are merged, so the queue never floods)
//       ST_DRAG_END   : finger lifted. X,Y = last position, Duration = whole gesture

typedef bool (*SuperTouchReadFn)(int16_t &X, int16_t &Y);

enum ST_EventType : uint8_t {
    ST_NONE = 0,
    ST_SHORT_CLICK,
    ST_MEDIUM_CLICK,
    ST_LONG_CLICK,
    ST_DRAG,
};

enum ST_DragPhase : uint8_t {
    ST_DRAG_START = 0,
    ST_DRAG_MOVE,
    ST_DRAG_END,
};

struct SuperTouchEvent {
    ST_EventType Type     = ST_NONE;
    ST_DragPhase Phase    = ST_DRAG_START;   // only meaningful for ST_DRAG
    int16_t      X        = 0;
    int16_t      Y        = 0;
    int16_t      StartX   = 0;               // touch-down point
    int16_t      StartY   = 0;
    uint32_t     Duration = 0;               // ms since touch-down
    bool         Double   = false;           // click events: second click followed (see above)
};

class SuperTouch {
public:
    uint16_t Click1_MAX     = 175;   // held <= this -> short
    uint16_t Click2_MAX     = 800;   // held <= this -> medium, else long
    uint16_t ClickD_MAX     = 150;   // window after release for a double-click (0 = off)
    uint16_t Radius         = 10;    // max movement (pixels) still counted as a click
    uint16_t DoubleRadius   = 30;    // 2nd click must land within this of the 1st (pixels)
    uint8_t  PollInterval   = 10;    // ms between touch reads
    uint8_t  PressSamples   = 2;     // consecutive touched reads to register a press
    uint8_t  ReleaseSamples = 3;     // consecutive untouched reads to register a release
                                     // (resistive panels often drop out for a sample or two)

    SuperTouch(SuperTouchReadFn fn = nullptr) : GetTouch(fn) {}

    void Begin(SuperTouchReadFn fn) { GetTouch = fn; Reset(); }

    // Call from loop() as often as you like; reads are rate-limited by PollInterval.
    void Poll() {
        if (!GetTouch) return;
        uint32_t now = millis();
        if (now - PollMillis < PollInterval) return;
        PollMillis = now;

        int16_t x, y;
        if (GetTouch(x, y)) {
            RelCount = 0;
            LastTouchMillis = now;
            if (!Down) {
                if (PressCount == 0) { DownMillis = now; DownX = x; DownY = y; }
                if (++PressCount >= PressSamples) {
                    Down = true;
                    PressCount = 0;
                    OnDown();
                    OnHold(x, y);
                }
            } else {
                OnHold(x, y);
            }
        } else {
            PressCount = 0;
            if (Down && ++RelCount >= ReleaseSamples) {
                Down = false;
                RelCount = 0;
                OnUp();
            }
        }

        // Pending click whose double-click window has closed with no new touch: deliver it.
        if (Pending && !Down && PressCount == 0 && now - PendUpMillis > ClickD_MAX)
            FlushPending(false);
    }

    // Non-blocking. Returns true and fills 'ev' if an event is waiting.
    bool GetEvent(SuperTouchEvent &ev) {
        if (QCount == 0) return false;
        ev = Queue[QTail];
        QTail = (QTail + 1) % Q_SIZE;
        QCount--;
        return true;
    }

    // Discard pending events and forget any touch in progress.
    void Reset() {
        QHead = QTail = QCount = 0;
        St = IDLE;
        Down = false;
        Pending = SecondPress = false;
        PressCount = RelCount = 0;
    }

    bool    IsTouched()  const { return Down; }             // debounced
    bool    IsDragging() const { return St == DRAGGING; }
    int16_t GetX()       const { return LastX; }            // last touched position
    int16_t GetY()       const { return LastY; }

private:
    enum State : uint8_t { IDLE, PRESSED, DRAGGING };
    static constexpr uint8_t Q_SIZE = 8;

    SuperTouchReadFn GetTouch;
    State    St = IDLE;
    bool     Down = false;
    uint8_t  PressCount = 0, RelCount = 0;
    uint32_t PollMillis = 0, DownMillis = 0, LastTouchMillis = 0;
    int16_t  DownX = 0, DownY = 0, LastX = 0, LastY = 0;

    // First click waiting to see whether a second one follows.
    bool         Pending = false, SecondPress = false;
    ST_EventType PendType = ST_NONE;
    int16_t      PendX = 0, PendY = 0;
    uint32_t     PendDownMillis = 0, PendUpMillis = 0;

    SuperTouchEvent Queue[Q_SIZE];
    uint8_t QHead = 0, QTail = 0, QCount = 0;

    void OnDown() {
        if (Pending) {
            int32_t dx = DownX - PendX, dy = DownY - PendY;
            if (DownMillis - PendUpMillis <= ClickD_MAX &&
                dx * dx + dy * dy <= (int32_t)DoubleRadius * DoubleRadius)
                SecondPress = true;                 // keep first click pending
            else
                FlushPending(false);                // too late / too far: it was a lone click
        }
        St = PRESSED;
        LastX = DownX;
        LastY = DownY;
    }

    void OnHold(int16_t x, int16_t y) {
        bool moved = (x != LastX || y != LastY);
        LastX = x;
        LastY = y;

        if (St == PRESSED) {
            int32_t dx = x - DownX, dy = y - DownY;
            if (dx * dx + dy * dy > (int32_t)Radius * Radius) {
                St = DRAGGING;
                if (Pending) FlushPending(false);   // a drag can't be a double-click
                SecondPress = false;
                Emit(ST_DRAG, ST_DRAG_START, x, y, LastTouchMillis);
            }
        } else if (St == DRAGGING && moved) {
            Emit(ST_DRAG, ST_DRAG_MOVE, x, y, LastTouchMillis);
        }
    }

    void OnUp() {
        // Use the last *touched* sample time so ReleaseSamples doesn't inflate the duration.
        uint32_t held = LastTouchMillis - DownMillis;
        if (St == PRESSED) {
            ST_EventType t = (held <= Click1_MAX) ? ST_SHORT_CLICK
                           : (held <= Click2_MAX) ? ST_MEDIUM_CLICK
                           :                        ST_LONG_CLICK;
            if (SecondPress) {
                FlushPending(true);                 // report the first click, flagged double
                SecondPress = false;
            } else if (ClickD_MAX > 0) {
                Pending = true;                     // wait to see if a second click follows
                PendType = t;
                PendX = DownX;  PendY = DownY;
                PendDownMillis = DownMillis;
                PendUpMillis   = LastTouchMillis;
            } else {
                Emit(t, ST_DRAG_START, DownX, DownY, LastTouchMillis);
            }
        } else if (St == DRAGGING) {
            Emit(ST_DRAG, ST_DRAG_END, LastX, LastY, LastTouchMillis);
        }
        St = IDLE;
    }

    void FlushPending(bool dbl) {
        SuperTouchEvent e;
        e.Type = PendType;
        e.X = e.StartX = PendX;
        e.Y = e.StartY = PendY;
        e.Duration = PendUpMillis - PendDownMillis;
        e.Double = dbl;
        Pending = false;
        Push(e);
    }

    void Emit(ST_EventType type, ST_DragPhase phase, int16_t x, int16_t y, uint32_t at) {
        SuperTouchEvent e;
        e.Type = type; e.Phase = phase;
        e.X = x; e.Y = y;
        e.StartX = DownX; e.StartY = DownY;
        e.Duration = at - DownMillis;
        Push(e);
    }

    void Push(const SuperTouchEvent &e) {
        ST_EventType type = e.Type;
        ST_DragPhase phase = e.Phase;

        // Merge consecutive unread DRAG_MOVEs so a slow consumer can't overflow the queue.
        if (type == ST_DRAG && phase == ST_DRAG_MOVE && QCount > 0) {
            SuperTouchEvent &newest = Queue[(QHead + Q_SIZE - 1) % Q_SIZE];
            if (newest.Type == ST_DRAG && newest.Phase == ST_DRAG_MOVE) { newest = e; return; }
        }

        if (QCount == Q_SIZE) {                    // full: drop oldest
            QTail = (QTail + 1) % Q_SIZE;
            QCount--;
        }
        Queue[QHead] = e;
        QHead = (QHead + 1) % Q_SIZE;
        QCount++;
    }
};
