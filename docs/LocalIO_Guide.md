# Local IO Manager - ESP32 Hardware I/O Control

## Описание

**LocalIOManager** управлява всички локални hardware I/O портове на ESP32. Системата предоставя unified interface за работа с различни типове IO и автоматично свързване с PLC променливи.

## Поддържани IO типове

### 1. Digital Input (DI)
- ✅ Direct GPIO read
- ✅ Software debounce (configurable ms)
- ✅ Moving average filter
- ✅ Edge detection (rising/falling/both)
- ✅ Pull-up/pull-down configuration
- ✅ Active high/low (invert)

### 2. Digital Output (DO)
- ✅ Standard GPIO output
- ✅ Pulse output (one-shot with configurable width)
- ✅ Toggle function
- ✅ Safe state on error/shutdown
- ✅ Active high/low (invert)

### 3. Analog Input (AI)
- ✅ 12-bit ADC (0-4095)
- ✅ Multiple voltage ranges (0-1.1V, 0-2.2V, 0-3.3V, 0-6V)
- ✅ Moving average filter
- ✅ Calibration (offset + scale)
- ✅ Engineering units conversion
- ✅ Only ADC1 (GPIO 32-39) - WiFi compatible

### 4. Analog Output (AO)
- ⚠️ Via PWM (any GPIO) - **Implemented as PWM**
- ⚠️ True DAC (GPIO 25, 26 only) - **TODO**

### 5. PWM Output
- ✅ Configurable frequency (1Hz - 40MHz)
- ✅ Configurable resolution (1-16 bits)
- ✅ Duty cycle control (0-100%)
- ✅ Frequency change on-the-fly
- ✅ Fade function (smooth transitions)
- ✅ 16 independent channels (ESP32 LEDC)

### 6. Pulse Counter
- ✅ Hardware counting (ESP32 PCNT)
- ✅ Rising/falling/both edge detection
- ✅ Counter mode (total pulses)
- ✅ Frequency mode (Hz)
- ✅ Period mode (ms)
- ✅ Glitch filter (configurable ns)
- ✅ 8 independent counters

## Architecture

```
┌────────────────────────────────────────────────────────┐
│                  LocalIOManager                        │
├────────────────────────────────────────────────────────┤
│  • Configuration loading (JSON)                        │
│  • Pin management                                      │
│  • Periodic I/O scanning                               │
│  • Error handling                                      │
│  • Safety functions                                    │
└────────────────────────────────────────────────────────┘
                         │
         ┌───────────────┴───────────────┐
         │          IOPinBase            │  (Abstract base)
         │  • begin()                    │
         │  • update()                   │
         │  • getState()                 │
         │  • setValue()                 │
         └───────────────┬───────────────┘
                         │
    ┌────────────────────┼────────────────────┐
    │                    │                    │
┌───▼───────┐  ┌────────▼─────┐  ┌──────────▼────┐
│ Digital   │  │  Analog      │  │  PWM          │
│ InputPin  │  │  InputPin    │  │  OutputPin    │
├───────────┤  ├──────────────┤  ├───────────────┤
│ • Debounce│  │ • ADC config │  │ • LEDC config │
│ • Filter  │  │ • Filtering  │  │ • Duty cycle  │
│ • Edge    │  │ • Calibrate  │  │ • Frequency   │
└───────────┘  └──────────────┘  └───────────────┘

┌──────────────┐  ┌──────────────┐
│ Digital      │  │ Pulse        │
│ OutputPin    │  │ CounterPin   │
├──────────────┤  ├──────────────┤
│ • Set/Reset  │  │ • PCNT HW    │
│ • Pulse      │  │ • Frequency  │
│ • Safe state │  │ • Period     │
└──────────────┘  └──────────────┘
```

## Configuration

### JSON Format

```json
{
  "digital_inputs": [...],
  "digital_outputs": [...],
  "analog_inputs": [...],
  "pwm_outputs": [...],
  "pulse_counters": [...],
  "plc_mapping": {...}
}
```

### Digital Input Example

```json
{
  "name": "button1",
  "pin": 15,
  "mode": "INPUT_PULLUP",
  "invert": true,
  "debounce_ms": 50,
  "filter_samples": 5,
  "description": "Push button"
}
```

