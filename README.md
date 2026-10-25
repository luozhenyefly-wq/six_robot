# six_robot — STM32 Hexapod Robot Control Firmware

**English** | [中文](README.zh-CN.md)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A hexapod (six-legged) robot motion controller based on the **STM32F407VET6**. Omnidirectional movement is achieved with a **tripod gait**: 18 bus servos drive six 3-DOF legs, a JY61P attitude sensor closes the heading loop, FreeRTOS handles multitasking, and Bluetooth remote control is supported. The firmware serves as the low-level motion platform for SLAM mapping and autonomous navigation.

> 📦 **Host-side repository (ROS 2 mapping & navigation)**: [luozhenyefly-wq/six_robot_slam](https://github.com/luozhenyefly-wq/six_robot_slam)
>
> **Division of the two repositories**: this repository, `six_robot`, is the **lower-level controller** — the STM32 firmware handles gait planning, servo driving, attitude solving and odometry data acquisition. [`six_robot_slam`](https://github.com/luozhenyefly-wq/six_robot_slam) is the **host side**, running on a D-Robotics RDK X5 (aarch64 / Ubuntu 22.04 / ROS 2 Humble): it reads the LiDAR and the serial frames from this board, fuses them with robot_localization (EKF) into odometry, builds a 2D occupancy grid with slam_toolbox, and reserves a Nav2 navigation interface.
>
> The two halves communicate over **USART6 (115200, 8-byte frame / 20 ms period)**; the protocol is described in section **6.2** below.

---

## 1. Hardware

| Item | Specification |
|---|---|
| MCU | STM32F407VET6 (LQFP100, 512 KB Flash / 192 KB RAM, Cortex-M4F) |
| System clock | **168 MHz** (HSI 16 MHz → PLLM=8 / PLLN=168 / PLLP=2, APB1=42 MHz, APB2=84 MHz) |
| Mechanics | Hexapod, 3 DOF per leg (J1 yaw / J2 hip / J3 knee), **18 servos** in total |
| Servos | 270° bus servos; PWM 500–2500 maps to 0–270° (centre 1500 = 135°) |
| Attitude sensor | JY61P (UART4, 115200; angle frame `0x53` and angular-rate frame `0x52`) |
| Display | 0.96" OLED, SSD1306 / 128×64 (**software I2C**: PB8=SCL, PB9=SDA; pins managed via CubeMX User Labels) |
| Remote control | HC-05 Bluetooth serial module (USART3, 9600) |
| SLAM host | USART6, 115200 (pose and odometry output to the mapping/navigation host: Yaw / vYaw / displacement increment) |
| Debug | SWD (PA13=SWDIO, PA14=SWCLK) |

### UART allocation

| Port | Baud | Purpose | Pins |
|---|---|---|---|
| USART2 | 9600 | General-purpose serial | PA2 / PA3 |
| USART3 | 9600 | Bluetooth remote control | PB10 / PB11 |
| UART4 | 115200 | JY61P attitude sensor | PC10 / PC11 |
| UART5 | 115200 | Servo control board (TX only) | PC12 (TX) / PD2 (RX) |
| USART6 | 115200 | SLAM host (pose & odometry) | PC6 / PC7 |

> **Note**: this project runs from the internal **HSI** (16 MHz) multiplied by the PLL to 168 MHz; no external crystal is used. If your board has a 25 MHz crystal, switch RCC to HSE in `CubeMX` and regenerate (adjusting PLLM accordingly; `six_robot.ioc` already records `HSE_VALUE=25MHz`).

---

## 2. Repository layout

```
six_robot/
├── Core/                  # CubeMX-generated HAL init and interrupts
│   ├── Inc/               #   main.h / usart.h / tim.h / gpio.h / FreeRTOSConfig.h
│   └── Src/               #   main.c / freertos.c / usart.c / tim.c / gpio.c / ISRs
├── User/                  # ★ Application layer (the core of this project, all hand-written)
├── Drivers/               # STM32F4xx HAL drivers (managed by CubeMX)
├── Middlewares/           # FreeRTOS + CMSIS-RTOS v2
├── MDK-ARM/               # Keil MDK project (six_robot.uvprojx)
├── tools/                 # Helper scripts (J-Link environment fix, Git cheatsheet)
├── six_robot.ioc          # STM32CubeMX project configuration ★
└── .mxproject             # CubeMX-generated dependency records
```

### `User/` application modules

| File | Responsibility |
|---|---|
| `leg_gait.c/h` | **Tripod gait core**: foot trajectory planning, group command packing, heading PID loop, parking sequence |
| `leg_angle.c/h` | Per-leg inverse kinematics (`Leg_IK`), body→leg frame transform (`Body_To_Leg`) |
| `leg.c/h` | Servo driver layer: angle/PWM conversion, serial command transmission |
| `JY61P.c/h` | Attitude sensor parsing (angle / angular-rate frames, checksum, boot-time yaw zeroing) |
| `bt_control.c/h` | Bluetooth command parsing (single-character commands + angle-entry state machine) |
| `PID.c/h` | Generic PID controller |
| `odom.c/h` | Incremental odometry (measurement and accumulation only; coordinate propagation is done host-side by SLAM/navigation) |
| `balance.c/h` | Body attitude / levelling adjustment |
| `OLED.c/h`, `OLED_Data.c/h` | OLED driver and font data (adapted from the open-source code by 江协科技) |
| `face.c/h` | On-screen facial expressions (7 emotions) |
| `key.c/h` | Key scanning (start/stop) |

---

## 3. Build and flash

**Requirements**: Keil MDK 5 + ARM Compiler 5/6 + STM32F4xx DFP

1. Open `MDK-ARM/six_robot.uvprojx`
2. Make sure the target is `six_robot` and the device is `STM32F407VETx`
3. Build (F7) → download (F8, J-Link / ST-Link)

> **J-Link users**: `tools/` ships `apply-jlink-fix.bat` and `fix-jlink-env.ps1` to work around download failures caused by J-Link environment variables.

**Note**: build-output directories such as `MDK-ARM/six_robot/` and `MDK-ARM/robot/` are excluded by `.gitignore` (about 138 MB) and are regenerated on the first build after cloning.

To change the peripheral configuration, open `six_robot.ioc` in STM32CubeMX and regenerate **while keeping the USER CODE blocks intact**.

---

## 4. Software architecture

### FreeRTOS tasks

Built on the **CMSIS-RTOS v2** wrapper generated by CubeMX; heap size 40 KB, FPU enabled.

| Task | Priority | Stack (words) | Period | Responsibility |
|---|---|---|---|---|
| `leg_gait` | 14 (highest) | 512 | 1 ms loop | Main gait loop: trajectory solving + servo command dispatch + odometry accumulation |
| `yaw_send` | 13 | 256 | 20 ms | Sends pose and displacement increments to the SLAM host |
| `bt_control` | 12 | 256 | 20 ms | Bluetooth command parsing, angle-entry timeout check |
| `key` | 10 | 128 | 20 ms | Key scanning |
| `defaultTask` | 9 | 256 | 70 ms | OLED status refresh |

Tasks communicate through global `volatile` variables and a message queue (`bt_msg_Queue`, 16×uint8); servo commands are packed into a **static buffer** to avoid consuming task stack.

### Control main loop

```
main()
 ├─ HAL_Init / SystemClock_Config (168 MHz)
 ├─ Peripheral init: GPIO / UART4,5,2,3,6 / TIM1,9,12,13
 ├─ Application init: OLED_Init → JY61P_Init → boot-time yaw zeroing
 │              → PID_Init → LEG_Init → Bt_Init
 ├─ TIM1 four-channel PWM start + update interrupt (10 ms)
 └─ osKernelStart()
```

---

## 5. Gait algorithm

### Tripod gait

The six legs are split by phase into groups **A / B** that alternately support the body, so at least three legs are always on the ground:

```
Group A: front-left (1,2,3)  · rear-left (7,8,9)      · middle-right (13,14,15)
Group B: middle-left (4,5,6) · front-right (10,11,12) · rear-right (16,17,18)
```

### Key parameters (`leg_gait.h`)

| Macro | Value | Meaning |
|---|---|---|
| `GAIT_N` | 8 | Frames per gait cycle |
| `GAIT_CYCLE_MS` | 1000 | Cycle length (ms), i.e. 125 ms per frame |
| `GAIT_SWING_FRAC` | 0.35 | Swing-phase fraction (stance phase 0.65) |
| `GAIT_Z_STAND` | −6.0 | Foot Z when standing; body 60 mm above the ground |
| `GAIT_Z_LIFT` | −1.5 | Foot lift height during swing; 45 mm above the ground |
| `HOLD_ENTER_DEG` / `HOLD_EXIT_DEG` | 1.5 / 3.5 | Enter/exit thresholds of the heading-hold loop (hysteresis to avoid jitter) |

### Foot trajectory (`GaitTraj`)

- **Swing phase** (s < 0.35): lift and forward swing occur simultaneously (cosine interpolation), tracing an arc — slow at both ends, fast in the middle, which gives a biomimetic feel
- **Stance phase** (s ≥ 0.35): **strictly constant-speed** backward motion — with the feet planted the body advances at constant speed; adding easing here would make the robot lurch

### Heading closed loop

JY61P supplies Yaw; `Zhuanwan_PID` performs heading PID control and switches parameter sets according to the motion state (different Kp/Ki/Kd for forward / backward / no-load). The heading loop is **evaluated every tick** (keeping the derivative continuous) but only takes effect at the phase point where the foot offset `d≈0`, so the legs never change abruptly mid-swing.

### Parking sequence

A stop request does not cut the output immediately: the controller waits until the phase reaches the safe point `d≈0`, brakes there (stride locked to 0, feet dropped vertically), then advances 3 ticks to let the airborne legs land, ensuring a stable final posture.

---

## 6. Communication protocols

### 6.1 Servo control (UART5, 115200)

**Group commands** are sent to the bus-servo control board, up to 9 servos per frame:

```
{#001P1500T0600!#002P1800T0600!...}
```

| Field | Meaning |
|---|---|
| `#nnn` | Servo ID (001~018) |
| `Pxxxx` | Target PWM (500~2500, automatically clamped) |
| `Txxxx` | Travel time (ms, max 9999) |

A 9-servo frame is about 138 bytes. When an angle is out of range (<0 or >270) that joint is **skipped instead of sent**, rather than being clamped to the end-stop and stalling the servo.

### 6.2 SLAM / navigation data output (USART6, 115200, 20 ms period)

Localization and mapping are performed host-side ([`six_robot_slam`](https://github.com/luozhenyefly-wq/six_robot_slam)); this board only supplies raw kinematic and attitude data. Fixed 8-byte frame, every quantity scaled by 100 and transmitted as big-endian int16:

| Byte | Content |
|---|---|
| 0 | `0xAA` frame header |
| 1~2 | Yaw × 100 (int16, big-endian) |
| 3~4 | vYaw × 100 (yaw rate, deg/s) |
| 5~6 | Δs × 100 (displacement increment of this frame, cm, forward positive) |
| 7 | `0xBB` frame trailer |

**Layered design**: the firmware only measures and accumulates — **no trigonometry, no coordinate transforms**; coordinate propagation (integrating Yaw / vYaw into x / y, including arc integration and crab-walk compensation) all lives in the host-side SLAM and navigation program, so calibration changes require no re-flashing. Odometry scale factor:

```c
#define ODOM_K_STRIDE   1.538f   // = 1/(1-0.35), stride → actual displacement
```

> Host-side parsing: `Yaw = (int16)(b1<<8|b2)/100.0`, `ds = (int16)(b5<<8|b6)/100.0`

### 6.3 Bluetooth remote control (USART3, 9600)

**Single-character commands** (`bt_go_flag` is a master switch; motion commands are ignored until it is on):

| Command | Function |
|---|---|
| `F` / `f` | Forward (10 cm stride) |
| `B` / `b` | Backward (−10 cm) |
| `L` / `l` | Open-loop continuous left turn |
| `R` / `r` | Open-loop continuous right turn |
| `S` / `s` | Open-loop stop |
| `H` / `h` | Closed-loop stop (halt while holding the current heading) |
| `A` + angle | Turn to an absolute angle, e.g. `A90`, `A-45` (entry ends on Enter / a non-digit, or after 100 ms without a new character) |
| `1`~`7` | OLED faces: normal / happy / sad / angry / surprised / sleeping / goofy |

Angle entry uses a three-state machine (`IDLE → SIGN → NUM`) that supports a minus sign and ±180° normalization; invalid input is discarded.

---

## 7. Posture and kinematics

- **Boot-time zero**: `Serial_JY61P_Zero_Yaw()` is called once in `main()` and once when the gait task starts, taking the current heading as the 0° reference
- **Per-leg inverse kinematics**: `Leg_IK(x, y, z, L1, L2, L3, solution, left_or_right)` — takes the foot position in the leg frame and returns the three joint angles J1/J2/J3
- **Coordinate transform**: `Body_To_Leg()` converts a target position in the body frame into each leg's frame (including mounting azimuth `psi` and mounting offsets `mx/my/mz`)
- **Leg mounting parameters** live in `leg_cfg[]` in `leg_gait.c`: neutral position (X0, Y0) of each of the six legs, servo ID mapping, left/right flag and phase group

---

## 8. Known caveats

- The servos are the **270°** variant; if you swap in 180° servos, change `SERVO_MAX_DEG` in `leg.h`
- `GAIT_DWELL` (inter-frame dwell) **must not be removed** — it prevents the servo control board's serial buffer from overflowing
- PID parameters for high-speed forward motion (`gait_v = 15`) were tuned separately and are less stable; use with care
- **All sources are UTF-8 (no BOM)**: 21 files under `User/` were originally GBK (Keil's default on Chinese Windows) and were converted to UTF-8 in commit `5a524d6`, so Chinese text displays correctly both on GitHub and in Keil. Please keep new files UTF-8
- If the OLED is replaced with a 1.3" panel, set `OLED_IS_SH1106` to `1` in `OLED.c` (the SH1106 has 132 columns and needs a 2-column offset); if the screen stays dark, increase `OLED_I2C_DELAY_US`

---

## 9. References

- `tools/git-cheatsheet.md` — Git operations commonly used in this project
- STM32CubeMX project: `six_robot.ioc`
- Firmware package: STM32Cube FW_F4 V1.28.3

---

## License

This project is released under the **MIT License** — free to use, modify and distribute (including commercially), provided the copyright notice is retained. See [LICENSE](LICENSE).

### Third-party components

The following parts are third-party code and **remain under their own licenses**; they are not covered by this project's MIT license:

| Component | Path | License |
|---|---|---|
| STM32F4xx HAL drivers | `Drivers/STM32F4xx_HAL_Driver/` | BSD-3-Clause (© STMicroelectronics) |
| CMSIS | `Drivers/CMSIS/` | Apache-2.0 (© ARM Limited) |
| FreeRTOS kernel and CMSIS-RTOS v2 | `Middlewares/Third_Party/FreeRTOS/` | MIT (© Amazon.com, Inc.) |
| OLED driver | `User/OLED.c`, `OLED.h`, `OLED_Data.*` | Adapted from the open-source code by 江协科技 |
| STM32CubeMX generated code | `Core/`, `MDK-ARM/`, `six_robot.ioc` | ST template terms |

Please retain the license files and copyright notices of the components above when redistributing.
