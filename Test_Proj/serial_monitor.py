"""Display Pico USB serial output and reconnect after USB is unplugged."""

import argparse
import codecs
import time

import serial
from serial.tools import list_ports


PICO_USB_VENDOR_ID = 0x2E8A
RETRY_SECONDS = 1


def find_device(preferred_port, identity):
    """Find a matching serial device without opening or reading the port.

    Args:
        preferred_port: Initial device path, such as /dev/ttyACM0.
        identity: Remembered (USB vendor ID, serial number), or None initially.

    Returns:
        Port metadata for the remembered board, preferred initial path, or a
        single unambiguous Pico; None when no suitable device is available.

    A remembered identity takes priority over tty numbering after reconnect.
    """
    ports = list(list_ports.comports())

    if identity is not None:
        # Match the board itself even if Linux changes its ttyACM number.
        for port in ports:
            if (port.vid, port.serial_number) == identity:
                return port
        return None

    for port in ports:
        if port.device == preferred_port:
            return port

    # Before the first connection, only auto-select an unambiguous Pico port.
    candidates = [port for port in ports if port.vid == PICO_USB_VENDOR_ID]
    if len(candidates) == 1:
        return candidates[0]
    return None


def monitor(preferred_port, baud):
    """Stream USB serial text and retry when the device is missing or unplugged.

    Args:
        preferred_port: Starting device path; remembered identity can replace it.
        baud: Serial port setting passed to pyserial, not the Pico SPI clock.

    Prints decoded UTF-8 to the terminal and remembers the board identity.
    Reads have a timeout; connection errors trigger a retry delay. The port is
    closed by its context manager on errors or Ctrl+C. Normally runs forever;
    KeyboardInterrupt propagates to main(). Does not send data to the Pico.
    """
    identity = None
    last_error = None
    print("Waiting for Pico... (Ctrl+C to quit)", flush=True)

    while True:
        device = find_device(preferred_port, identity)
        if device is None:
            time.sleep(RETRY_SECONDS)
            continue

        # Remember metadata before opening, in case USB disappears mid-open.
        if device.serial_number:
            identity = (device.vid, device.serial_number)
        preferred_port = device.device

        try:
            # A timeout lets the loop remain responsive without requiring data.
            # The context manager closes the port on disconnect or Ctrl+C.
            with serial.Serial(device.device, baud, timeout=0.5) as connection:
                last_error = None
                print(f"Connected to {device.device}", flush=True)
                # Preserve characters split across reads; replace invalid bytes.
                decoder = codecs.getincrementaldecoder("utf-8")(errors="replace")
                while True:
                    data = connection.read(256)
                    if data:
                        print(decoder.decode(data), end="", flush=True)
                    # An empty timed-out read is normal, not a disconnect.
        except (serial.SerialException, OSError) as error:
            # Avoid flooding the terminal with identical connection errors.
            message = str(error)
            if message != last_error:
                print(f"\nSerial connection unavailable: {message}", flush=True)
                print("Waiting for Pico...", flush=True)
                last_error = message
            time.sleep(RETRY_SECONDS)


def main():
    """Parse command-line settings and run the reconnecting serial monitor.

    Accepts optional port and baud arguments, defaulting to /dev/ttyACM0 and
    115200. Handles Ctrl+C by printing a stop message and returning None;
    malformed arguments are handled by argparse.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", nargs="?", default="/dev/ttyACM0")
    parser.add_argument("baud", nargs="?", type=int, default=115200)
    args = parser.parse_args()

    try:
        monitor(args.port, args.baud)
    except KeyboardInterrupt:
        print("\nSerial monitor stopped.")


if __name__ == "__main__":
    main()
