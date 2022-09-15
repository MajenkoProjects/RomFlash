#include "pins.h"

bool debug = false;
bool powered = false;

uint32_t extendedSegmentAddress = 0;

USBHS usbDevice;
USBManager USB(usbDevice, 0xf055, 0x9837, "Majenko Technologies", "EEPROM Programmer");
CDCACM uSerial;

#define SERIAL uSerial

#define IDENT \
	if (chip == NULL) { \
		SERIAL.println(F("*Chip not identified")); \
		SERIAL.println(F(E_IDENT)); \
		return; \
	}

#define POWER \
	if (!powered) { \
		SERIAL.println(F("*Chip not powered")); \
		return; \
	}

#define NEED(X) \
	if ((chip->flags & (X)) == 0) { \
		SERIAL.println(F("*Operation not supported")); \
		SERIAL.println(F(E_UNSUPPORTED)); \
		return; \
	}

struct pinout {
	uint8_t n_address;
	uint8_t address[20];
	uint8_t data[8];
	uint8_t vcc;
	uint8_t gnd;
	uint8_t oe;
	uint8_t wr;
	uint8_t ce;
};

//       -------\/-------
//   01 o| A18      VDD |o 32
//   02 o| A16      WE# |o 31
//   03 o| A15      A17 |o 30
//   04 o| A12      A14 |o 29
//   05 o| A7       A13 |o 28
//   06 o| A6        A8 |o 27
//   07 o| A5        A9 |o 26
//   08 o| A4       A11 |o 25
//   09 o| A3       OE# |o 24
//   10 o| A2       A10 |o 23
//   11 o| A1       CE# |o 22
//   12 o| A0        D7 |o 21
//   13 o| D0        D6 |o 20
//   14 o| D1        D5 |o 19
//   15 o| D2        D4 |o 18
//   16 o| GND       D3 |o 17
//       ----------------
struct pinout Flash32x19 = {
	19, { P12, P11, P10, P09, P08, P07, P06, P05, P27, P26, P23, P25, P04, P28, P29, P03, P02, P30, P01 },
	{ P13, P14, P15, P17, P18, P19, P20, P21 },
	P32, P16, P24, P31, P22 
};
struct pinout Flash32x18 = {
	18, { P12, P11, P10, P09, P08, P07, P06, P05, P27, P26, P23, P25, P04, P28, P29, P03, P02, P30 },
	{ P13, P14, P15, P17, P18, P19, P20, P21 },
	P32, P16, P24, P31, P22
};
struct pinout Flash32x17 = {
	17, { P12, P11, P10, P09, P08, P07, P06, P05, P27, P26, P23, P25, P04, P28, P29, P03, P02 },
	{ P13, P14, P15, P17, P18, P19, P20, P21 },
	P32, P16, P24, P31, P22
};

//       -------\/-------
//   01 o| Vpp      VDD |o 32
//   02 o| A12      A14 |o 31
//   03 o| A7       A13 |o 30
//   04 o| A6        A8 |o 29
//   05 o| A5        A9 |o 28
//   06 o| A4       A11 |o 27
//   07 o| A3       OE# |o 26
//   08 o| A2       A10 |o 25
//   09 o| A1       CE# |o 24
//   10 o| A0        D7 |o 23
//   11 o| D0        D6 |o 22
//   12 o| D1        D5 |o 21
//   13 o| D2        D4 |o 20
//   14 o| GND       D3 |o 19 
//       ----------------
struct pinout Eprom28x15 = {
	15, { P10, P09, P08, P07, P06, P05, P04, P03, P29, P28, P25, P27, P02, P30, P31},
	{ P11, P12, P13, P19, P20, P21, P22, P23},
	P32, P14, P26, -1, P24
};

struct chipdef {
	uint8_t vid;
	uint8_t pid;

	const char *name;

	struct pinout *form;
	uint32_t size;
	uint32_t sectors;
	uint32_t secsize;
	uint32_t saddpin;

	uint32_t flags;
};


// Can the chip be erased in its entirety with a simple JEDEC command
#define F_CHIP_ERASE 			0x0001

// Can you erase individual sectors or blocks in the chip
#define F_SECTOR_ERASE 			0x0002

// Are the address pins multiplexed? If so the CS pin becomes the R/C pin
#define F_MUX_ADDRESS			0x0004

