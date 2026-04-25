# Remote Board CAN Specification — pingpong counterpart

**Date:** 2026-04-25  
**Project:** pingpong  
**Nucleo firmware:** `pingpong/Core/Src/main.c`

---

## 1  Purpose

This document specifies the CAN behaviour required from the remote board that
communicates with the NUCLEO-F446RE running the *pingpong* firmware.

The Nucleo periodically sends a **PING** frame and expects a **PONG** reply.
The remote board's only CAN obligation is to echo each PING back as a PONG
within the timeout window.

---

## 2  Physical layer

| Parameter | Value |
|-----------|-------|
| Bus topology | Two-node point-to-point (or multi-drop with proper termination) |
| Bit rate | **500 kbps** |
| Frame format | Classical CAN 2.0A (11-bit standard IDs only) |
| Termination | 120 Ω at each bus end |
| Connector | Application-specific; logic levels are CAN-standard differential |

The Nucleo exposes CAN1 on **CN10 pin 3** (PB8 = CANH/RX side) and
**CN10 pin 5** (PB9 = CANL/TX side) through a CAN transceiver (e.g. TJA1050 or
SN65HVD230).  The remote board must use a compatible transceiver on its side.

---

## 3  Message catalogue

### 3.1  PING (Nucleo → remote)

| Field | Value |
|-------|-------|
| CAN ID | **0x010** (11-bit standard) |
| DLC | 2 |
| data[0] | Sequence number — `uint8_t`, increments 0 → 255 → 0 → … |
| data[1] | Fixed marker **0x50** (`'P'`) |
| Period | Every 500 ms (nominal) |

### 3.2  PONG (remote → Nucleo)

| Field | Value |
|-------|-------|
| CAN ID | **0x011** (11-bit standard) |
| DLC | 2 |
| data[0] | **Echoed** sequence number — must match data[0] of the received PING |
| data[1] | Fixed marker **0x50** (`'P'`) |
| Deadline | Must be transmitted within **100 ms** of receiving the PING |

---

## 4  Protocol state machine

```
Remote board perspective:

  IDLE ──[receive frame with ID 0x010]──► PROCESS
  PROCESS: copy data[0] (seq), set data[1]=0x50, transmit with ID 0x011
  PROCESS ──[TX complete]──► IDLE
```

- All other CAN IDs received by the remote board **must be ignored**.
- The remote board must not transmit spontaneously; it only replies to PING.
- If two PINGs arrive before the PONG is sent, only the most recent seq number
  must be echoed (no queuing required).

---

## 5  Timing requirements

| Parameter | Requirement |
|-----------|-------------|
| PONG latency (PING RX → PONG TX start) | ≤ 50 ms (recommended ≤ 5 ms) |
| PONG must be received by Nucleo within | 100 ms of PING transmission |
| Nucleo PING interval | 500 ms nominal |

The Nucleo's pass/fail criterion:

- **PASS**: PONG received within 100 ms, ID = 0x011, data[0] matches sent seq,
  data[1] = 0x50.
- **FAIL**: Any of the above conditions not met, or no PONG within 100 ms
  (reported as TIMEOUT).

---

## 6  Acceptance filter recommendation

To reduce CPU load the remote board's CAN controller filter should pass only
CAN ID **0x010**.  Example mask-mode filter (32-bit, standard frame):

```
Filter ID   = 0x010 << 5  = 0x0200   (bits [15:5] hold the 11-bit ID)
Filter mask = 0x7FF << 5  = 0xFFE0   (match all 11 ID bits, ignore RTR/IDE)
```

---

## 7  Nucleo serial output format

For reference, the Nucleo reports each exchange on its ST-Link VCP (115 200 baud,
8N1) in the following format:

```
pingpong started
CAN1_Init=0
CAN1 state=3 err=0x00000000
PING seq=000 -> PONG ID=0x011 [00 50] PASS  (ok=1/1)
PING seq=001 -> PONG ID=0x011 [01 50] PASS  (ok=2/2)
PING seq=002 -> TIMEOUT  (ok=2/3)
...
```

The counters show successful PONGs / total PINGs sent.

---

## 8  Checklist for remote board implementer

- [ ] CAN controller configured for 500 kbps, standard frame format
- [ ] Acceptance filter passes ID 0x010, rejects all others
- [ ] On PING reception: copy seq byte, fix marker byte to 0x50
- [ ] Transmit PONG with ID 0x011 within 50 ms
- [ ] Termination resistor fitted (120 Ω)
- [ ] Transceiver compatible with 3.3 V or 5 V logic as appropriate
