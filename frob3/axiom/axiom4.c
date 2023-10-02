////////////////////////////////////////
//   PART--BEGIN-HEADER--
////////////////////////////////////////

// Conventional types and constants for Frobio

#define true (bool)1
#define false (bool)0
#define OKAY (errnum)0
#define NOTYET (errnum)1

typedef unsigned char bool;
typedef unsigned char byte;
typedef unsigned char errnum;
typedef unsigned int word;
typedef void (*func_t)();

#define TCP_CHUNK_SIZE 1024  // Chunk to send or recv in TCP.
#define WIZ_PORT  0xFF68   // Hardware port.
#define WAITER_TCP_PORT  2319   // w# a# i#
#define CASBUF 0x01DA // Rock the CASBUF, rock the CASBUF!
#define VDG_RAM  0x0400  // default 32x16 64-char screen
#define VDG_END  0x0600

#include "frob3/wiz/w5100s_defs.h"

#ifdef __GNUC__

#include <stdarg.h>
typedef unsigned int size_t;
#define restrict
void abort(void);
void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memset(void *s, int c, size_t n);
char *strcpy(char *restrict dest, const char *restrict src);
char *strncpy(char *restrict dest, const char *restrict src, size_t n);
size_t strlen(const char *s);
#undef restrict

#elif defined(__CMOC__)

#include <cmoc.h>
#include <stdarg.h>

#define volatile /* not supported by cmoc */

#else
---- which compiler are you using? ----
#endif

// How to talk to the four hardware ports.
struct wiz_port {
  volatile byte command;
  volatile word addr;
  volatile byte data;
};

// Global state for axiom4.
struct axiom4_vars {
    // THE FIELD orig_s_reg MUST BE FIRST (due to how we zero the vars).
    word orig_s_reg;  // Remember original stack pointer.
    word rom_sum_0;
    word rom_sum_1;
    word rom_sum_2;
    word rom_sum_3;

    volatile struct wiz_port* wiz_port;  // which hardware port?
    word transaction_id;
    bool got_dhcp;
    bool got_lan;
    bool need_dhcp;

    byte hostname[8];
    byte ip_addr[4];      // dhcp fills
    byte ip_mask[4];      // dhcp fills
    byte ip_gateway[4];   // dhcp fills
    byte ip_resolver[4];  // dhcp fills
    byte ip_waiter[4];
    byte mask_num;
    word waiter_port;
    byte xid[4];    // Temporary packet id in DHCP

    word vdg_addr;   // mapped via SAM
    word vdg_begin;  // begin useable portion
    word vdg_end;    // end usable portion
    word vdg_ptr;    // inside usable portion

    char* line_ptr;

    char line_buf[64];
};
#define Vars ((struct axiom4_vars*)CASBUF)
#define WIZ  (Vars->wiz_port)

struct axiom4_rom_tail { // $DFE4..$DFFF
  byte rom_reserved[3];
  byte rom_wiz_hw_port;  // $68 for $ff68
  byte rom_hostname[8];
  byte rom_mac_tail[4];  // After two $02 bytes.
  byte rom_secrets[16];  // For Challenge/Response Authentication Protocols.
};
#define Rom ((struct axiom4_rom_tail*)0xDFEA)

// Constants for the (four) Wiznet sockets' registers.
struct sock {
    word base;
    word tx_ring;
    word rx_ring;
};
extern const struct sock WizSocketFacts[4];

#define SOCK0_AND (Socks+0),
#define JUST_SOCK0 (Socks+0)

#define SOCK1_AND (Socks+1),
#define JUST_SOCK1 (Socks+1)

#define PARAM_JUST_SOCK   const struct sock* sockp
#define PARAM_SOCK_AND    const struct sock* sockp,
#define JUST_SOCK         sockp
#define SOCK_AND          sockp,

#define B    (sockp->base)
#define T    (sockp->tx_ring)
#define R    (sockp->rx_ring)
#define N    (sockp->nth)

#define RING_SIZE 2048
#define RING_MASK (RING_SIZE - 1)

enum Commands {
  CMD_POKE = 0,
  CMD_LOG = 200,
  CMD_INKEY = 201,
  CMD_PUTCHAR = 202,
  CMD_PEEK = 203,
  CMD_DATA = 204,
  CMD_SP_PC = 205, // deprecate
  CMD_REV = 206, // deprecate
  CMD_RTI = 214,  // experimental
  CMD_JSR = 255,
};

struct proto {
  byte open_mode;
  byte open_status;
  bool is_broadcast;
  byte send_command;
};
extern const struct proto TcpProto;
extern const struct proto UdpProto;
extern const struct proto BroadcastUdpProto;
#if 0
extern const char ClassAMask[4];
extern const char ClassBMask[4];
extern const char ClassCMask[4];
#endif

struct wiz_udp_recv_header {
    byte ip_addr[4];
    word udp_port;
    word udp_payload_len;
};

extern const byte HexAlphabet[];

//// Function Declarations.

byte IsThisGomar();
byte WizGet1(word reg);
word WizGet2(word reg);
void WizGetN(word reg, void* buffer, word size);
void WizPut1(word reg, byte value);
void WizPut2(word reg, word value);
void WizPutN(word reg, const void* data, word size);
word WizTicks();
byte WizTocks();
void WizReset();
void WizConfigure();
void WizIssueCommand(PARAM_SOCK_AND byte cmd);
void WizWaitStatus(PARAM_SOCK_AND byte want);