struct chipdef chips[] = {
	// VID   PID  Name              Pinout      	Size      Secs    Sec Size   Sector Erase Address Pin
	{ 0xBF,	0xB5, "SST39SF010A",	&Flash32x17,    0x20000,    32,		0x1000,		12,		F_CHIP_ERASE | F_SECTOR_ERASE },
	{ 0xBF,	0xB6, "SST39SF020A",	&Flash32x18,    0x40000,    64,		0x1000,		12,		F_CHIP_ERASE | F_SECTOR_ERASE },
	{ 0xBF,	0xB7, "SST39SF040", 	&Flash32x19,    0x80000,   128,		0x1000,		12,		F_CHIP_ERASE | F_SECTOR_ERASE },

	{ 0x01,	0x20, "Am29F010",   	&Flash32x17,    0x20000,	 8,		0x4000,		14,		F_CHIP_ERASE | F_SECTOR_ERASE },

	{ 0xDA, 0x46, "W29C040",		&Flash32x17,    0x20000,   512,      0x100,		 0,		F_CHIP_ERASE },


	{ 0xff, 0xff, "27C256", 		&Eprom28x15,    0x8000,      1,     0x8000,      0,       0 },

	{0, 0, 0, 0, 0, 0, 0, 0}
};


#define E_OK			"$200"

#define E_UNKNOWN		"$402"
#define E_FAIL			"$403"
#define E_DENIED 		"$404"
#define E_UNSUPPORTED	"$405"

#define E_IDENT			"$501"



struct chipdef *chip = NULL;

void start() {
	if ((chip->flags & F_MUX_ADDRESS) == 0) {
	    digitalWrite(chip->form->ce, LOW);
	}
}

void end() {
	if ((chip->flags & F_MUX_ADDRESS) == 0) {
    	digitalWrite(chip->form->ce, HIGH);
	}
}

void loadMuxedAddress(uint32_t addr) {
	uint32_t mask = (1 << chip->form->n_address) - 1;
	uint32_t addr_l = addr & mask;
	uint32_t addr_h = addr >> chip->form->n_address;

	loadPlainAddress(addr_l);
	digitalWrite(chip->form->ce, LOW);
	loadPlainAddress(addr_h);
	digitalWrite(chip->form->ce, HIGH);
}

void loadPlainAddress(uint32_t addr) {
    for (int i = 0; i < chip->form->n_address; i++) {
        digitalWrite(chip->form->address[i], (addr & (1UL << i) ? HIGH : LOW));
    }
}

void loadAddress(uint32_t addr) {
	if (chip->flags & F_MUX_ADDRESS) {
		loadMuxedAddress(addr);
	} else {
		loadPlainAddress(addr);
	}
}

void write(uint32_t addr, uint8_t v) {
	if (debug) {
		char tmp[30];
		sprintf(tmp, "*W: %08lX < %02X", addr, v);
		SERIAL.println(tmp);
	}
	loadAddress(addr);
    for (int i = 0; i < 8; i++) {
        pinMode(chip->form->data[i], OUTPUT);
        digitalWrite(chip->form->data[i], (v & (1UL << i) ? HIGH : LOW));
    }
    digitalWrite(chip->form->wr, LOW);
    asm volatile("nop");
    digitalWrite(chip->form->wr, HIGH);
    for (int i = 0; i < 8; i++) {
        pinMode(chip->form->data[i], INPUT);
    }
}

uint8_t read(uint32_t addr) {
	loadAddress(addr);
    for (int i = 0; i < 8; i++) {
        pinMode(chip->form->data[i], INPUT);
    }
    digitalWrite(chip->form->oe, LOW);
    asm volatile("nop");
    uint8_t v = 0;
    for (int i = 0; i < 8; i++) {
        v |= (digitalRead(chip->form->data[i]) ? (1UL << i) : 0);
    }
    digitalWrite(chip->form->oe, HIGH);

	if (debug) {
		char tmp[30];
		sprintf(tmp, "*R: %08lX > %02X", addr, v);
		SERIAL.println(tmp);
	}
    return v;
}

void power(bool state) { IDENT
	powered = state;
    pinMode(chip->form->vcc, OUTPUT);
    pinMode(chip->form->gnd, OUTPUT);
    if (state) { digitalWrite(chip->form->vcc, HIGH); } else { digitalWrite(chip->form->vcc, LOW); }
    digitalWrite(chip->form->gnd, LOW);
    SERIAL.println(F(E_OK));
}

void command(uint8_t cmd) {
    write(0x5555, 0xAA);
    write(0x2AAA, 0x55);
    write(0x5555, cmd);
}