**Parameters:**
- `name` - Unique pin name
- `pin` - GPIO number (0-39)
- `mode` - `"INPUT"`, `"INPUT_PULLUP"`, `"INPUT_PULLDOWN"`
- `invert` - `true` for active low
- `debounce_ms` - Debounce time (0 = disabled)
- `filter_samples` - Filter size (0 = no filter)

### Digital Output Example

```json
{
  "name": "relay1",
  "pin": 23,
  "invert": false,
  "initial_state": false,
  "pulse_width_ms": 100,
  "safe_state": false,
  "description": "Main relay"
}
```

**Parameters:**
- `name` - Unique pin name
- `pin` - GPIO number (0-33, not 34-39)
- `invert` - `true` for active low
- `initial_state` - State at boot
- `pulse_width_ms` - Pulse duration
- `safe_state` - Safe state on error

### Analog Input Example

```json
{
  "name": "temp_sensor",
  "pin": 36,
  "range": "0-3.3V",
  "resolution": 12,
  "sample_rate": 100,
  "filter_samples": 10,
  "calib_offset": 0.0,
  "calib_scale": 1.0,
  "min_value": 0.0,
  "max_value": 100.0,
  "description": "Temperature sensor"
}
```

**Parameters:**
- `pin` - GPIO 32-39 (ADC1 only!)
- `range` - `"0-1.1V"`, `"0-2.2V"`, `"0-3.3V"`, `"0-6V"`
- `resolution` - ADC bits (9-12)
- `sample_rate` - Hz
- `filter_samples` - Moving average size
- `calib_offset`, `calib_scale` - Calibration
- `min_value`, `max_value` - Engineering units

### PWM Output Example

```json
{
  "name": "fan_speed",
  "pin": 18,
  "frequency": 25000,
  "resolution": 10,
  "channel": 0,
  "initial_duty": 0.0,
  "description": "Fan control"
}
```

**Parameters:**
- `pin` - GPIO 0-33
- `frequency` - PWM frequency (Hz)
- `resolution` - Bits (1-16)
- `channel` - LEDC channel (0-15)
- `initial_duty` - Initial duty cycle (0-100%)

### Pulse Counter Example

```json
{
  "name": "water_flow",
  "pin": 25,
  "edge": "RISING",
  "mode": "FREQUENCY",
  "sample_window_ms": 1000,
  "enable_filter": true,
  "filter_threshold_ns": 1000,
  "description": "Flow meter"
}
```

**Parameters:**
- `edge` - `"RISING"`, `"FALLING"`, `"BOTH"`
- `mode` - `"COUNTER"`, `"FREQUENCY"`, `"PERIOD"`
- `sample_window_ms` - Frequency measurement window
- `enable_filter` - Glitch filter
- `filter_threshold_ns` - Filter threshold

## API Reference

### Initialization

```cpp
#include "LocalIO/LocalIOManager.h"

LocalIOManager ioManager;

void setup() {
    // Initialize
    ioManager.begin();

    // Load configuration
    ioManager.loadConfigFromFile("/config/local_io.json");
    // or
    ioManager.loadConfig(jsonString);
}

void loop() {
    // Update all IO (call every 10ms recommended)
    ioManager.loop();
}
```

### Digital I/O

```cpp
// Read digital input
bool buttonPressed = ioManager.readDigital("button1");

// Write digital output
ioManager.writeDigital("relay1", true);

// Get raw pin object for advanced operations
IOPinBase* pin = ioManager.getPin("button1");
DigitalInputPin* diPin = dynamic_cast<DigitalInputPin*>(pin);
if (diPin) {
    bool edgeDetected = diPin->getEdgeDetected();
}

// Trigger pulse output
DigitalOutputPin* doPin = dynamic_cast<DigitalOutputPin*>(
    ioManager.getPin("valve_control")
);
if (doPin) {
    doPin->triggerPulse(500); // 500ms pulse
}
```

### Analog I/O