typedef word tx_ptr_t;

void WizOpen(PARAM_SOCK_AND const struct proto* proto, word local_port );
void TcpDial(PARAM_SOCK_AND const byte* host, word port);
void TcpEstablish(PARAM_JUST_SOCK);
errnum WizCheck(PARAM_JUST_SOCK);
errnum WizRecvGetBytesWaiting(PARAM_SOCK_AND word* bytes_waiting_out);
tx_ptr_t WizReserveToSend(PARAM_SOCK_AND  size_t n);
tx_ptr_t WizDataToSend(PARAM_SOCK_AND tx_ptr_t tx_ptr, const char* data, size_t n);
tx_ptr_t WizBytesToSend(PARAM_SOCK_AND tx_ptr_t tx_ptr, const byte* data, size_t n);
void WizFinalizeSend(PARAM_SOCK_AND const struct proto *proto, size_t n);
errnum WizSendChunk(PARAM_SOCK_AND  const struct proto* proto, char* data, size_t n);
errnum WizRecvChunkTry(PARAM_SOCK_AND char* buf, size_t n);
errnum WizRecvChunk(PARAM_SOCK_AND char* buf, size_t n);
errnum WizRecvChunkBytes(PARAM_SOCK_AND byte* buf, size_t n);
errnum TcpRecv(PARAM_SOCK_AND char* p, size_t n);
errnum TcpSend(PARAM_SOCK_AND  char* p, size_t n);
void UdpDial(PARAM_SOCK_AND  const struct proto *proto,
             const byte* dest_ip, word dest_port);
void WizClose(PARAM_JUST_SOCK);

void ConfigureTextScreen(word addr, bool orange);
word StackPointer();
char PolCat(); // Return one INKEY char, or 0, with BASIC `POLCAT` subroutine.
void Delay(word n);

void PutChar(char ch);
void PutStr(const char* s);
void PutHex(word x);
void PutDec(word x);
void Fatal(const char* wut, word arg);
void ShowLine(word line);
void PrintF(const char* format, ...);
void AssertEQ(word a, word b);
void AssertLE(word a, word b);

////////////////////////////////////////
//   PART--END-HEADER--
////////////////////////////////////////

#define LAN_REQUEST_UDP_PORT 12114 // L=12 A=1 N=14
#define LAN_REPLY_UDP_PORT   12118 // ........ R=18

struct lan_discovery_request {
  byte lan_opcode[4];  // "LAN\0"
  byte lan_xid[4];
  byte lan_reserved[8];  // TODO: 6309 bit.  Gomar bit.

  word orig_s_reg;     // how big is memory?
  word main;           // where is axiom, rom or ram?
  word rom_sum_0;      // what roms are available?
  word rom_sum_1;
  word rom_sum_2;
  word rom_sum_3;
  byte mac_tail[4];    // 4 bytes from $DFEC:$DFF0
};

struct lan_discovery_reply {
  byte lan_opcode[4];  // "LAR\0"
  byte lan_xid[4];
  byte lan_reserved[8];

  byte axiom_commands[64];  // ASCII commands to execute.
};

////////////////////////////////////////
//   PART--END-HEADER--
////////////////////////////////////////

////////////////////////////////////////
//   PART-NETLIB3
////////////////////////////////////////

////////////////////////////////////////////////////////
///
///  GCC Standard Library Routines.

void* memcpy(void* dest, const void* src, size_t n) {
  char* d = (char*)dest;
  char* s = (char*)src;
  for (size_t i = 0; i < n; i++) *d++ = *s++;
  return dest;
}

void *memset(void *s, int c, size_t n) {
  char* dest = (char*)s;
  for (size_t i = 0; i < n; i++) *dest++ = c;
  return s;
}

char *strcpy(char *restrict dest, const char *restrict src) {
  void* z = dest;
  while (*src) *dest++ = *src++;
  return z;
}

char *strncpy(char *restrict dest, const char *restrict src, size_t n) {
  void* z = dest;
  int i = 0;
  while (*src) {
    *dest++ = *src++;
    i++;
    if (i>=n) break;
  }
  return z;
}

size_t strlen(const char *s) {
  const char* p = s;
  while (*p) p++;
  return p-s;
}

size_t strnlen(const char *s, size_t max) {
  const char* p = s;
  while (*p && (p-s < max)) p++;
  return p-s;
}

////////////////////////////////////////////////////////
const byte HexAlphabet[] = "0123456789abcdef";

const struct sock Socks[2] = {
    { 0x400, 0x4000, 0x6000 },
    { 0x500, 0x4800, 0x6800 },
    // not needed // { 0x600, 0x5000, 0x7000 },
    // not needed // { 0x700, 0x5800, 0x7800 },
};

const struct proto TcpProto = {
  SK_MR_TCP+SK_MR_ND, // TCP Protocol mode with No Delayed Ack.
  SK_SR_INIT,
  false,
  SK_CR_SEND,
};
const struct proto UdpProto = {
  SK_MR_UDP, // UDP Protocol mode.
  SK_SR_UDP,
  false,
  SK_CR_SEND,
};
const struct proto BroadcastUdpProto = {
  SK_MR_UDP, // UDP Protocol mode.
  SK_SR_UDP,
  true,
  SK_CR_SEND+1,
};
const char SixFFs[6] = {(char)0xFF, (char)0xFF, (char)0xFF, (char)0xFF, (char)0xFF, (char)0xFF};
const char Eight00s[8] = {0, 0, 0, 0, 0, 0, 0, 0};