void eraseSector(char *buf) { IDENT
	POWER
	NEED(F_SECTOR_ERASE);
	uint32_t sec = strtol(buf, NULL, 10);
    extendedSegmentAddress = 0;   
    start();
    command(0x80);
	write(0x5555, 0xAA);
	write(0x2AAA, 0x55);
	write(sec << chip->saddpin, 0x30);
	char tmp[30];
    uint8_t v1 = read(0);
    uint8_t v2 = read(0);
    while ((v1 & 0x40) != (v2 & 0x40)) {
        v1 = v2;
        v2 = read(0);
    }
    end();
	SERIAL.print("*Result: ");
	sprintf(tmp, "%02x:%02x", v1, v2);
	SERIAL.println(tmp);
	SERIAL.println(F(E_OK));
}

void eraseChip() { IDENT
	POWER
	NEED(F_CHIP_ERASE);
    extendedSegmentAddress = 0;   
    start();
    command(0x80);
    command(0x10);
	char tmp[30];
    uint8_t v1 = read(0);
    uint8_t v2 = read(0);
    while ((v1 & 0x40) != (v2 & 0x40)) {
        v1 = v2;
        v2 = read(0);
    }
    end();
	SERIAL.print("*Result: ");
	sprintf(tmp, "%02x:%02x", v1, v2);
	SERIAL.println(tmp);
	SERIAL.println(F(E_OK));
}

void unlock() { IDENT
	POWER
	start();
	command(0x80);
	command(0x20);
	delay(10);
	end();
    SERIAL.println(F(E_OK));
}

void lock() { IDENT
	POWER
	start();
	command(0xA0);
	delay(10);
	end();
	SERIAL.println(F(E_OK));
}
	

void ident() {
	POWER
    start();
    command(0x90);
    uint8_t mfg = read(0x0000);
    uint8_t model = read(0x0001);
    command(0xF0);
    end();

    char tmp[10];
    sprintf(tmp, "*%02X%02X", mfg, model);
    SERIAL.println(tmp);


	for (struct chipdef *c = chips; c->vid != 0; c++) {
		if (c->vid == mfg && c->pid == model) {
			if (c == chip) {
				SERIAL.println(F(E_OK));
				return;
			}
		}	
	}

    SERIAL.println(F("*Chip mismatch"));
	SERIAL.println(F(E_FAIL));

}

static inline uint8_t h2d(uint8_t hex) {
    if(hex > 0x39) hex -= 7; 
    return(hex & 0xf);
}

uint16_t decodeHex(const char *c, int len, bool bswap = false) {
    uint16_t v = 0;
    for (int i = 0; i < len; i++) {
        v <<= 4;
        v |= h2d(c[i]);
    }

    if (bswap) {
        uint16_t v1 = ((v & 0xFF) << 8) | ((v & 0xFF00) >> 8);
        return v1;
    }
    return v;       
}

void processHexLine(const char *data) { IDENT
	POWER
    uint8_t len = decodeHex(data, 2);
    uint16_t addr = decodeHex(data + 2, 4);
    uint8_t code = decodeHex(data + 6, 2);

    uint8_t cs = 0;

    cs += decodeHex(data, 2);   // len
    cs += decodeHex(data + 2, 2);   // addr 1
    cs += decodeHex(data + 4, 2);   // addr 2
    cs += decodeHex(data + 6, 2);   // code

    for (int i = 0; i < len; i++) {
        cs += decodeHex(data + 8 + (i * 2), 2);
    }

    cs = ~cs;
    cs++;

    if (cs != decodeHex(data + 8 + (len * 2), 2)) {
        SERIAL.println(F(E_FAIL));
        return;
    }
    
    switch (code) {
        case 0: { // Write data
                uint32_t offset = extendedSegmentAddress * 16;
                for (int i = 0; i < len; i++) {
                    uint8_t b = decodeHex(data + 8 + (i * 2), 2);
                    start();
                    command(0xA0);
                    write(offset + (addr + i), b);
                    delayMicroseconds(20);
                    end();

                    start();
                    uint8_t v = read(offset + (addr + i));
                    end();
                    if (v != b) {
                        SERIAL.print("*");
                        SERIAL.print(offset + (addr + i), HEX);
                        SERIAL.print(":");
                        SERIAL.print(v, HEX);
                        SERIAL.print("!=");
                        SERIAL.println(b, HEX);
                        SERIAL.println(F(E_FAIL));
                        return;
                    }
                }

                SERIAL.println(F(E_OK));
                return;
            }
            break;
        case 1: // Ignore end of file
            SERIAL.println(F(E_OK));
            return;
            break;
        case 2: // Set extended segment address
            extendedSegmentAddress = decodeHex(data + 8, 4);
            SERIAL.println(F(E_OK));
            return;
            break;
    }
    SERIAL.println(F(E_FAIL));
}

