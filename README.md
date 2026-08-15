# FreeRTOS ADC Acquisition & Display System — MSP430F5529

A multitasking embedded firmware project built on **FreeRTOS** for the **MSP430F5529**. The system performs periodic dual-channel ADC acquisition, processes samples with a moving-average filter, and routes results to a multiplexed 7-segment display, with button-driven task targeting and full inter-task communication via queues and notifications.

---

## Block Diagram

```
Software Timer (500 ms)
        │
        ▼
  ADC12 Trigger ──► ADC12 ISR ──► xADCQueue ──► xTask1 (processing)
                                                       │
                    Buttons S3/S4 ──► xTask2 ──► xTaskQueue
                                                       │
                                            xTask1 packs & routes via
                                            xTaskNotify (target ID + averages)
                                                       │
                                        ┌──────────────┴──────────────┐
                                        ▼                             ▼
                                    xTask3 (S3 path)              xTask4 (S4 path)
                                        │                             │
                                        └────────► 7-segment display ◄┘
                                             (multiplexed, task ID + value)
```

---

## Firmware Overview

### Key Tasks

| Task | Role |
|------|------|
| **xTask1** | Main processing task — receives raw ADC samples, maintains an 8-sample moving average per channel, packs results with the active target ID, and notifies the target output task |
| **xTask2** | Button task — blocks on a semaphore given by the port ISR, debounces, identifies which button (S3/S4) was pressed, and pushes the target ID to `xTaskQueue` |
| **xTask3** | Output task for the S3 path — waits on a task notification, unpacks the target ID and channel-A0 average, and multiplexes the result onto the 7-segment display |
| **xTask4** | Output task for the S4 path — same as xTask3, but for the channel-A1 average, using a blocking wait instead of a polling wait |

### Acquisition

- **Trigger:** a software timer (`xAcquisitionTimer`, 500 ms period) sets `ADC12ENC + ADC12SC` to start a sequence-of-channels conversion
- **Channels:** A0 and A1, sampled back-to-back (`ADC12CONSEQ_1`), each generating its own interrupt flag
- **ISR:** `vADC12ISR` reads `ADC12MEM0`/`ADC12MEM1`, scales to 8 bits, and pushes an `ADC_Message_t {channel, temp}` onto `xADCQueue` from ISR context
- **Filtering:** xTask1 keeps two 8-element circular sample buffers (one per channel) and computes a running average (`sum >> 3`) on every new sample

### Inter-task Communication

- **`xADCQueue`** — ISR → xTask1, carries raw channel/sample pairs
- **`xTaskQueue`** — xTask2 → xTask1, carries the button-selected target task ID (non-blocking receive inside xTask1's main loop)
- **`xEvent_Button`** — binary semaphore, PORT1 ISR → xTask2
- **Task Notifications (mailbox pattern)** — xTask1 → xTask3/xTask4, a single 32-bit value packs the target ID (byte 3) and both channel averages (bytes 2 and 0), consumed with `eSetValueWithOverwrite`

### Display

- 7-segment display multiplexed between two digits (task ID and value), driven by a 50-iteration on/off loop with `vTaskDelay(3 ms)` per digit — gives a flicker-free readout without a dedicated display task

### Buttons

- **S3 / S4** on P1.4 / P1.5, configured with internal pull-ups, falling-edge interrupt
- Debounced in xTask2 with a short busy-wait after the semaphore is given, then re-checked against the live pin state before a target ID is queued

---

## Repository Structure

```
├── main.c                     # Task definitions, ISRs, hardware setup
├── FreeRTOSConfig.h           # RTOS configuration
├── ETF5529_HAL/               # HAL for buttons, 7-segment display, clocks
└── ...                        # CCS/IAR project files
```

---

## Tools

- **Code Composer Studio** — MSP430 toolchain
- **FreeRTOS** — real-time task scheduling
- **MSP430F5529 LaunchPad** — target hardware

---

## Status

Firmware complete and functional — ADC acquisition, button-triggered task notification, and multiplexed display output all verified on hardware.