////////////////////////////////////////////////////////

void PrintH(const char* format, ...) {
  const char ** fmt_ptr = &format;
#ifdef __GNUC__
  asm volatile (
      "ldx %0\n  nop\n  fcb 33\n  fcb 111"
      :                // No output
      : "m" (fmt_ptr)  // Input pointer to stack
      : "x"            // clobbers X
  );
#else
  asm {
      ldx fmt_ptr
      nop      // Step 1
      fcb 33   // Step 2: BRN...
      fcb 111  // PrintH2 in emu/hyper.go
  }
#endif
}


////////////////////////////////////////////////////////

word StackPointer() {
  word result;
#ifdef __GNUC__
    asm ("tfr s,%0" : "=r" (result));
#else
    asm {
      tfr s,x
      stx result
    }
#endif
    return result;
}

char PolCat() {
  char inkey = 0;
  // Gomar does not emulate keypresses for PolCat (yet).
  if (!IsThisGomar()) {

#ifdef __GNUC__
    asm volatile (R"(
      jsr [$A000]
      sta %0
    )" : "=m" (inkey) );
#else
    asm {
      jsr [$A000]
      sta :inkey
    }
#endif

  }
  return inkey;
}

void Delay(word n) {
#if !__GOMAR__
  while (n--) {
#ifdef __GNUC__
    asm volatile ("mul" : : : "d", "b", "a");
    asm volatile ("mul" : : : "d", "b", "a");
    asm volatile ("mul" : : : "d", "b", "a");
    asm volatile ("mul" : : : "d", "b", "a");
    asm volatile ("mul" : : : "d", "b", "a");
#else
    asm {
      mul
      mul
      mul
      mul
      mul
    }
#endif
  }
#endif
}

void PutChar(char ch) {
#if __GOMAR__
  PrintH("CH: %x %c\n", ch, (' ' <= ch && ch <= '~') ? ch : '?');
  return;
#endif
    if (ch == 13 || ch == 10) { // Carriage Return
      do {
        PutChar(' ');
      } while ((Vars->vdg_ptr & 31));
      return;
    }

    word p = Vars->vdg_ptr;
    if (ch == 8) { // Backspace
      if (p > Vars->vdg_begin) {
        *(byte*)p = 32;
        --p;
      }
      goto END;
    }

    if (ch == 1) { // Hilite previous char.
      if (p > Vars->vdg_begin) {
        *(byte*)(p-1) ^= 0x40;  // toggle the inverse bit.
      }
      goto END;
    }

    if (ch < 32) return;  // Ignore other control chars.

    // Only use 64-char ASCII.
    if (96 <= ch && ch <= 126) ch -= 32;
    byte codepoint = (byte)ch;

    *(byte*)p = (0x3f & codepoint);
    p++;

    if (p>=Vars->vdg_end) {
        for (word i = Vars->vdg_begin; i< Vars->vdg_end; i++) {
            if (i < Vars->vdg_end-32) {
                // Copy the line below.
                *(volatile byte*)i = *(volatile byte*)(i+32);
            } else {
                // Clear the last line.
                *(volatile byte*)i = 32;
            }
        }
        p = Vars->vdg_end-32;
    }
END:
    *(byte*)p = 0xEF;  // display Blue Box cursor.
    Vars->vdg_ptr = p;
}

void PutStr(const char* s) {
    while (*s) PutChar(*s++);
}

void PutHex(word x) {
  if (x > 15u) {
    PutHex(x >> 4u);
  }
  PutChar( HexAlphabet[ 15u & x ] );
}
void PutDec(word x) {
  if (x > 9u) {
    PutDec(x / 10u);
  }
  PutChar('0' + (byte)(x % 10u));
}

void Fatal(const char* wut, word arg) {
    PutStr(" *FATAL* <");
    PutStr(wut);
    PutChar('$');
    PutHex(arg);
    PutStr("> ");
    while (1) continue;
}

void AssertEQ(word a, word b) {
  if (a != b) {
    PutHex(a);
    Fatal(":EQ", b);
  }
}

void AssertLE(word a, word b) {
  if (a > b) {
    PutHex(a);
    Fatal(":LE", b);
  }
}

void PrintF(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    const char* s = format;
    while (*s) {
        if (*s == '%') {
            ++s;
            switch (*s) {
                case 'a': {  // "%a" -> IPv4 address as Dotted Quad.
                    byte* x;
                    x = va_arg(ap, byte*);
                    PutDec(x[0]);
                    PutChar('.');
                    PutDec(x[1]);
                    PutChar('.');
                    PutDec(x[2]);
                    PutChar('.');
                    PutDec(x[3]);
                }
                break;
                case 'x': {
                    word x;
                    x = va_arg(ap, word);
                    PutHex(x);
                }
                break;
                case 'u': {
                    word x = va_arg(ap, word);
                    PutDec(x);
                }
                break;
                case 'd': {
                    int x = va_arg(ap, int);
                    if (x<0) {
                      PutChar('-');
                      x = -x;
                    }
                    PutDec((word)x);
                }
                break;
                case 's': {
                    const char* x = va_arg(ap, const char*);
                    PutStr(x);
                }
                break;
                default:
                    PutChar(*s);
            }
        } else {
            PutChar(*s);
        }
        s++;
    }
    va_end(ap);
}