void writeBytes(char *buf) { IDENT
	char *addr = strtok(buf, ",");
	char *val = strtok(NULL, ",");
	start();
	while (val != NULL) {
		uint32_t a = strtoul(addr, NULL, 16);
		uint8_t v = strtoul(val, NULL, 16);
		write(a, v);
		addr = strtok(NULL, ",");
		val = strtok(NULL, ",");
	}
	end();
	SERIAL.println(F(E_OK));
}

void readSector(char *buf) { IDENT
	POWER
	uint32_t sec = strtoul(buf, NULL, 10);
	uint32_t astart = chip->secsize * sec;
	uint32_t aend = astart + chip->secsize - 1;
	dumpMemory(astart, aend);
	SERIAL.println(F(E_OK));
}

void readRange(char *buf) { IDENT
	POWER
    uint32_t astart = 0;
    uint32_t aend = 0;
	if (buf[0] == 0) {
		aend = chip->size - 1;
	} else if (strchr(buf, ',') != NULL) {
        char *s = strtok(buf, ",");
        char *l = strtok(NULL, ",");
        astart = strtoul(s, NULL, 16);
        aend = astart + strtoul(l, NULL, 16);
    } else if (strchr(buf, '-') != NULL) {
        char *s = strtok(buf, "-");
        char *e = strtok(NULL, "-");
        astart = strtoul(s, NULL, 16);
        aend = strtoul(e, NULL, 16);
    }

	dumpMemory(astart, aend);
    SERIAL.println(F(E_OK));
}

void dumpMemory(uint32_t astart, uint32_t aend) {
    char tmp[30];
	char abuf[30] = {0};
    start();

	uint32_t addr = astart;
	uint32_t c = 0;

	while (addr <= aend) {
		if (SERIAL.available()) {
			char ch = SERIAL.read();
			if (ch == 3) {
				SERIAL.println("^C");
				return;
			}
		}
		if (c == 0) {
			sprintf(tmp, "%08lX: ", addr);
			SERIAL.print(tmp);
			abuf[0] = 0;
		}
		uint8_t b = read(addr);
		sprintf(tmp, "%02x ", b);
		SERIAL.print(tmp);

		if (b >= ' ' && b <= 127) {
			abuf[c] = b;
			abuf[c+1] = 0;
		} else {
			abuf[c] = '.';
			abuf[c+1] = 0;
		}

		c++;
		addr++;

		if (c == 16) {
			SERIAL.print(" ");
			SERIAL.println(abuf);
			c = 0;
		}
	}

	if (c > 0) {
		while (c < 16) {
			SERIAL.print("   ");
			c++;
		}
		SERIAL.print(" ");
		SERIAL.println(abuf);
	}
			

    end();
    
}

void testPin(int p) {
    for (int i = 0; i < 10; i++) {
        digitalWrite(p, HIGH);
        delay(1);
        digitalWrite(p, LOW);
        delay(1);
    }
}

void testAddress(const char *pin) {
    int p = strtol(pin, NULL, 10);
    if (p < 0 || p >= chip->form->n_address) {
        SERIAL.println(F(E_DENIED));
        return;
    }
    testPin(chip->form->address[p]);
    SERIAL.println(F(E_OK));
}

void testData(const char *pin) {
    int p = strtol(pin, NULL, 10);
    if (p < 0 || p >= 8) {
        SERIAL.println(F(E_DENIED));
        return;
    }
    pinMode(chip->form->data[p], OUTPUT);
    testPin(chip->form->data[p]);
    pinMode(chip->form->data[p], INPUT);
    SERIAL.println(F(E_OK));
}

bool blankCheckSector(int snum) { 
	uint32_t saddr = snum * chip->secsize;
	for (uint32_t i = 0; i < chip->secsize; i++) {	
		if(read(saddr + i) != 0xFF) return false;
	}
	return true;
}

void blankCheck() { IDENT
	POWER
	bool ok = true;
	SERIAL.print("*Blank Check: ");
	start();
	for (int s = 0; s < chip->sectors; s++) {
		if (blankCheckSector(s)) {
			SERIAL.print(".");
		} else {
			SERIAL.print("N");
			ok = false;
		}
	}
	end();
	SERIAL.println();

	if (ok) {
		SERIAL.println(F(E_OK));
	} else {
		SERIAL.println(F(E_FAIL));
	}
}

