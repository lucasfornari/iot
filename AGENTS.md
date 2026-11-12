# Repository Guidelines

## Project Structure & Module Organization

- `src/main.cpp`: Arduino firmware for the ESP32, DHT11, SSD1306 OLED, and MAX4466 microphone. The current firmware runs offline; it does not publish MQTT telemetry.
- `platformio.ini`: board configuration, serial ports, and Adafruit dependencies.
- `include/` and `lib/`: shared headers and project-specific libraries.
- `data/`: complementary web dashboard and bundled MQTT JavaScript client.
- `backend/app.py`: Python HTTP API, static-file server, optional MQTT collector, and SQLite storage (`backend/dados.db`).
- `mqtt/`: Mosquitto broker configuration and Docker Compose service.
- `test/`: PlatformIO test directory; currently contains only a README.

## Build, Test, and Development Commands

Run commands from the repository root:

- `pio run`: compile firmware for `esp32doit-devkit-v1`.
- `pio run -t upload`: compile and flash the connected ESP32.
- `pio device monitor`: inspect serial output at 115200 baud. Adjust `/dev/ttyACM0` in `platformio.ini` for your machine.
- `python3 backend/app.py`: start the dashboard and API at `http://localhost:5000`; initialize SQLite automatically. HTTP operation requires only Python's standard library; MQTT collection optionally uses `paho-mqtt`.
- `docker compose -f mqtt/compose.yaml up -d`: start Mosquitto on TCP 1883 and WebSocket 9001.
- `pio test`: run PlatformIO tests once test cases have been added.

## Coding Style & Naming Conventions

Use two-space indentation in C++ and four spaces in Python. Follow existing Portuguese identifiers and comments: camelCase for firmware functions/variables, snake_case for Python functions, and UPPER_SNAKE_CASE for constants. Preserve surrounding HTML/CSS/JavaScript style. No formatter or linter is configured. Keep firmware timing compatible with periodic sensor reads and acoustic detection.

## Testing Guidelines

No automated test cases or coverage threshold currently exist. Build firmware changes with `pio run`, then verify single-clap temperature, double-clap humidity, and five-second standby return on hardware. For backend changes, check `/api/status` and POST/GET `/api/leituras`; verify dashboard history filters for UI changes. Document checks and hardware limitations in the PR. Place future firmware suites under `test/test_<feature>/`.

## Commit & Pull Request Guidelines

History uses prefixes such as `feat:`, `refactor:`, and `docs:` with concise Portuguese descriptions. Follow that pattern. PRs should explain behavior changes, list validation performed, link relevant issues, and include screenshots or hardware observations for display/dashboard changes. Update `README.md` when pin assignments or setup steps change.

## Security & Configuration Tips

Keep credentials out of commits, including changes to `include/credenciais.h`. Leave generated databases and `.pio/` artifacts untracked. Use `MQTT_HOST` and `MQTT_PORT` for backend broker configuration. Mosquitto permits anonymous access; restrict it to a trusted development network.
