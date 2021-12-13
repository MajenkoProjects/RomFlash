//       -------\/-------
//   53 o| A18      VDD |o 52
//   51 o| A16      WE# |o 50
//   49 o| A15      A17 |o 48
//   47 o| A12      A14 |o 46
//   45 o| A7       A13 |o 44
//   43 o| A6        A8 |o 42
//   41 o| A5        A9 |o 40
//   39 o| A4       A11 |o 38
//   37 o| A3       OE# |o 36
//   35 o| A2       A10 |o 34
//   33 o| A1       CE# |o 32
//   31 o| A0        D7 |o 30
//   29 o| D0        D6 |o 28
//   27 o| D1        D5 |o 26
//   25 o| D2        D4 |o 24
//   23 o| GND       D3 |o 22
//       ----------------



uint32_t extendedSegmentAddress = 0;

const uint8_t pins_a[] = {
    31, 33, 35, 37, 39, 41, 43, 45, 42, 40, 34, 38, 47, 44, 46, 49, 51, 48, 53
};

#define NUM_ADDR (sizeof(pins_a))

const uint8_t pins_d[] = {
    29, 27, 25, 22, 24, 26, 28, 30
};

const uint8_t pin_wr = 50;
const uint8_t pin_oe = 36;
const uint8_t pin_ce = 32;

#define pVCC 52
#define pGND 23

void start() {
    digitalWrite(pin_ce, LOW);
}

void end() {
    digitalWrite(pin_ce, HIGH);
}

void write(uint32_t addr, uint8_t v) {
    for (int i = 0; i < NUM_ADDR; i++) {
        digitalWrite(pins_a[i], (addr & (1UL << i) ? HIGH : LOW));
    }
    for (int i = 0; i < 8; i++) {
        pinMode(pins_d[i], OUTPUT);
        digitalWrite(pins_d[i], (v & (1UL << i) ? HIGH : LOW));
    }
    digitalWrite(pin_wr, LOW);
    asm volatile("nop");
    digitalWrite(pin_wr, HIGH);
    for (int i = 0; i < 8; i++) {
        pinMode(pins_d[i], INPUT);
    }
}

uint8_t read(uint32_t addr) {
    for (int i = 0; i < NUM_ADDR; i++) {
        digitalWrite(pins_a[i], (addr & (1UL << i) ? HIGH : LOW));
    }
    for (int i = 0; i < 8; i++) {
        pinMode(pins_d[i], INPUT);
    }
    digitalWrite(pin_oe, LOW);
    asm volatile("nop");
    uint8_t v = 0;
    for (int i = 0; i < 8; i++) {
        v |= (digitalRead(pins_d[i]) ? (1UL << i) : 0);
    }
    digitalWrite(pin_oe, HIGH);
    return v;
}

void power(bool state) {
    pinMode(pVCC, OUTPUT);
    pinMode(pGND, OUTPUT);
    if (state) { digitalWrite(pVCC, HIGH); } else { digitalWrite(pVCC, LOW); }
    digitalWrite(pGND, LOW);
    Serial.println("$200");
}

void command(uint8_t cmd) {
    write(0x5555, 0xAA);
    write(0x2AAA, 0x55);
    write(0x5555, cmd);
}

void eraseChip() {
    extendedSegmentAddress = 0;   
    start();
    command(0x80);
    command(0x10);
    uint8_t v1 = read(0);
    uint8_t v2 = read(0);
    while (v1 != v2) {
        v1 = v2;
        v2 = read(0);
    }
    end();
}