void listChips() {
	for (struct chipdef *c = chips; c->vid != 0; c++) {
		SERIAL.print("*");
		SERIAL.println(c->name);
	}
}

void selectChip(char *buf) {
	if (buf[0] == 0) {
		listChips();
		SERIAL.println(F(E_OK));
		return;
	}

	for (struct chipdef *c = chips; c->vid != 0; c++) {
		if (strcasecmp(buf, c->name) == 0) {
			initChip(c);
			SERIAL.println(F(E_OK));
			return;
		}
	}
	SERIAL.println(F(E_FAIL));
}

void initChip(struct chipdef *c) {

	if (chip != NULL) {
		for (int i = 0; i < chip->form->n_address; i++) {
			pinMode(chip->form->address[i], INPUT);
		}
		for (int i = 0; i < 8; i++) {
			pinMode(chip->form->data[i], INPUT);
		}
		pinMode(chip->form->ce, INPUT);
		pinMode(chip->form->oe, INPUT);
		pinMode(chip->form->wr, INPUT);
		pinMode(chip->form->vcc, INPUT);
		pinMode(chip->form->gnd, INPUT);
	}

	chip = c;

    for (int i = 0; i < chip->form->n_address; i++) {
        pinMode(chip->form->address[i], OUTPUT);
    }
    for (int i = 0; i < 8; i++) {
        pinMode(chip->form->data[i], INPUT);
    }
    pinMode(chip->form->ce, OUTPUT); digitalWrite(chip->form->ce, HIGH);
    pinMode(chip->form->oe, OUTPUT); digitalWrite(chip->form->oe, HIGH);
    pinMode(chip->form->wr, OUTPUT); digitalWrite(chip->form->wr, HIGH);

}

void setup() {
	USB.addDevice(uSerial);
	USB.begin();
    SERIAL.begin(460800);
}

void loop() {
    static char buffer[128] = { 0 };
    static int bpos = 0;
    static bool echo = false;

    if (SERIAL.available()) {
        char c = SERIAL.read();
		c = toupper(c);
        if (c == '\r' || c == '\n') {
            if (echo) {
				SERIAL.println();
            }
            if (bpos == 0) {
                return;
            }
            if (buffer[0] == ':') {
                processHexLine(buffer + 1);
            } else if (strcmp(buffer, "E") == 0) {
                eraseChip();
			} else if (strncmp(buffer, "ES", 2) == 0) {
				eraseSector(buffer + 2);
            } else if (strcmp(buffer, "O") == 0) {
                echo = !echo;
                SERIAL.println(F(E_OK));
            } else if (strcmp(buffer, "O0") == 0) {
                echo = false;
                SERIAL.println(F(E_OK));
            } else if (strcmp(buffer, "O1") == 0) {
                echo = true;
                SERIAL.println(F(E_OK));
            } else if (strcmp(buffer, "D") == 0) {
                debug = !debug;
                SERIAL.println(F(E_OK));
            } else if (strcmp(buffer, "I") == 0) {
                ident();
			} else if (strncmp(buffer, "C", 1) == 0) {
				selectChip(buffer + 1);
			} else if (strncmp(buffer, "RS", 2) == 0) {
				readSector(buffer + 2);
            } else if (strncmp(buffer, "R", 1) == 0) {
                readRange(buffer + 1);
			} else if (strncmp(buffer, "W", 1) == 0) {
				writeBytes(buffer + 1);
            } else if (strncmp(buffer, "TA", 2) == 0) {
                testAddress(buffer + 2);
            } else if (strncmp(buffer, "TD", 2) == 0) {
                testData(buffer + 2);
            } else if (strcmp(buffer, "P0") == 0) {
                power(false);
            } else if (strcmp(buffer, "P1") == 0) {
                power(true);
			} else if (strcmp(buffer, "L") == 0) {
				lock();
			} else if (strcmp(buffer, "U") == 0) {
				unlock();
			} else if (strcmp(buffer, "B") == 0) {
				blankCheck();
            } else {
                SERIAL.println(F(E_UNKNOWN));
            }

            buffer[0] = 0;
            bpos = 0;
            return;
        }

		if (c == 8) {
			if (bpos > 0) {
				bpos--;
				buffer[bpos] = 0;
				if (echo) SERIAL.print(" "); 
			}
			return;
		}

        if (echo) SERIAL.write(c);
        buffer[bpos++] = c;
        buffer[bpos] = 0;
        if (bpos >= 127) {
            SERIAL.println(F(E_DENIED));
            bpos = 0;
            buffer[0] = 0;
        }

    }

}
