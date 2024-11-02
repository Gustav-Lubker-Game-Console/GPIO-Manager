#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <wiringPi.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define BUTTON_DOWN 23
#define BUTTON_UP 24
#define BUTTON_LEFT 16
#define BUTTON_RIGHT 26
#define BUTTON_ENTER 27
#define BUTTON_BACKSPACE 5
#define BUTTON_ESCAPE 6

int uinput_fd = -1;
int keycodes[30]; // Assuming GPIO pins range up to 29
int debounce_time = 50; // 50 ms debounce time

int setup_uinput_device() {
    struct uinput_user_dev uidev;
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) {
        perror("ioctl");
        close(fd);
        return -1;
    }

    int keys[] = {KEY_DOWN, KEY_UP, KEY_LEFT, KEY_RIGHT, KEY_ENTER, KEY_BACKSPACE, KEY_ESC};
    for (int i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        if (ioctl(fd, UI_SET_KEYBIT, keys[i]) < 0) {
            perror("ioctl");
            close(fd);
            return -1;
        }
    }

    memset(&uidev, 0, sizeof(uidev));
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "gpio-keyboard");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor = 0x1234;
    uidev.id.product = 0x5678;
    uidev.id.version = 1;

    if (write(fd, &uidev, sizeof(uidev)) < 0) {
        perror("write");
        close(fd);
        return -1;
    }

    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        perror("ioctl");
        close(fd);
        return -1;
    }

    return fd;
}

void send_key_event(int keycode, int value) {
    struct input_event ev;
    memset(&ev, 0, sizeof(struct input_event));
    ev.type = EV_KEY;
    ev.code = keycode;
    ev.value = value;
    if (write(uinput_fd, &ev, sizeof(struct input_event)) < 0) {
        perror("write");
        return;
    }

    memset(&ev, 0, sizeof(struct input_event));
    ev.type = EV_SYN;
    ev.code = SYN_REPORT;
    ev.value = 0;
    if (write(uinput_fd, &ev, sizeof(struct input_event)) < 0) {
        perror("write");
        return;
    }

    printf("Key event: keycode=%d, value=%d\n", keycode, value); // Debug statement
}

void start_or_relaunch_game_console() {
    char command[256];

    // Kill existing instances of the game console
    system("pkill -f /home/pi/game_console/build/game_console");

    // Start the game console
    snprintf(command, sizeof(command), "/home/pi/game_console/build/game_console &");
    system(command);
}

void* button_thread(void* arg) {
    int gpio = *(int*)arg;
    int keycode = keycodes[gpio];
    int last_state = HIGH;
    int state;
    unsigned long last_debounce_time = 0;
    unsigned long debounce_delay = debounce_time; // milliseconds

    printf("Starting thread for GPIO %d, keycode %d\n", gpio, keycode); // Debug statement

    while (1) {
        state = digitalRead(gpio);
        if (state != last_state) {
            last_debounce_time = millis();
        }

        if ((millis() - last_debounce_time) > debounce_delay) {
            if (state == LOW) {
                printf("Button pressed on GPIO %d\n", gpio); // Debug statement
                send_key_event(keycode, 1);
                usleep(50000);
                send_key_event(keycode, 0);
                if (keycode == KEY_ESC) {
                    start_or_relaunch_game_console();
                }
            }
        }

        last_state = state;
        delay(5);
    }
    return NULL;
}

void setup_gpio(int gpio, int keycode) {
    pinMode(gpio, INPUT);
    pullUpDnControl(gpio, PUD_UP);
    keycodes[gpio] = keycode;

    // Pass the GPIO pin by value to the thread
    int* gpio_ptr = malloc(sizeof(int));
    *gpio_ptr = gpio;

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, button_thread, gpio_ptr);

    printf("Setup GPIO %d for keycode %d\n", gpio, keycode); // Debug statement
}

void cleanup(int signum) {
    if (uinput_fd != -1) {
        ioctl(uinput_fd, UI_DEV_DESTROY);
        close(uinput_fd);
    }
    printf("Cleaned up uinput device\n");
    exit(signum);
}

int main() {
    wiringPiSetupGpio();

    uinput_fd = setup_uinput_device();
    if (uinput_fd < 0) {
        fprintf(stderr, "Failed to setup uinput device\n");
        return -1;
    }

    setup_gpio(BUTTON_DOWN, KEY_DOWN);
    setup_gpio(BUTTON_UP, KEY_UP);
    setup_gpio(BUTTON_LEFT, KEY_LEFT);
    setup_gpio(BUTTON_RIGHT, KEY_RIGHT);
    setup_gpio(BUTTON_ENTER, KEY_ENTER);
    setup_gpio(BUTTON_BACKSPACE, KEY_BACKSPACE);
    setup_gpio(BUTTON_ESCAPE, KEY_ESC);

    signal(SIGINT, cleanup);

    while (1) {
        // Keep the main thread running
        delay(1000);
    }

    return 0;
}
