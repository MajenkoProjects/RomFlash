#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <termios.h>
#include <stdlib.h>

#define    BOTHER 0010000

struct termios savedOptions;

int openSerial(const char *_dev, uint32_t baud) {
    int _fd;
    struct termios options;
    _fd = open(_dev, O_RDWR|O_NOCTTY);

    if (!_fd) {
        return -1;
    }

    fcntl(_fd, F_SETFL, 0);
    tcgetattr(_fd, &savedOptions);
    tcgetattr(_fd, &options);
    cfmakeraw(&options);
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~CRTSCTS;
    options.c_cflag &= ~CBAUD;
    if (strcmp(_dev, "/dev/tty") == 0) {
        options.c_lflag |= ISIG;
    }
    options.c_cflag |= BOTHER;

    speed_t speed = B0;
#if defined(B4000000)
    if (baud <= 4000000) speed = B4000000;
#endif
#if defined(B3500000)
    if (baud <= 3500000) speed = B3500000;
#endif
#if defined(B3000000)
    if (baud <= 3000000) speed = B3000000;
#endif
#if defined(B2500000)
    if (baud <= 2500000) speed = B2500000;
#endif
#if defined(B2000000)
    if (baud <= 2000000) speed = B2000000;
#endif
#if defined(B1500000)
    if (baud <= 1500000) speed = B1500000;
#endif
#if defined(B1152000)
    if (baud <= 1152000) speed = B1152000;
#endif
#if defined(B1000000)
    if (baud <= 1000000) speed = B1000000;
#endif
#if defined(B921600)
    if (baud <= 921600) speed = B921600;
#endif
#if defined(B576000)
    if (baud <= 576000) speed = B576000;
#endif
#if defined(B500000)
    if (baud <= 500000) speed = B500000;
#endif
#if defined(B460800)
    if (baud <= 460800) speed = B460800;
#endif
#if defined(B230400)
    if (baud <= 230400) speed = B230400;
#endif
#if defined(B115200)
    if (baud <= 115200) speed = B115200;
#endif
#if defined(B57600)
    if (baud <= 57600) speed = B57600;
#endif
#if defined(B38400)
    if (baud <= 38400) speed = B38400;
#endif
#if defined(B19200)
    if (baud <= 19200) speed = B19200;
#endif
#if defined(B9800)
    if (baud <= 9800) speed = B9800;
#endif
#if defined(B4800)
    if (baud <= 4800) speed = B4800;
#endif
#if defined(B2400)
    if (baud <= 2400) speed = B2400;
#endif
#if defined(B1800)
    if (baud <= 1800) speed = B1800;
#endif
#if defined(B1200)
    if (baud <= 1200) speed = B1200;
#endif
#if defined(B600)
    if (baud <= 600) speed = B600;
#endif
#if defined(B300)
    if (baud <= 300) speed = B300;
#endif
#if defined(B200)
    if (baud <= 200) speed = B200;
#endif
#if defined(B150)
    if (baud <= 150) speed = B150;
#endif
#if defined(B134)
    if (baud <= 134) speed = B134;
#endif
#if defined(B110)
    if (baud <= 110) speed = B110;
#endif
#if defined(B75)
    if (baud <= 75) speed = B75;
#endif
#if defined(B50)
    if (baud <= 50) speed = B50;
#endif

    if (speed == B0) {
        printf("Invalid baud\n");
        close(_fd);
        return -1;
    }

    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);

    if (tcsetattr(_fd, TCSANOW, &options) != 0) {
        fprintf(stderr, "Can't set up serial\n");
    }

    return _fd;
}

void closeSerial(int fd) {
    tcsetattr(fd, TCSANOW, &savedOptions);
    close(fd);
}

void readSerial(int fd, char *buffer) {
    int pos = 0;
    buffer[0] = 0;
    while (1) {
        char c;
        read(fd, &c, 1);
        if (c == '\n') {
            return;
        }
        if (c >= ' ') {
            buffer[pos++] = c;
            buffer[pos] = 0;
        }
    }
}

int command(int fd, const char *str) {
    write(fd, str, strlen(str));
    write(fd, "\n", 1);
    char ret[100];
    readSerial(fd, ret);
    while (ret[0] != '$') {
        printf("%s\n", ret);
        readSerial(fd, ret);
    }
    return strtol(ret + 1, NULL, 10);
}

int countLines(const char *filename) {
    int count = 0;
    FILE *f = fopen(filename, "r");
    if (!f) return -1;
    char temp[1024];
    while (!feof(f)) {
        fgets(temp, 1024, f);
        count++;
    }
    fclose(f);
    return count;
        
}

int burn(int fd, const char *fn) {
    int lines = countLines(fn);
    if (lines < 0) {
        printf("Unable to open file\n");
        return -1;
    }

    int count = 0;
    FILE *f = fopen(fn, "r");
    char tmp[1024];
    while (!feof(f)) {
        fgets(tmp, 1024, f);
        int rv = command(fd, tmp);
        if (rv != 200) {
            return 0;
        }
        count++;
        printf("\rSending line %d of %d (%d%%)", count, lines, count * 100 / lines);
        fflush(stdout);
    }
    printf("\n");
    fclose(f);
	return 1;
}

// Usage: flash <chip> <command> [parameters] [<command> [parameters]...]


int main(int argc, char **argv) {
	if (argc < 3) {
		printf("Usage: flash <chip> <command> [parameters] [command parameters...]\n");
		return -1;
	}

    char *port ="/dev/ttyACM0";
    int fd = openSerial(port, 460800);
    if (fd == -1) {
        printf("Unable to open serial port\n");
        return -1;
    }


	char *chip = argv[1];
	
	int i = 2;

	char temp[100];

	snprintf(temp, 100, "C%s", chip);
	command(fd, temp);

	command(fd, "O0");


	while (i < argc) {

		char *cmd = argv[i++];

		if (strcmp(cmd, "power") == 0) {
			char *p = argv[i++];
			if (strcmp(p, "on") == 0) {
				int rv = command(fd, "P1");
				printf("power on: %s\n", rv == 200 ? "ok" : "fail");
			} else if (strcmp(p, "off") == 0) {
				int rv = command(fd, "P0");
				printf("power off: %s\n", rv == 200 ? "ok" : "fail");
			} else {
				printf("Usage: power <on/off>\n");
			}
		} else if (strcmp(cmd, "ident") == 0) {
			int rv = command(fd, "I");
			printf("ident: %s\n", rv == 200 ? "ok" : "fail");
		} else if (strcmp(cmd, "erase") == 0) {
			int rv = command(fd, "E");
			printf("erase: %s\n", rv == 200 ? "ok" : "fail");
		} else if (strcmp(cmd, "blankcheck") == 0) {
			int rv = command(fd, "B");
			printf("blankcheck: %s\n", rv == 200 ? "ok" : "fail");
		} else if (strcmp(cmd, "burn") == 0) {
			char *p = argv[i++];
			if (burn(fd, p)) {
				printf("burn: ok\n");
			} else {
				printf("burn: fail\n");
			}
		} else {
			printf("Unknown command: %s\n", cmd);
		}
	}

    if (fd >= 0) {
        closeSerial(fd);
    }
    return 0;
}