void ident() {
    start();
    command(0x90);
    uint8_t mfg = read(0x0000);
    uint8_t model = read(0x0001);
    command(0xF0);
    end();

    char tmp[10];
    sprintf(tmp, "*%02X%02X", mfg, model);
    Serial.println(tmp);
    uint16_t code = mfg << 8 | model;
    
    switch (code) {
        case 0xBFB5: Serial.println("*SST39SF010A"); break;
        case 0xBFB6: Serial.println("*SST39SF020A"); break;
        case 0xBFB7: Serial.println("*SST39SF040"); break;
        default: Serial.println("*UNKNOWN"); break;
    }

    Serial.println(F("$200"));
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

void testGlue() {
    start();
    for (uint32_t i = 0; i < 4096; i++) {
        uint8_t v = read(i);

        Serial.print(i & 0x0001 ? 'L' : 'l');
        Serial.print(i & 0x0002 ? 'U' : 'u');
        Serial.print(i & 0x0004 ? 'R' : 'W');
        Serial.print(" ");
        Serial.print((i & 0x07F8) >> 3, HEX);
        Serial.print(" ");
        Serial.print(i & 0x0800 ? 'A' : '.');

        Serial.print(": ");

        Serial.print(v, HEX);
        Serial.print(" ");

        Serial.print(v & 0x01 ? '.' : 'W');
        Serial.print(v & 0x02 ? '.' : 'R');
        Serial.print(v & 0x04 ? '.' : 'L');
        Serial.print(v & 0x08 ? '.' : 'U');
        Serial.print(v & 0x10 ? '.' : 'R');
        Serial.print(v & 0x20 ? '.' : 'W');
        Serial.print(v & 0x40 ? '.' : 'U');
        Serial.print(v & 0x80 ? '.' : 'L');
        Serial.println();
    }
    end();
}

void processHexLine(const char *data) {
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
        Serial.println(F("$408"));
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
                        Serial.print("*");
                        Serial.print(offset + (addr + i), HEX);
                        Serial.print(":");
                        Serial.print(v, HEX);
                        Serial.print("!=");
                        Serial.println(b, HEX);
                        Serial.println(F("$409"));
                        return;
                    }
                }

                Serial.println(F("$200"));
                return;
            }
            break;
        case 1: // Ignore end of file
            Serial.println(F("$200"));
            return;
            break;
        case 2: // Set extended segment address
            extendedSegmentAddress = decodeHex(data + 8, 4);
            Serial.println(F("$200"));
            return;
            break;
    }
    Serial.println(F("$404"));
}

void readRange(char *buf) {
    uint32_t astart = 0;
    uint32_t aend = 0;
    if (strchr(buf, ',') != NULL) {
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
    
    char tmp[30];
    start();

    for (uint32_t i = astart; i <= aend; i++) {
        uint8_t b = read(i);
        Serial.print(i, HEX);
        Serial.print(":");
        Serial.println(b, HEX);
    }
    Serial.println(F("$200"));
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
    if (p < 0 || p >= NUM_ADDR) {
        Serial.println(F("$407"));
        return;
    }
    testPin(pins_a[p]);
    Serial.println(F("$200"));
}

void testData(const char *pin) {
    int p = strtol(pin, NULL, 10);
    if (p < 0 || p >= 8) {
        Serial.println(F("$407"));
        return;
    }
    pinMode(pins_d[p], OUTPUT);
    testPin(pins_d[p]);
    pinMode(pins_d[p], INPUT);
    Serial.println(F("$200"));
}

void setup() {
    Serial.begin(460800);
    for (int i = 0; i < NUM_ADDR; i++) {
        pinMode(pins_a[i], OUTPUT);
    }
    for (int i = 0; i < 8; i++) {
        pinMode(pins_d[i], INPUT);
    }

    pinMode(pin_ce, OUTPUT); digitalWrite(pin_ce, HIGH);
    pinMode(pin_oe, OUTPUT); digitalWrite(pin_oe, HIGH);
    pinMode(pin_wr, OUTPUT); digitalWrite(pin_wr, HIGH);
}

void loop() {
    static char buffer[128] = { 0 };
    static int bpos = 0;
    static bool echo = false;

    if (Serial.available()) {
        char c = Serial.read();
        if (echo) Serial.write(c);
        if (c == '\r' || c == '\n') {
            if (echo) {
                if (c == '\r') Serial.write('\n');
                if (c == '\n') Serial.write('\r');
            }
            if (bpos == 0) {
                return;
            }
            if (buffer[0] == ':') {
                processHexLine(buffer + 1);
            } else if (strcmp(buffer, "!E") == 0) {
                eraseChip();
                Serial.println(F("$200"));
            } else if (strcmp(buffer, "!O") == 0) {
                echo = !echo;
                Serial.println(F("$200"));
            } else if (strcmp(buffer, "!I") == 0) {
                ident();
            } else if (strncmp(buffer, "!R", 2) == 0) {
                readRange(buffer + 2);
            } else if (strncmp(buffer, "!TA", 3) == 0) {
                testAddress(buffer + 3);
            } else if (strncmp(buffer, "!TD", 3) == 0) {
                testData(buffer + 3);
            } else if (strcmp(buffer, "!P0") == 0) {
                power(false);
            } else if (strcmp(buffer, "!P1") == 0) {
                power(true);
            } else if (strcmp(buffer, "!Z") == 0) {
                testGlue();
            } else {
                Serial.println(F("$401"));
            }

            buffer[0] = 0;
            bpos = 0;
            return;
        }

        buffer[bpos++] = c;
        buffer[bpos] = 0;
        if (bpos >= 127) {
            Serial.println(F("$402"));
            bpos = 0;
            buffer[0] = 0;
        }

    }

}