```cpp
// Read analog input (engineering units)
float temperature = ioManager.readAnalog("temp_sensor");

// Get raw ADC value
AnalogInputPin* aiPin = dynamic_cast<AnalogInputPin*>(
    ioManager.getPin("temp_sensor")
);
if (aiPin) {
    int rawADC = aiPin->getRawADC();
    float voltage = aiPin->getVoltage();
}
```

### PWM

```cpp
// Set PWM duty cycle
ioManager.setPWMDutyCycle("fan_speed", 75.0); // 75%

// Set PWM frequency
ioManager.setPWMFrequency("fan_speed", 30000); // 30kHz

// Advanced: Fade effect
PWMOutputPin* pwmPin = dynamic_cast<PWMOutputPin*>(
    ioManager.getPin("led_brightness")
);
if (pwmPin) {
    pwmPin->fadeTo(100.0, 2000); // Fade to 100% over 2 seconds
}
```

### Pulse Counter

```cpp
// Get pulse count
int32_t totalPulses = ioManager.getPulseCount("water_flow");

// Get frequency
float flowRate = ioManager.getPulseFrequency("water_flow");

// Reset counter
ioManager.resetPulseCounter("water_flow");
```

### Status & Diagnostics

```cpp
// Get all IO status as JSON
String status = ioManager.getStatusJson();

// Get specific pin status
String pinStatus = ioManager.getPinStatusJson("relay1");

// Get memory usage
size_t memUsage = ioManager.getMemoryUsage();

// Enable/disable all IO
ioManager.setEnabled(false);
```

### Safety

```cpp
// Set all outputs to safe state (called on error/shutdown)
ioManager.setSafeState();
```

## PLC Integration

LocalIOManager автоматично интегрира hardware I/O с PLC Engine. IO пиновете се декларират като PLC променливи и се синхронизират автоматично.

### Automatic Mapping

**1. Дефинирайте plc_mapping в IO конфигурация:**

```json
{
  "plc_mapping": {
    "inputs": [
      {
        "io_pin": "button1",
        "plc_var": "DI.Button1",
        "type": "bool"
      },
      {
        "io_pin": "temp_sensor",
        "plc_var": "AI.Temperature",
        "type": "real"
      }
    ],
    "outputs": [
      {
        "io_pin": "relay1",
        "plc_var": "DO.Relay1",
        "type": "bool"
      },
      {
        "io_pin": "fan_speed",
        "plc_var": "AO.FanSpeed",
        "type": "real"
      }
    ]
  }
}
```

**2. Свържете LocalIOManager с PlcEngine:**

```cpp
#include "LocalIO/LocalIOManager.h"
#include "PlcEngine/Engine/PlcEngine.h"

LocalIOManager ioManager;
PlcEngine* plcEngine;

void setup() {
    // Initialize IO Manager
    ioManager.begin();
    ioManager.loadConfigFromFile("/config/local_io_example.json");

    // Initialize PLC Engine
    plcEngine = new PlcEngine(timeManager, meshManager);
    plcEngine->begin();

    // Connect IO to PLC - this automatically:
    // - Declares all PLC variables from plc_mapping
    // - Enables auto-sync between IO and PLC
    PlcProgram* program = plcEngine->getProgram("main_program");
    if (program) {
        PlcMemory& memory = program->getMemory();
        ioManager.setPlcMemory(&memory);
    }

    // Load and run PLC program that uses IO variables
    plcEngine->loadProgram("my_program", plcConfigJson);
    plcEngine->runProgram("my_program");
}

void loop() {
    // Update IO and auto-sync with PLC
    ioManager.loop();
}
```

**3. Използвайте IO променливи в PLC програма:**

```json
{
  "program_name": "temperature_control",
  "memory": {
    "_comment": "IO variables (DI.*, DO.*, AI.*, AO.*) are auto-declared",
    "setpoint": { "type": "real", "initial_value": 22.0 }
  },
  "logic": [
    {
      "comment": "Enable heater if temperature too low",
      "block_type": "LT",
      "inputs": {
        "in1": "AI.Temperature",
        "in2": "setpoint"
      },
      "outputs": {
        "out": "DO.HeaterEnable"
      }
    }
  ]
}
```

**Как работи:**