///////////////////////////////////////////////////////////

byte WizGet1(word reg) {
  WIZ->addr = reg;
  return WIZ->data;
}
word WizGet2(word reg) {
  WIZ->addr = reg;
  byte z_hi = WIZ->data;
  byte z_lo = WIZ->data;
  return ((word)(z_hi) << 8) + (word)z_lo;
}
void WizGetN(word reg, void* buffer, word size) {
  volatile struct wiz_port* wiz = WIZ;
  byte* to = (byte*) buffer;
  wiz->addr = reg;
   for (word i=size; i; i--) {
    *to++ = wiz->data;
  }
}
void WizPut1(word reg, byte value) {
  WIZ->addr = reg;
  WIZ->data = value;
}
void WizPut2(word reg, word value) {
  WIZ->addr = reg;
  WIZ->data = (byte)(value >> 8);
  WIZ->data = (byte)(value);
}
void WizPutN(word reg, const void* data, word size) {
  volatile struct wiz_port* wiz = WIZ;
  const byte* from = (const byte*) data;
  wiz->addr = reg;
  for (word i=size; i; i--) {
    wiz->data = *from++;
  }
}

// WizTicks: 0.1ms but may have readbyte,readbyte error?
word WizTicks() {
    return WizGet2(0x0082/*TCNTR Tick Counter*/);
}
byte WizTocks() {
    return WizGet1(0x0082/*TCNTR Tick Counter*/);
}

void WizReset() {
  WIZ->command = 128; // Reset
  Delay(5000);
  WIZ->command = 3;   // IND=1 AutoIncr=1 BlockPingResponse=0 PPPoE=0
  Delay(1000);

  // GLOBAL OPTIONS FOR SOCKETLESS AND ALL SOCKETS:

  // Interval until retry: 1 second.
  WizPut2(RTR0, 10000 /* Tenths of milliseconds. */ );
  // Number of retries: 10 x 1sec = 10sec.
  // Sometimes reset-to-carrier takes over 5 seconds.
  WizPut1(RCR, 10);
}

void WizConfigure() {
  WizPutN(0x0001/*gateway*/, Vars->ip_gateway, 4);
  WizPutN(0x0005/*mask*/, Vars->ip_mask, 4);
  WizPutN(0x000f/*ip_addr*/, Vars->ip_addr, 4);
  WizPutN(0x0009/*ether_mac*/, Rom->rom_mac_tail, 4);
  WizPut2(0x0009/*ether_mac+0*/, 0x0202);
  WizPutN(0x000B/*ether_mac+2*/, Rom->rom_mac_tail, 4);
}

/////////////////////////////////////////////////////////////////////////////////////

void WizIssueCommand(const struct sock* sockp, byte cmd) {
  WizPut1(B+SK_CR, cmd);
  while (WizGet1(B+SK_CR)) {
    // ++ *(char*)0x401;
  }
  // if (cmd == 0x40) PutChar('<');
  // else if (cmd == 0x20) PutChar('>');
  // else PutChar('!');
}

void WizWaitStatus(const struct sock* sockp, byte want) {
  byte status;
  byte stuck = 200;
  do {
    // ++ *(char*)0x400;
    status = WizGet1(B+SK_SR);
    if (!--stuck) Fatal("W", status);
  } while (status != want);
}

void WizOpen(const struct sock* sockp, const struct proto* proto, word local_port ) {
  WizPut1(B+SK_MR, proto->open_mode);
  WizPut2(B+SK_PORTR0, local_port); // Set local port.
  WizPut1(B+SK_IR, 0xFF); // Clear all interrupts.
  WizIssueCommand(SOCK_AND SK_CR_OPEN);

  WizWaitStatus(SOCK_AND proto->open_status);
}

void TcpDial(const struct sock* sockp, const byte* host, word port) {
  WizPut2(B+SK_TX_WR0, T); // does this help

  WizPutN(B+SK_DIPR0, host, 4);
  WizPut2(B+SK_DPORTR0, port);
  WizPut1(B+SK_IR, 0xFF); // Clear Interrupt bits.
  WizIssueCommand(SOCK_AND SK_CR_CONN);
}

// For Server or Client to accept/establish connection.
void TcpEstablish(PARAM_JUST_SOCK) {
  byte stuck = 250;
  while(1) {
    Delay(2000);
    PutChar('+');
    // Or we could wait for the connected interrupt bit,
    // and not the disconnected nor the timeout bit.
    byte status = WizGet1(B+SK_SR);
    if (!--stuck) Fatal("TEZ", status);

    if (status == SK_SR_ESTB) break;
    if (status == SK_SR_INIT) continue;
    if (status == SK_SR_SYNS) continue;

    Fatal("TE", status);

  };

  WizPut1(B+SK_IR, SK_IR_CON); // Clear the Connection bit.
}

////////////////////////////////////////
//   PART-COMMANDS
////////////////////////////////////////

#define BUF (Vars->line_buf)
#define PTR (Vars->line_ptr)


void NativeMode() {
  asm volatile("ldmd #1");
}

