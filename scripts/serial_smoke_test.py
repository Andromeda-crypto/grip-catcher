import time
import serial

PORT = "/dev/cu.usbserial-0001"
TESTS = [
    ("p", "PONG"),
    ("s", "Stopped"),
    ("?", "Stopped"),
    ("r", "Running"),
    ("?", "Running"),
    ("s", "Stopped"),
    ("?", "Stopped"),
]

try:
    with serial.Serial(PORT,115200, timeout=2, write_timeout=2) as esp:
        time.sleep(2) # allow esp32 to finish booting
        esp.reset_input_buffer() # clear any boot messages

        for command, expected in TESTS:
            esp.write(command.encode("utf-8"))
            reply = esp.readline().decode("utf-8").strip()
            print(f"Sent: {command}, received: {reply}")

            if reply != expected:
                raise SystemExit(f"Fail: expected {expected!r}; received {reply!r}"
                )

    print("Pass: All tests completed successfully. ")

except serial.SerialException as e:
    raise SystemExit(f"Serial error: {e}")