1. **Configuration Loading**: LocalIOManager зарежда `plc_mapping` от JSON
2. **Variable Declaration**: `setPlcMemory()` автоматично декларира променливи:
   - Input pins → PLC input variables (DI.*, AI.*)
   - Output pins → PLC output variables (DO.*, AO.*)
3. **Auto-Sync**: Всяко извикване на `loop()`:
   - **Inputs**: Hardware → IO Manager → PLC variables
   - **Outputs**: PLC variables → IO Manager → Hardware

**Преимущества:**

✅ Няма нужда от ръчно синхронизиране
✅ PLC програмата работи еднакво с локално IO и mesh endpoints
✅ Configuration-driven: промяна на пинове без код
✅ Type-safe: автоматична конвертация между типове

### Manual Mapping (Alternative)

```cpp
// In PLC program JSON:
{
  "memory": {
    "DI_Button1": { "type": "bool" },
    "AI_Temperature": { "type": "real" },
    "DO_Relay1": { "type": "bool" },
    "AO_FanSpeed": { "type": "real" }
  },
  "io_points": [
    {
      "plc_var": "DI_Button1",
      "endpoint": "local.button1.state.bool",
      "direction": "input",
      "auto_sync": true
    },
    {
      "plc_var": "AI_Temperature",
      "endpoint": "local.temp_sensor.value.real",
      "direction": "input",
      "auto_sync": true
    },
    {
      "plc_var": "DO_Relay1",
      "endpoint": "local.relay1.state.bool",
      "direction": "output",
      "auto_sync": true
    },
    {
      "plc_var": "AO_FanSpeed",
      "endpoint": "local.fan_speed.duty.real",
      "direction": "output",
      "auto_sync": true
    }
  ]
}
```

## ESP32 Pin Reference

### Safe Pins for General Use

```
GPIO 0, 2, 4, 5, 12-15, 16-19, 21-23, 25-27, 32-39
```

### Pin Restrictions

| Function | Suitable Pins | Notes |
|----------|--------------|-------|
| **Digital Input** | 0-39 | Avoid 1, 3, 6-11 |
| **Digital Output** | 0-33 | 34-39 are input-only |
| **Analog Input (ADC1)** | 32-39 | **Use ADC1 only** (WiFi compatible) |
| **Analog Input (ADC2)** | 0, 2, 4, 12-15, 25-27 | **Conflicts with WiFi!** |
| **PWM (LEDC)** | 0-33 | 16 channels available |
| **Pulse Counter (PCNT)** | Any | 8 units available |
| **DAC** | 25, 26 | True analog output |
| **Touch Sensor** | 0, 2, 4, 12-15, 27, 32-33 | Built-in touch |

### Avoid These Pins

- **GPIO 1, 3** - UART (Serial)
- **GPIO 6-11** - Flash memory (SPI)
- **GPIO 34-39** - Input-only (no pull-up/pull-down, no output)

### Built-in Peripherals

- **GPIO 2** - Built-in LED (most boards)
- **GPIO 0** - Boot button
- **GPIO 36 (VP)**, **GPIO 39 (VN)** - No pull resistors

## Performance

### Memory Usage

| Component | RAM | Notes |
|-----------|-----|-------|
| LocalIOManager | ~2KB | Base overhead |
| DigitalInputPin | ~150 bytes | Per pin |
| DigitalOutputPin | ~100 bytes | Per pin |
| AnalogInputPin | ~200 bytes | Per pin + filter |
| PWMOutputPin | ~100 bytes | Per pin |
| PulseCounterPin | ~150 bytes | Per pin |
| **Total (20 pins)** | **~5KB** | Typical |

### Update Performance

| Operation | Time | Notes |
|-----------|------|-------|
| Digital read | <1µs | GPIO direct |
| Digital write | <1µs | GPIO direct |
| Analog read | ~100µs | ADC conversion |
| PWM write | <1µs | Hardware LEDC |
| Pulse counter | <1µs | Hardware PCNT |
| **Full scan (20 pins)** | **~5ms** | All updates |

## Examples

### Example 1: Temperature Controlled Fan