void GetUpperCaseLine(char initialChar) {
  memset(BUF, 0, sizeof BUF);
  char* p = BUF;  // Rewind.

  if (33 <= initialChar && initialChar <= 126) {
    *p++ = initialChar;
     PutChar(initialChar);
  }

  while (true) {
    char ch = PolCat();
    if (!ch) continue;
    if (ch==13) break; // CR

    if (ch == 8) { // backspace
      if (p > BUF) {
        --p;
        *p = '\0';
        PutChar(8);
      }
      continue;
    }

    if (ch < 32) continue;  // no control
    if (ch > 126) continue; // only printable ASCII

    if ('a' <= ch && ch <= 'z') ch -= 32; // to UPPER case

    if (p < BUF + sizeof BUF - 2) {
      *p = ch;
      p++;
      PutChar(ch);
    }
  }
  PutChar(13);

  PTR = BUF;  // Rewind.
}

void SkipWhite() {
  while (*PTR == ' ' || *PTR == '.' || *PTR == '/' || *PTR == ':') ++PTR;
}

bool GetNum2Bytes(word* num_out) {
  SkipWhite();
  bool gotnum = false;
  word x = 0;
  if (*PTR == '$') {
    ++PTR;
    while (('0' <= *PTR && *PTR <= '9') ||
           ('A' <= *PTR && *PTR <= 'F')) {
      if (*PTR <= '9') {
        x = x * 16 + (*PTR - '0');
      } else {
        x = x * 16 + (*PTR - 'A' + 10);
      }
      gotnum = true;
      ++PTR;
    }
  } else {
    while ('0' <= *PTR && *PTR <= '9') {
      x = x * 10 + (*PTR - '0');
      gotnum = true;
      ++PTR;
    }
  }
  *num_out = x;
  return gotnum;
}

bool GetNum1Byte(byte* num_out) {
  word x;
  bool b = GetNum2Bytes(&x);
  *num_out = (byte)x;
  return b;
}

bool GetAddyInXid() {
  memset(Vars->xid, 0, 4);
  if (GetNum1Byte(Vars->xid+0) && 
      GetNum1Byte(Vars->xid+1) && 
      GetNum1Byte(Vars->xid+2) && 
      GetNum1Byte(Vars->xid+3) ) {
    return true;
  }
  return false;
}

byte MaskBits(int n) {
  byte z = 0;
  while (n>0) {
    z = (z >> 1) | 0x80;
    n--;
  }
  return z;
}

void SetMask(byte width) {
      if (width == 0) width = 24;
      Vars->mask_num = width;
      Vars->ip_mask[0] = MaskBits((int)width);
      Vars->ip_mask[1] = MaskBits((int)width-8);
      Vars->ip_mask[2] = MaskBits((int)width-16);
      Vars->ip_mask[3] = MaskBits((int)width-24);
}

errnum DoNetwork(byte a, byte b) {
  byte tail;
  if (!GetNum1Byte(&tail)) {
    tail = 30 + (byte)(WizTicks() % 200u);  // random 30..229
  }
  Vars->ip_addr[0] = a;
  Vars->ip_addr[1] = b;
  Vars->ip_addr[2] = 23;
  Vars->ip_addr[3] = tail;

  Vars->ip_gateway[0] = a;
  Vars->ip_gateway[1] = b;
  Vars->ip_gateway[2] = 23;
  Vars->ip_gateway[3] = 1;

  Vars->ip_waiter[0] = a;
  Vars->ip_waiter[1] = b;
  Vars->ip_waiter[2] = 23;
  Vars->ip_waiter[3] = 23;

  SetMask(24);
  return OKAY;
}

void GetHostname() {
  byte* h = Vars->hostname;
  byte n = sizeof(Vars->hostname);

  memset(h, '_', n);
  SkipWhite();
  while ('!' <= *PTR && *PTR <= '~') {
    for (byte i = 0; i < n-1; i++) {
      h[i] = h[i+1];  // Shift name to the left.
    }
    h[n-1] = *PTR;
    ++PTR;
  }
}

void ShowNetwork() {
  if (Vars->need_dhcp) {
    PrintF("d\1\n");
  } else {
    PrintF("i\1 %a/%d %a\n", Vars->ip_addr, Vars->mask_num, Vars->ip_gateway);
  }
  PrintF("w\1 %a:%d\n", Vars->ip_waiter, Vars->waiter_port);
}

