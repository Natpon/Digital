# Digital
Project
// ลองรัน(esp32) ใช้โค้ดจาก ESP32 MQTT ก่อน [มันมาล้อตเดียวกัน]

##prompt##


"I have a 3-floor elevator control system. The FPGA is the main controller — it runs fully standalone without ESP32. ESP32 is only an optional WiFi bridge for Blynk IoT remote monitoring.

HARDWARE:

FPGA Board: Surveyor-6 (50MHz clock, Active High reset)
Motor Driver: L298N (ENA=PWM, IN1/IN2=direction)
Motor Encoder: rotary encoder, outputs pulse per rotation
Display: 1x 7-segment Common Cathode (shows current floor 1/2/3)
Inputs on FPGA:

sw_floor1, sw_floor2, sw_floor3 → select destination floor (highest priority)
door_hold → hold door open while pressed
pulse_in → encoder pulse from motor
rx_in → UART from ESP32 (optional, lower priority than switches)


Outputs on FPGA:

ena_pwm → motor enable
in1_dir, in2_dir → motor direction
door_led → door open indicator LED
seg_out (7-bit) → 7-segment display
tx_out → UART status back to ESP32




FPGA MODULES (5 files + 1 new):
1. UART_RX.vhd

Baud: 9600, Clock: 50MHz, CLKS_PER_BIT=5208
States: IDLE → START_BIT → DATA_BITS → STOP_BIT
2-stage metastability synchronizer (rx_meta → rx_sync)
Samples at mid-bit (CLKS_PER_BIT/2) in START_BIT
Validates stop bit = '1', else framing error → discard
data_ready pulses 1 clock cycle only

2. UART_TX.vhd

Same baud rate as UART_RX (5208)
States: IDLE → START_BIT → DATA_BITS → STOP_BIT
tx_busy flag prevents overlap
Sends floor status ASCII '1'/'2'/'3' to ESP32
Only sends when floor changes

3. Floor_Position_Tracker.vhd

3-stage pipeline: pulse_meta → pulse_sync(0) → pulse_sync(1)
Rising edge detection: pulse_sync(1)='0' AND pulse_sync(0)='1'
dir_up='1' → pos_counter+1, dir_up='0' → pos_counter-1
Overflow guard: counter stays at 65535 max, 0 min
Output: current_pos as 16-bit STD_LOGIC_VECTOR

4. Elevator_FSM.vhd

States: IDLE, MOVING_UP, MOVING_DOWN, DOOR_OPEN
Floor positions: F1=0, F2=5000, F3=10000 pulses (calibrate on site)
TOLERANCE = 50 pulses
IDLE: latches target_floor → target_pos only when stopped (prevents mid-travel retargeting). Underflow protection: checks target_pos > TOLERANCE before subtracting
MOVING_UP: motor_run='1', motor_dir='1'. Stops when pos >= target-TOLERANCE
MOVING_DOWN: motor_run='1', motor_dir='0'. Stops when pos <= target+TOLERANCE
DOOR_OPEN: door_led='1', 5-second timer (250,000,000 cycles at 50MHz) using unsigned(27 downto 0). If door_hold='1' → reset timer (keep open). After timeout → back to IDLE
New port: door_hold : in STD_LOGIC

5. Elevator_Top.vhd (Structural)

Instantiates: UART_RX, UART_TX, Floor_Position_Tracker, Elevator_FSM
5 processes:

Command Parser: sw_floor has priority over UART. sw_floor1→"01", sw_floor2→"10", sw_floor3→"11". UART accepts ASCII x"31"/'x"32"/x"33"
Motor Driver: sig_motor_run/dir → ena_pwm, in1_dir, in2_dir. Has reset.
Position→Floor converter: current_pos → sig_current_floor "01"/"10"/"11". Holds last floor while moving.
7-Segment encoder: "01"→"0000110", "10"→"1011011", "11"→"1001111" (Common Cathode)
UART TX trigger: sends ASCII when sig_current_floor changes, checks tx_busy before sending




ESP32 (optional WiFi bridge only):

Library: Blynk IoT (blynk.cloud)
Serial2: RX=16, TX=17, 9600 baud
Virtual Pins:

V1 → user's current floor → sends to FPGA via Serial2 (pickup command)
V2 → destination floor → only valid after V1 pressed, sends to FPGA
V3 → Label widget showing elevator current floor (updated when FPGA sends status)


Receives floor status from FPGA Serial2 → Blynk.virtualWrite(V3)
WiFi timeout: 10 seconds, if failed → log only mode
Non-blocking reconnect every 5 seconds using millis()


KEY BUGS FIXED:

UART_RX: end case → end if in IDLE state (was syntax error)
UART_RX: added 2-stage synchronizer for metastability
UART_RX: stop bit validation added
FSM: target_pos now latched in IDLE only (was combinational, caused mid-travel retargeting)
FSM: integer underflow fix for floor 1 (0 - TOLERANCE was negative)
FSM: delay_timer changed from integer to unsigned(27 downto 0)
FSM: TIME_3_SEC → TIME_5_SEC = 250,000,000
FSM: added door_hold port and logic
Position Tracker: separated synchronizer from edge detector (3 stages)
Elevator_Top: motor driver process now has reset
Elevator_Top: added UART_TX, 7-segment, sw_floor ports and processes


WORKFLOW FOR REVIEW:

Review each file one by one
Explain workflow section by section
Confirm with me at each step before proceeding
After confirmed, generate complete corrected code for that file
FPGA files first (UART_RX → UART_TX → FSM → Tracker → Top), ESP32 last
After all files done, generate all 6 files together in final version"**


