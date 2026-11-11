# ESP32-S3 WiFi Home Control

A small, cloud-free controller for four relays. An ESP32-S3 DevKitC-1 runs a web server, and a phone opens
the control page in its browser, with no app to install.

- Four tiles with animated icons: **Bell** (rings for a fixed 1.5 seconds, then switches itself off), **Fan**, and two spare switches.
- **Voice and typed commands**, such as "bell", "fan on" or "everything off".
- **Works anywhere:** the board creates its own WiFi network (`HomeControl`, page at `192.168.4.1`), and it also joins your home or college WiFi automatically.
- Everything runs on the board: no cloud service, no external files.
- Built with PlatformIO and the Arduino framework. The full firmware is one file (`src/main.cpp`), and the repo includes a wiring guide and a project report.

Full write-up: see `Project_Report.pdf` in this folder.

> **Safety:** wire mains loads only through the relay's COM and NO terminals, in an insulated enclosure, with the power off.
> The page has no login, so anyone on the same network can use it.

## Folder contents

| File | What it is |
|---|---|
| `src/main.cpp` | The complete firmware: relay control, WiFi, web server and the web page |
| `platformio.ini` | PlatformIO project settings (board, framework, serial speed) |
| `Project_Report.pdf` | The project report |
| `README.md` | This file |

## Hardware

- ESP32-S3 DevKitC-1 (USB-C)
- 4-channel relay module (active-LOW, 5 V)
- 5 V USB power supply (1 A or more)
- The devices to switch (bell, fan, ...)

### Wiring

| Relay module | ESP32-S3 |
|---|---|
| VCC | 5V |
| GND | GND |
| IN1 (Bell) | GPIO 4 |
| IN2 (Fan) | GPIO 5 |
| IN3 (Switch 3) | GPIO 6 |
| IN4 (Switch 4) | GPIO 7 |

Wire each load through the relay's **COM** and **NO** terminals (the live/positive wire only).

> **Safety:** if you switch mains voltage, use an insulated enclosure and switch the power off before wiring.
> If you are not experienced with mains wiring, test with a low-voltage load first.

## Build and upload

### Option A: PlatformIO (VS Code)

1. Install the **PlatformIO IDE** extension in VS Code.
2. Open this folder in VS Code.
3. Open `src/main.cpp` and set your WiFi name and password in `KNOWN_WIFI` (top of the file).
4. Plug the board in and click **Upload** (the arrow in the bottom status bar).

### Option B: Arduino IDE

1. Install the **esp32 by Espressif Systems** board package.
2. Create a new sketch and paste the contents of `src/main.cpp`.
3. Board: **ESP32S3 Dev Module**. Set **USB CDC On Boot** to **Enabled**.
4. Upload.

If the upload cannot connect, hold **BOOT**, tap **RST**, release **BOOT**, then upload again.
If the board shows no COM port, use a **data** USB cable and the USB-C port labelled **USB**.
The port labelled **UART** needs the Silicon Labs CP210x driver.

## Using it

The board always creates its own WiFi network, and also tries to join the networks listed in `KNOWN_WIFI`.

| Way to connect | How |
|---|---|
| Directly to the board | Join WiFi **HomeControl** (password `homecontrol123`), open `http://192.168.4.1` |
| Through your router | Join the same WiFi as the board and open the IP address it prints on the serial monitor (115200 baud) |

- **Bell:** tap to ring for 1.5 seconds. It switches itself off. It cannot be held on.
- **Fan, Switch 3, Switch 4:** tap to switch on or off.
- **All off:** switches everything off.
- **Voice / typed commands:** for example "bell", "fan on", "fan off", "switch 3 off", "everything off".

### Voice notes

Voice recognition runs in the phone's browser, not on the board. Chrome blocks the microphone on plain `http`
pages. In Chrome, open `chrome://flags/#unsafely-treat-insecure-origin-as-secure`, add the board's address,
enable it and relaunch. Recognition also needs internet or an offline language pack. On iPhone use the keyboard's
dictation key in the "Type a command" box instead. Typed commands always work.

## HTTP interface

| URL | Action |
|---|---|
| `/` | The control page |
| `/api/state` | JSON list of the devices and their state |
| `/api/set?id=1&state=1` | Switch device `id` (0 = Bell, 1 = Fan, 2 = Switch 3, 3 = Switch 4) on (1) or off (0) |
| `/api/ring?id=0` | Ring the Bell for its set time |
| `/api/alloff` | Switch everything off |

## Settings you may want to change (top of `src/main.cpp`)

- `AP_SSID`, `AP_PASS`: the board's own network name and password.
- `KNOWN_WIFI`: the WiFi networks the board should join (2.4 GHz, plain password only).
- `devices[]`: names, GPIO pins and the Bell ring time (`1500` is milliseconds).
- `RELAY_ON` / `RELAY_OFF`: swap `LOW` and `HIGH` if your relay module is active-HIGH.

## Known limits

- There is no password on the control page. Anyone on the same network who knows the board's address can use it.
- It works only on the local network. Control from outside needs an internet service, which is not part of this project.
- The board's WiFi reception is weak at a distance, so keep it close to the router.