// returns true when commands are done.
bool DoOneCommand(char initialChar) {
  bool done = false;
  errnum e = OKAY;
  word peek_addr = 0;

  GetUpperCaseLine(initialChar);

  SkipWhite();
  char cmd = *PTR;
  PTR++;

  if (cmd == 'U') {
      Vars->wiz_port = (struct wiz_port*)0xFF78;
  } else if (cmd == 'D') {
      GetHostname();
      Vars->need_dhcp = true;
  } else if (cmd == 'I') {
      if (!GetAddyInXid()) { e = 11; goto END; }
      memcpy(Vars->ip_addr, Vars->xid, 4);
      memset(Vars->ip_gateway, 0, 4);
      byte width;
      if (GetNum1Byte(&width)) {
        if (GetAddyInXid()) {
          memcpy(Vars->ip_gateway, Vars->xid, 4);
        }
      }
      SetMask(width);
      
  } else if (cmd == 'W') {
    if (!GetAddyInXid()) { e = 12; goto END; }
    memcpy(Vars->ip_waiter, Vars->xid, 4);
    word port;
    if (GetNum2Bytes(&port)) {
        Vars->waiter_port = port;
    }

  } else if (cmd == 'S') {
    ShowNetwork();

  } else if (cmd == 'A') {
    e = DoNetwork(10, 23);

  } else if (cmd == 'B') {
    e = DoNetwork(176, 23);

  } else if (cmd == 'C') {
    e = DoNetwork(192, 168);

  } else if (cmd == 'X') {
    e = DoNetwork(127, 23);

  } else if (cmd == 'Y') {
    e = DoNetwork(44, 23);

  } else if (cmd == 'Z') {
    e = DoNetwork(0, 23);

  } else if (cmd == '@') {
    done = true;
    goto END;

  } else if (cmd == 'Q') {
    *(byte*)0xFFD9 = 1;

  } else if (cmd == 'N') {
    NativeMode();

  } else if (cmd == '<') {
    word tmp = peek_addr;
    if (!GetNum2Bytes(&peek_addr)) peek_addr = tmp;

    PrintF("%x: ", peek_addr);
    for (byte i = 0; i < 8; i++) {
      PrintF(" %x", *(byte*)peek_addr);
      ++peek_addr;
    }
    PutChar(13);

  } else if (cmd == '>') {
      byte b;
    if (GetNum2Bytes(&peek_addr) && GetNum1Byte(&b)) {
      *(byte*)peek_addr = b;
    } else {
      PrintF("?\n");
    }

  } else if (cmd == 0) {
    // end of line
  } else {
    PrintF("U\1 :upper wiznet port $FF78\n");
    PrintF("Q\1 :quick: poke 1 to $FFD9\n");
    PrintF("N\1 :native mode for H6309\n");
    PrintF("D\1 :use DHCP\n");
    PrintF("I\1 1.2.3.4/24 5.6.7.8\n");
    PrintF("  :Set IP addr, mask, gateway\n");
    PrintF("W\1 3.4.5.6:%d :set waiter\n", WAITER_TCP_PORT);
    PrintF("A\1 :preset 10.23.23.*\n");
    PrintF("B\1 :preset 176.23.23.*\n");
    PrintF("C\1 :preset 192.168.23.*\n");
    PrintF("X\1 or Y\1 or Z\1 :goofy presets\n");
    PrintF("S\1 :show settings\n");
    PrintF("Finally:  @\1 : launch!\n");
  }

END:
  if (e) {
    PrintF("*** error %d\n", e);
  }
  return done;
}

////////////////////////////////////////
//   PART-MAIN
////////////////////////////////////////

void CallWithNewStack(word new_stack, func_t fn) {
  asm volatile("ldx %0\n  ldy %1\n  tfr x,s\n  jsr ,y"
      : // no outputs
      : "m" (new_stack), "m" (fn) // two inputs
  );
}

// Returns 0 if not Gomar, 'G' if is Gomar.
byte IsThisGomar() {
#ifdef __CMOC__
  byte result;
  asm {
     CLRA    ; optional, for 16-bit return value in D.
     CLRB
     NOP     ; begin hyper sequence...
     FCB $21 ; brn...
     FCB $FF ;    results in "LDB #$47" if on Gomar.
     STB :result
  }
  return result;
#else  // for GCC
  word result;
  asm volatile("CLRA\n  CLRB\n  NOP\n  FCB $21\n  FCB $ff\n std %0"
      : "=m" (result) // the output
      : // no inputs
      : "d" // Clobbers D register.
  );
  return (byte)result;
#endif
}

////////////////////////////////////////////////

// Only called for TCP Client.
// Can probably be skipped.
errnum WizCheck(PARAM_JUST_SOCK) {
      byte ir = WizGet1(B+SK_IR); // Socket Interrupt Register.
      if (ir & SK_IR_TOUT) { // Timeout?
        return SK_IR_TOUT;
      }
      if (ir & SK_IR_DISC) { // Disconnect?
        return SK_IR_DISC;
      }
      return OKAY;
}

errnum WizRecvGetBytesWaiting(const struct sock* sockp, word* bytes_waiting_out) {
  errnum e = WizCheck(JUST_SOCK);
  if (e) return e;

  *bytes_waiting_out = WizGet2(B+SK_RX_RSR0);  // Unread Received Size.
  return OKAY;
}

errnum WizRecvChunkTry(const struct sock* sockp, char* buf, size_t n) {
  word bytes_waiting = 0;
  errnum e = WizRecvGetBytesWaiting(SOCK_AND &bytes_waiting);
  if (e) return e;
  if( bytes_waiting < n) return NOTYET;

  word rd = WizGet2(B+SK_RX_RD0);
  word begin = rd & RING_MASK; // begin: Beneath RING_SIZE.
  word end = begin + n;    // end: Sum may not be beneath RING_SIZE.

  if (end >= RING_SIZE) {
    word first_n = RING_SIZE - begin;
    word second_n = n - first_n;
    WizGetN(R+begin, buf, first_n);
    WizGetN(R, buf+first_n, second_n);
  } else {
    WizGetN(R+begin, buf, n);
  }

  WizPut2(B+SK_RX_RD0, rd + n);
  WizIssueCommand(SOCK_AND  SK_CR_RECV); // Inform socket of changed SK_RX_RD.
  return OKAY;
}