```cpp
LocalIOManager io;

void setup() {
    io.begin();
    io.loadConfigFromFile("/config/local_io.json");
}

void loop() {
    io.loop();

    // Read temperature
    float temp = io.readAnalog("temp_sensor");

    // Control fan speed based on temperature
    float fanSpeed = 0.0;
    if (temp > 40.0) {
        fanSpeed = 100.0; // Max speed
    } else if (temp > 30.0) {
        fanSpeed = (temp - 30.0) * 10.0; // 0-100%
    }

    io.setPWMDutyCycle("fan_speed", fanSpeed);

    delay(100);
}
```

### Example 2: Water Flow Monitoring

```cpp
LocalIOManager io;

void setup() {
    io.begin();
    io.loadConfigFromFile("/config/local_io.json");
}

void loop() {
    io.loop();

    // Get flow rate
    float flowHz = io.getPulseFrequency("water_flow");

    // Convert to liters per minute (assuming 450 pulses/liter)
    float flowLPM = (flowHz * 60.0) / 450.0;

    Serial.printf("Flow: %.2f L/min\n", flowLPM);

    delay(1000);
}
```

### Example 3: Button with Debounce

```cpp
LocalIOManager io;

void setup() {
    io.begin();

    // Configure button with debounce
    DigitalInputConfig config;
    config.pin = 15;
    config.mode = DigitalInputMode::INPUT_PULLUP;
    config.invert = true;
    config.debounceMs = 50;

    auto* button = new DigitalInputPin("button1", config);
    // Pin will be added by io.loadConfig() or manually
}

void loop() {
    io.loop();

    if (io.readDigital("button1")) {
        Serial.println("Button pressed!");
    }

    delay(10);
}
```

## Troubleshooting

### ADC readings unstable

**Problem**: Analog input values fluctuate wildly

**Solutions**:
1. Increase `filter_samples` (try 20-30)
2. Add hardware capacitor (e.g., 100nF) on input
3. Use ADC1 pins (32-39) instead of ADC2
4. Check for proper ground connection
5. Calibrate with `calib_offset` and `calib_scale`

### PWM not working

**Problem**: PWM output doesn't change

**Solutions**:
1. Check pin number (0-33 only, not 34-39)
2. Verify channel number is unique (0-15)
3. Try different frequency (1000-5000 Hz)
4. Check resolution (10 bits = 1024 levels)
5. Verify duty cycle range (0.0-100.0%)

### Pulse counter always zero

**Problem**: Pulse counter doesn't count

**Solutions**:
1. Check signal voltage (3.3V logic required)
2. Verify edge configuration (rising/falling/both)
3. Disable glitch filter if pulses are slow
4. Check pin connection
5. Verify PCNT unit available (max 8)

### Digital input always high/low

**Problem**: Input stuck at one value

**Solutions**:
1. Check pullup/pulldown configuration
2. Verify `invert` setting
3. Check physical connection
4. Try different pin
5. Test with multimeter (expect 0V or 3.3V)

## Best Practices

### 1. Pin Selection

- ✅ Use ADC1 pins (32-39) for analog inputs
- ✅ Reserve GPIO 0, 2 for debugging
- ✅ Avoid strapping pins (0, 2, 5, 12, 15) if possible
- ✅ Group related I/O physically

### 2. Configuration

- ✅ Use descriptive pin names (`temp_sensor`, not `ai1`)
- ✅ Document pin functions in JSON
- ✅ Set safe states for all outputs
- ✅ Configure appropriate debounce times

### 3. Performance

- ✅ Call `io.loop()` every 10ms
- ✅ Use hardware features (PWM, PCNT) instead of software
- ✅ Filter analog inputs to reduce noise
- ✅ Batch IO operations when possible

### 4. Safety

- ✅ Define safe states for all outputs
- ✅ Call `setSafeState()` on error
- ✅ Use watchdog timer
- ✅ Validate analog input ranges

## Future Enhancements

- [ ] True DAC output (GPIO 25, 26)
- [ ] Touch sensor support
- [ ] Hall sensor support
- [ ] Temperature sensor (internal)
- [ ] Capacitive touch calibration
- [ ] ADC2 support (with WiFi coordination)
- [ ] Interrupt-based digital inputs
- [ ] DMA for high-speed analog sampling

---

**LocalIOManager v1.0** - Hardware I/O для ESP32 🔌
