from gpiozero import Button
from signal import pause
import threading
import time
import sys

# Define the button pins
button_pins = {
    "DOWN": 23,
    "UP": 24,
    "LEFT": 16,
    "RIGHT": 26,
    "ENTER": 27,
    "BACKSPACE": 5,
    "ESCAPE": 6
}

buttons = {}

# Define the callback function for button press
def button_pressed(name):
    print(f"Button {name} pressed!")

# Define the function for handling the button hold
def button_held(name, button):
    while button.is_pressed:
        print(f"Button {name} held!")
        time.sleep(0.1)  # Adjust the rate here

# Define the wrapper function to handle button press and hold
def handle_button(name, button):
    button_pressed(name)  # Immediate response for button press
    # Start a separate thread for handling the hold action
    threading.Thread(target=button_held, args=(name, button)).start()

# Initialize buttons and assign events
for name, pin in button_pins.items():
    try:
        button = Button(pin)
        button.when_pressed = lambda b=button, n=name: handle_button(n, b)
        buttons[name] = button
        print(f"Initialized button {name} on GPIO {pin}")
    except Exception as e:
        print(f"Failed to initialize button {name} on GPIO {pin}: {e}")

# Ensure cleanup on exit
def cleanup(signal, frame):
    print("Cleaning up GPIO...")
    for button in buttons.values():
        button.close()
    sys.exit(0)

import signal
signal.signal(signal.SIGINT, cleanup)

# Wait for events
pause()