errnum WizRecvChunk(const struct sock* sockp, char* buf, size_t n) {
  PrintH("WizRecvChunk %x...", n);
  errnum e;
  do {
    e = WizRecvChunkTry(SOCK_AND buf, n);
  } while (e == NOTYET);
  PrintH("WRC %x: %x %x %x %x %x.", n,
      buf[0], buf[1], buf[2], buf[3], buf[4]);
  return e;
}
errnum WizRecvChunkBytes(const struct sock* sockp, byte* buf, size_t n) {
  return WizRecvChunk(sockp, (char*)buf, n);
}
errnum TcpRecv(const struct sock* sockp, char* p, size_t n) {
  while (n) {
    word chunk = (n < TCP_CHUNK_SIZE) ? n : TCP_CHUNK_SIZE;
    errnum e = WizRecvChunk(SOCK_AND  p, chunk);
    if (e) return e;
    n -= chunk;
    p += chunk;
  }
  return OKAY;
}

void UdpDial(const struct sock* sockp,  const struct proto *proto,
             const byte* dest_ip, word dest_port) {
  if (proto->is_broadcast) {
    // Broadcast to 255.255.255.255 to FF:FF:FF:FF:FF:FF.
    WizPutN(B+6/*Sn_DHAR*/, SixFFs, 6);
    WizPutN(B+SK_DIPR0, SixFFs, 4);
  } else {
    WizPutN(B+SK_DIPR0, dest_ip, 4);
  }
  WizPut2(B+SK_DPORTR0, dest_port);
}

// returns tx_ptr
tx_ptr_t WizReserveToSend(const struct sock* sockp,  size_t n) {
  PrintH("ResTS %x;", n);
  // Wait until free space is available.
  word free_size;
  do {
    free_size = WizGet2(B+SK_TX_FSR0);
    PrintH("Res free %x;", free_size);
  } while (free_size < n);

  return WizGet2(B+SK_TX_WR0) & RING_MASK;
}

tx_ptr_t WizBytesToSend(const struct sock* sockp, tx_ptr_t tx_ptr, const byte* data, size_t n) {
  return WizDataToSend(SOCK_AND tx_ptr, (char*)data, n);
}
tx_ptr_t WizDataToSend(const struct sock* sockp, tx_ptr_t tx_ptr, const char* data, size_t n) {

  word begin = tx_ptr;  // begin: Beneath RING_SIZE.
  word end = begin + n;       // end:  Sum may not be beneath RING_SIZE.

  if (end >= RING_SIZE) {
    word first_n = RING_SIZE - begin;
    word second_n = n - first_n;
    WizPutN(T+begin, data, first_n);
    WizPutN(T, data+first_n, second_n);
  } else {
    WizPutN(T+begin, data, n);
  }
  return (n + tx_ptr) & RING_MASK;
}

void WizFinalizeSend(const struct sock* sockp, const struct proto *proto, size_t n) {
  word tx_wr = WizGet2(B+SK_TX_WR0);
  tx_wr += n;
  WizPut2(B+SK_TX_WR0, tx_wr);
  WizIssueCommand(SOCK_AND  proto->send_command);
}

errnum WizSendChunk(const struct sock* sockp,  const struct proto* proto, char* data, size_t n) {
  PrintH("Ax WizSendChunk %x@%x : %x %x %x %x %x", n, data, 
      data[0], data[1], data[2], data[3], data[4]);
  errnum e = WizCheck(JUST_SOCK);
  if (e) return e;
  tx_ptr_t tx_ptr = WizReserveToSend(SOCK_AND  n);
  WizDataToSend(SOCK_AND tx_ptr, data, n);
  WizFinalizeSend(SOCK_AND proto, n);
  PrintH("Ax WSC.");
  return OKAY;
}
errnum TcpSend(const struct sock* sockp,  char* p, size_t n) {
  while (n) {
    word chunk = (n < TCP_CHUNK_SIZE) ? n : TCP_CHUNK_SIZE;
    errnum e = WizSendChunk(SOCK_AND &TcpProto, p, chunk);
    if (e) return e;
    n -= chunk;
    p += chunk;
  }
  return OKAY;
}

void WizClose(PARAM_JUST_SOCK) {
  WizIssueCommand(SOCK_AND 0x10/*CLOSE*/);
  WizPut1(B+SK_MR, 0x00/*Protocol: Socket Closed*/);
  WizPut1(B+0x0002/*_IR*/, 0x1F); // Clear all interrupts.
}

//////////////////////////////

extern void DoKeyboardCommands(char initial_char);
extern errnum RunDhcp(const struct sock* sockp, const char* name4, word ticks);
const char name4[] = "NONE"; // unused

word RomSum(word begin, word end) {
  word sum = 0;
  while (begin < end) {
    sum += *(word*)begin;
    begin += 2;
  }
  return sum;
}

// --main--

void Discovery() {
#if TODO
  if (!Vars->got_lan) TryLanReply();
  if (!Vars->got_dhcp) TryDhcpReply();
  if (!Vars->got_lan) SendLanRequest();
  if (!Vars->got_dhcp) SendDhcpRequest();
#endif
}

#define TOCKS_PER_SECOND 39 // TOCK = 25.6 milliseconds.

char CountdownOrInitialChar() {
  PrintF("\n\nTo take control, hit space bar.\n\n");
  for (byte i = 0; i < 5; i++) {
    byte t = WizTocks();
    PrintF("%d... ", 5-i);
    Discovery();
    while(1) {
      byte now = WizTocks();
      byte interval = now - t; // Unsigned Difference tolerates rollover.
      if (interval > TOCKS_PER_SECOND) break;

      char initialChar = PolCat();
      if (initialChar) return initialChar;
    }
  }
  PrintF("0.\n");
  return '\0';
}

void ComputeRomSums() {
    Vars->rom_sum_0 = RomSum(0x8000, 0xA000);
    Vars->rom_sum_1 = RomSum(0xA000, 0xC000);
    Vars->rom_sum_2 = RomSum(0xC100, 0xC800);
    Vars->rom_sum_3 = RomSum(0xE000, 0xF000);
}


errnum LemmaClientS1() {  // old style does not loop.
  char quint[5];
  char inkey;
  errnum e;  // was bool e, but that was a mistake.

    inkey = PolCat();
    if (inkey) {
      memset(quint, 0, sizeof quint);
      quint[0] = CMD_INKEY;
      quint[4] = inkey;
      e = WizSendChunk(SOCK1_AND &TcpProto,  quint, sizeof quint);
      if (e) return e;
    }

    e = WizRecvChunkTry(SOCK1_AND quint, sizeof quint);
    if (e == OKAY) {
      word n = *(word*)(quint+1);
      word p = *(word*)(quint+3);
      switch ((byte)quint[0]) {
        case CMD_POKE:
          {
            PrintH("POKE(%x@%x)\n", n, p);
            TcpRecv(SOCK1_AND (char*)p, n);
            PrintH("POKE DONE\n");
          }
          break;
        case CMD_PUTCHAR:
          {
              PutChar(p);
          }
          break;
        case CMD_PEEK:
          {
            PrintH("PEEK(%x@%x)\n", n, p);

            quint[0] = CMD_DATA;
            WizSendChunk(SOCK1_AND &TcpProto, quint, 5);
            TcpSend(SOCK1_AND (char*)p, n);
            PrintH("PEEK DONE\n");
          }
          break;
        case CMD_JSR:
          {
            func_t fn = (func_t)p;
            PrintH("JSR(%x@%x)\n", p);
            fn();
          }
          break;
        default:
          Fatal("WUT?", quint[0]);
          break;
      } // switch
    }
    return (e==NOTYET) ? OKAY : e;
}
extern int main();
void main2() {
    // Clear our global variables to zero, except for orig_s_regs.
    memset(sizeof Vars->orig_s_reg + (char*)Vars,
           0,
           sizeof *Vars - sizeof Vars->orig_s_reg);

    Vars->wiz_port = (struct wiz_port*) (0xFF00 + Rom->rom_wiz_hw_port);
    Vars->transaction_id = WizTicks();

    // Set VDG 32x16 text screen to dots.
    // Preserve the top line.
    Vars->vdg_addr = VDG_RAM;
    Vars->vdg_begin = VDG_RAM+32; // Skip top status line.
    Vars->vdg_ptr = VDG_RAM+32;
    Vars->vdg_end = VDG_END;
    word vdg_n = Vars->vdg_end - Vars->vdg_begin;
    memset((char*)Vars->vdg_begin, '.', vdg_n);

    PrintF("X=%x M=%x:%x\n",
        Vars->transaction_id,
        *(word*)(Rom->rom_mac_tail+0),
        *(word*)(Rom->rom_mac_tail+2));

    ComputeRomSums();
    PrintF("S=%x R=%x:%x:%x:%x\n",
      Vars->rom_sum_0,
      Vars->rom_sum_1,
      Vars->rom_sum_2,
      Vars->rom_sum_3);

    char initial_char = CountdownOrInitialChar();

    if (initial_char == '1') {
      // Choose DHCP
    } else if (initial_char == '2') {
      // Choose LAN
    } else {
      // Choose Keyboard
      DoKeyboardCommands(initial_char);
    }

    PrintF("WizReset; ");
    WizReset();

    if (Vars->need_dhcp) {
      PrintF("DHCP:");
      errnum e = OKAY;
#if TODO
      e = RunDhcp(SOCK0_AND name4, Vars->transaction_id);
#endif
      if (e) Fatal("RunDhcp", e);
    }

    WizConfigure();

#define LOCAL_PORT ( 0x8000 | Vars->transaction_id )
    WizOpen(SOCK1_AND &TcpProto, LOCAL_PORT);
    PrintF("tcp dial %a:%u;", Vars->ip_waiter, Vars->waiter_port);
    TcpDial(SOCK1_AND Vars->ip_waiter, Vars->waiter_port);

    TcpEstablish(JUST_SOCK1);
    PrintF(" CONN; ");
    Delay(50000);

    while (1) {
        errnum e = LemmaClientS1();
        if (e) Fatal("BOTTOM", e);
    }
}

const byte waiter_default [4] = {
      134, 122, 16, 44 }; // lemma.yak.net

void DoKeyboardCommands(char initial_char) {
  // Set up defaults.
  memcpy(Vars->ip_waiter, waiter_default, 4);
  Vars->waiter_port = WAITER_TCP_PORT;
  SetMask(24);

  PrintF("Enter H for HELP.\n");

  while (true) {
    PrintF(">axiom> ");
    bool done = DoOneCommand(initial_char);
    if (done) break;
    initial_char = 0;
  }
  PrintF("Launch... ");
}

int main() {
    Vars->orig_s_reg = StackPointer();

    CallWithNewStack(0x0800, &main2);
}
