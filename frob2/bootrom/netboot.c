// Boot ROM code for CocoIO 

typedef unsigned char byte;
typedef unsigned int word;
typedef unsigned long quad;

#define WIZ_PORT 0xFF68
#define DHCP_HOSTNAME "coco"

#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67

#define VARS_RAM 0x1000  // TODO?
#define VDG_RAM 0x0400  // TODO?
#define VDG_MASK 0x03ff  // TODO?

struct dhcp {
    byte opcode; // 1=request 2=response
    byte htype;  // 1=ethernet
    byte hlen;   // == 6 bytes
    byte hops;   // 1 or 0, on a LAN

    byte xid[4];  // transaction id
    word secs;
    word flags;  // $80 broadcast, $00 unicast.

    quad ciaddr;      // Client IP addr
    quad yiaddr;      // Your IP addr
    quad siaddr;      // Server IP addr
    quad giaddr;      // Gateway IP addr
    byte chaddr[16];  // client hardware addr
    byte sname[64];   // server name
    byte bname[128];  // boot file name

    byte options[300];
};

struct vars {
    byte mac_addr[6];
    byte hostname[4];
    quad my_ipaddr;
    quad my_ipmask;
    quad my_gateway;
    byte* vdg_ptr;
    byte packet[600];
};

#define Vars ((struct vars*)VARS_RAM)

void PutChar(char ch) {
    *Vars->vdg_ptr = (byte)ch;
    Vars->vdg_ptr += 2;                // TODO?
    Vars->vdg_ptr &= VDG_MASK;
    Vars->vdg_ptr |= VDG_RAM;
}
void PutStr(const char* s) {
        while (*s) PutChar(*s);
}

const byte* HexAlphabet = "0123456789abcdef";

void PutHex(quad x) {
  if (x > 15) {
    PutHex(x >> 4);
    // TODO: report bug that (byte)x did not work.
    PutChar( HexAlphabet[ (byte)(word)x & (byte)15 ] );
  } else {
    PutChar( HexAlphabet[ (byte)x ] );
  }
}

void printk(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    const char* s = format;
    while (*s) {
        bool longer = false;
        if (*s == '%') {
NEXT:       ++s;
            switch (*s) {
                case 'l': {
                    longer = true;
                    goto NEXT;
                };
                break;
                case 'x': {
                    quad x;
                    if (longer) {
                        x = va_arg(ap, quad);
                    } else {
                        x = va_arg(ap, word);
                    }
                    PutHex(x);
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
            PutChar((*s < ' ') ? ' ' : *s);
        }
        s++;
    }
    va_end(ap);
}

word bogus_word_for_delay;
void wiz_delay(word n) {
  for (word i=0; i<n; i++) bogus_word_for_delay += i;
}

#define WIZ ((byte*)WIZ_PORT)

static byte peek(word reg) {
  WIZ[1] = (byte)(reg >> 8);
  WIZ[2] = (byte)(reg);
  byte z = WIZ[3];
  printk("[%x->%x]", reg, z);
  return z;
}
static word peek_word(word reg) {
  WIZ[1] = (byte)(reg >> 8);
  WIZ[2] = (byte)(reg);
  byte hi = WIZ[3];
  byte lo = WIZ[3];
  word z = ((word)(hi) << 8) + lo;
  printk("[%x->%x]", reg, z);
  return z;
}
static void poke(word reg, byte value) {
  WIZ[1] = (byte)(reg >> 8);
  WIZ[2] = (byte)(reg);
  WIZ[3] = value;
  printk("[%x<=%x]", reg, value);
}
static void poke_word(word reg, word value) {
  WIZ[1] = (byte)(reg >> 8);
  WIZ[2] = (byte)(reg);
  WIZ[3] = (byte)(value >> 8);
  WIZ[3] = (byte)(value);
  printk("[%x<=%x]", reg, value);
}
static void poke_n(word reg, const void* data, word size) {
  const byte* from = (const byte*) data;
  WIZ[1] = (byte)(reg >> 8);
  WIZ[2] = (byte)(reg);
  for (word i=0; i<size; i++) {
    WIZ[3] = *from++;
  }
}

void wiz_reset() {
  WIZ[0] = 128; // Reset
  wiz_delay(42);
  WIZ[0] = 3;   // IND=1 AutoIncr=1 BlockPingResponse=0 PPPoE=0
  wiz_delay(42);
}

void wiz_configure(quad ip_addr, quad ip_mask, quad ip_gateway) {
  WIZ[1] = 0; WIZ[2] = 1;  // start at addr 0x0001: Gateway IP.
  poke_n(0x0001/*gateway*/, &ip_gateway, 4);
  poke_n(0x0005/*mask*/, &ip_mask, 4);
  poke_n(0x000f/*ip_addr*/, &ip_addr, 4);

  // Create locally assigned mac_addr from ip_addr.
  poke_n(0x0009/*ether_mac*/, Vars->mac_addr, 6);

  poke(0x001a/*=Rx Memory Size*/, 0x55); // 2k per sock
  poke(0x001b/*=Tx Memory Size*/, 0x55); // 2k per sock

  // Force all 4 sockets to be closed.
  for (byte socknum=0; socknum<4; socknum++) {
      word base = ((word)socknum + 4) << 8;
      poke(base+SockCommand, 0x10/*CLOSE*/);
      wait(base+SockCommand, 0, 500);
      poke(base+SockMode, 0x00/*Protocol: Socket Closed*/);
      poke(base+0x001e/*_RXBUF_SIZE*/, 2); // 2KB
      poke(base+0x001f/*_TXBUF_SIZE*/, 2); // 2KB
  }
}

void wiz_configure_for_DHCP(const char* name4, byte* hw6_out) {
  wiz_configure(0L, 0xFFFFFF00, 0L);

  // Create locally assigned mac_addr from name4.
  byte mac_addr[6] = {2, 32, 0, 0, 0, 0};  // local prefix.
  strncpy((char*)mac_addr+2, name4, 4);
  poke_n(0x0009/*ether_mac*/, mac_addr, 6);
  memcpy(hw6_out, mac_addr, 6);
}

void wiz_reconfigure_for_DHCP(quad ip_addr, quad ip_mask, quad ip_gateway) {
  WIZ[1] = 0; WIZ[2] = 1;  // start at addr 0x0001: Gateway IP.
  poke_n(0x0001/*gateway*/, &ip_gateway, 4);
  poke_n(0x0005/*mask*/, &ip_mask, 4);
  poke_n(0x000f/*ip_addr*/, &ip_addr, 4);
  // Keep the same mac_addr.
}

void udp_close() {
  printk("CLOSE");

  word base = 0x0400;
  poke(base+0x0001/*_IR*/, 0x1F); // Clear all interrupts.
  poke(base+SockCommand, 0x10/*CLOSE*/);
  wait(base+SockCommand, 0, 500);
  poke(base+SockMode, 0x00/*Protocol: Socket Closed*/);
}

errnum udp_open(word src_port) {
  errnum err = OKAY;
  word base = 0x400;

  poke(base+SockMode, 2); // Set UDP Protocol mode.
  poke_word(base+SockSourcePort, src_port);
  poke(base+0x0001/*_IR*/, 0x1F); // Clear all interrupts.
  poke(base+0x002c/*_IMR*/, 0xFF); // mask all interrupts.
  poke_word(base+0x002d/*_FRAGR*/, 0); // don't fragment.

  poke(base+0x002f/*_MR2*/, 0x00); // no blocks.
  poke(base+SockCommand, 1/*=OPEN*/);  // OPEN IT!
  err = wait(base+SockCommand, 0, 500);
  if (err) goto Enable;

  err = wait(base+SockStatus, 0x22/*SOCK_UDP*/, 500);
  if (err) goto Enable;

  word tx_r = peek_word(base+TxReadPtr);
  poke_word(base+TxWritePtr, tx_r);
  word rx_w = peek_word(base+0x002A/*_RX_WR*/);
  poke_word(base+0x0028/*_RX_RD*/, rx_w);
  return err;
}

errnum udp_send(byte* payload, word size, quad dest_ip, word dest_port) {
  printk("SEND: sock=%x payload=%x size=%x ", socknum, payload, size);
  byte* d = (byte*)&dest_ip;
  printk(" dest=%x.%x.%x.%x:%x(dec) ", d[0], d[1], d[2], d[3], dest_port);

  word base = 0x400;
  word buf = TX_BUF(0);

  byte status = peek(base+SockStatus);
  if (status != 0x22/*SOCK_UDP*/) return 0xf6 /*E_NOTRDY*/;

  printk("dest_ip ");
  poke_n(base+SockDestIp, &dest_ip, sizeof dest_ip);
  printk("dest_p ");
  poke_word(base+SockDestPort, dest_port);

  bool broadcast = false;
  byte send_command = 0x20;
  if (dest_ip == 0xFFFFFFFFL) {
    // Broadcast to 255.255.255.255
    broadcast = true;
    send_command = 0x21;
    poke_n(base+6/*Sn_DHAR*/, "\xFF\xFF\xFF\xFF\xFF\xFF", 6);
  }

  word free = peek_word(base + TxFreeSize);
  printk("SEND: base=%x buf=%x free=%x ", base, buf, free);
  if (free < size) return 255; // no buffer room.

  word tx_r = peek_word(base+TxReadPtr);
  printk("tx_r=%x ", tx_r);
  printk("size=%x ", size);
  printk("tx_r+size=%x ", tx_r+size);
  printk("TX_SIZE=%x ", TX_SIZE);
  word offset = TX_MASK & tx_r;
  if (offset + size >= TX_SIZE) {
    // split across edges of circular buffer.
    word size1 = TX_SIZE - offset;
    word size2 = size - size1;
    poke_n(buf + offset, payload, size1);  // 1st part
    poke_n(buf, payload + size1, size2);   // 2nd part
  } else {
    // contiguous within the buffer.
    poke_n(buf + tx_r, payload, size);  // whole thing
  }

  printk("size ");
  word tx_w = peek_word(base+TxWritePtr);
  poke_word(base+TxWritePtr, tx_w + size);

  printk("status->%x ", peek(base+SockStatus));
  //sock_show(socknum);
  printk("cmd:SEND ");

  poke(base+SockInterrupt, 0x1f);  // Reset interrupts.
  poke(base+SockCommand, send_command);  // SEND IT!
  wait(base+SockCommand, 0, 500);
  printk("status->%x ", peek(base+SockStatus));

  while(1) {
    byte irq = peek(base+SockInterrupt);
    if (irq&0x10) break;
  }
  poke(base+SockInterrupt, 0x10);  // Reset RECV interrupt.
  return OKAY;
}

errnum udp_recv(byte* payload, word* size_in_out, quad* from_addr_out, word* from_port_out) {
  word base = 0x0400;
  word buf = RX_BUF(0);

  byte status = peek(base+SockStatus);
  if (status != 0x22/*SOCK_UDP*/) return 0xf6 /*E_NOTRDY*/;

  poke_word(base+0x000c, 0); // clear Dest IP Addr
  poke_word(base+0x000e, 0); // ...
  poke_word(base+0x0010, 0); // clear Dest port addr

  poke(base+SockInterrupt, 0x1f);  // Reset interrupts.
  poke(base+SockCommand, 0x40/*=RECV*/);  // RECV command.
  wait(base+SockCommand, 0, 500);
  printk("status->%x ", peek(base+SockStatus));

  printk(" ====== WAIT ====== ");
  while(1) {
    byte irq = peek(base+SockInterrupt);
    if (irq) {
      poke(base+SockInterrupt, 0x1f);  // Reset interrupts.
      if (irq != 0x04 /*=RECEIVED*/) {
        return 0xf4/*=E_READ*/;
      }
      break;
    }
  }

  word recv_size = peek_word(base+0x0026/*_RX_RSR*/);
  word rx_rd = peek_word(base+0x0028/*_RX_RD*/);
  word rx_wr = peek_word(base+0x002A/*_RX_WR*/);

  word ptr = rx_rd;
  ptr &= RX_MASK;

  struct UdpRecvHeader hdr;
  for (word i = 0; i < sizeof hdr; i++) {
      ((byte*)&hdr)[i] = peek(buf+ptr);
      ptr++;
      ptr &= RX_MASK;
  }
  
  if (hdr.len > *size_in_out) {
    printk(" *** Packet Too Big [rs=%x. sio=%x.]\n", hdr.len, *size_in_out);
    return 0xed/*E_NORAM*/;
  }

  for (word i = 0; i < hdr.len; i++) {
      payload[i] = peek(buf+ptr);
      ptr++;
      ptr &= RX_MASK;
  }
  *size_in_out = hdr.len;
  *from_addr_out = hdr.addr;
  *from_port_out = hdr.port;

  poke_word(base+0x0028/*_RX_RD*/, rx_rd + recv_size);

  return OKAY;
}

void UseOptions(byte* o) {
    if (o[0]!=99 || o[1]!=130 || o[2]!=83 || o[3]!=99) {
        printk("UseOptions: bad magic: %lx", *(quad*)o);
        Fatal();
    }
    byte* p = o+4;
    while (*p != 255) {
        byte opt = *p++;
        byte len = *p++;
        switch (opt) {
            case 1: // subnet mask
                Vars->ip_mask = *(quad*)p;
                printk("ip_mask %lx ", Vars->ip_mask);
                break;
            case 3: // Gateway
                Vars->ip_gateway = *(quad*)p;
                printk("ip_gateway %lx ", Vars->ip_gateway);
                break;
            case 6: // DNS Server
                Vars->ip_dns_server = *(quad*)p;
                printk("ip_dns_server %lx ", Vars->ip_dns_server);
                break;
            default:
                printk("(opt %x len %x) ", opt, len);
        }
        p += len;
    }
}

void RunDhcp() {
    struct dhcp* p = (struct dhcp*) Vars->packet;
    p->opcode = 1; // request
    p->htype = 1; // ethernet
    p->hlen = 6; // 6-byte hw addrs
    p->hops = 0;
    memcpy(p->xid, Name, 4);
    p->flags = 0x80; // broadcast
    memcpy(p->chaddr, MacAddr, 6);
    strcpy((char*)p->bname, "frobio");

    // The first four octets of the 'options' field of the DHCP message
    // contain the (decimal) values 99, 130, 83 and 99, respectively
    byte *w = p->options;
    *w++ = 99;  // magic cookie for DHCP
    *w++ = 130;
    *w++ = 83;
    *w++ = 99;

    *w++ = 53; // 53 = DHCP Message Type Option
    *w++ = 1;  // length 1 byte
    *w++ = 1;  // 1 = Discover
    
    *w++ = 12;  // 12 = Hostname
    *w++ = 4;  // length 4 chars
    *w++ = p->xid[0];
    *w++ = p->xid[1];
    *w++ = p->xid[2];
    *w++ = p->xid[3];

    *w++ = 255;  // 255 = End
    *w++ = 0;  // length 0 bytes
    DumpDHCP(&Discover);

    errnum e = udp_open(68);
    if (e) {
        printk("cannot udp_open: e=%x.", e);
        Fatal();
    }
    e = udp_send((byte*)p, sizeof *p, 0xFFFFFFFFL, 67);
    if (e) {
        printk("cannot udp_send: e=%x.", e);
        Fatal();
    }
    word size = sizeof Offer;
    quad recv_from = 0;
    word recv_port = 0;
    e = udp_recv((byte*)&Offer, &size, &recv_from, &recv_port);
    if (e) {
        printk("cannot udp_recv: e=%x.", e);
        Fatal();
    }
    DumpDHCP(&Offer);
    quad yiaddr = Offer.yiaddr;
    quad siaddr = Offer.siaddr;

    byte* yi = (byte*)&Offer.yiaddr;
    byte* si = (byte*)&Offer.siaddr;
    printk("you=%x.%x.%x.%x  server=%x.%x.%x.%x",
        yi[0], yi[1], yi[2], yi[3],
        si[0], si[1], si[2], si[3]
        );

    UseOptions(Offer.options);

    ////////////////////////////////////////////////////////////////
    // TODO: correct mask & gateway.
    wiz_reconfigure_for_DHCP(yiaddr, ip_mask, ip_gateway);
    ////////////////////////////////////////////////////////////////

    memset(Vars->packet, 0, 600);
    p->opcode = 1; // request
    p->htype = 1; // ethernet
    p->hlen = 6; // 6-byte hw addrs
    p->hops = 0;
    memcpy(p->xid, Name, 4);
    p->flags = 0x80; // broadcast
    p->ciaddr = yiaddr;
    p->yiaddr = yiaddr;
    p->siaddr = siaddr;
    memcpy(p->chaddr, MacAddr, 6);
    strcpy((char*)p->bname, "frobio");

    // The first four octets of the 'options' field of the DHCP message
    // contain the (decimal) values 99, 130, 83 and 99, respectively
    w = p->options;
    *w++ = 99;  // magic cookie for DHCP
    *w++ = 130;
    *w++ = 83;
    *w++ = 99;

    *w++ = 53; // 53 = DHCP Message Type Option
    *w++ = 1;  // length 1 byte
    *w++ = 3;  // 3 = Request
    
    *w++ = 50;  // 50 = Requested Address
    *w++ = 4;  // length 4
    *w++ = yi[0];
    *w++ = yi[1];
    *w++ = yi[2];
    *w++ = yi[3];

    *w++ = 12;  // 12 = Hostname
    *w++ = 4;  // length 4 chars
    *w++ = p->xid[0];
    *w++ = p->xid[1];
    *w++ = p->xid[2];
    *w++ = p->xid[3];

    *w++ = 255;  // 255 = End
    *w++ = 0;  // length 0 bytes
    DumpDHCP(&Request);

    e = udp_send((byte*)p, sizeof *p, 0xFFFFFFFFL, 67);
    if (e) {
        printk("cannot udp_send: e=%x.\n", e);
        Fatal();
    }
    size = sizeof Ack;
    recv_from = 0;
    recv_port = 0;
    e = udp_recv((byte*)&Ack, &size, &recv_from, &recv_port);
    if (e) {
        printk("cannot udp_recv: e=%x.\n", e);
        Fatal();
    }
    DumpDHCP(&Ack);

    // TODO: check for Ack option.

    yiaddr = Ack.yiaddr;
    siaddr = Ack.siaddr;

    yi = (byte*)&Offer.yiaddr;
    si = (byte*)&Offer.siaddr;
    byte* ma = (byte*)&ip_mask;
    byte* ga = (byte*)&ip_gateway;
    byte* dn = (byte*)&ip_dns_server;

    printk("you=%x.%x.%x.%x  server=%x.%x.%x.%x\n",
        yi[0], yi[1], yi[2], yi[3],
        si[0], si[1], si[2], si[3]);

    printk("mask=%x.%x.%x.%x  gateway=%x.%x.%x.%x  dns=%x.%x.%x.%x\n",
        ma[0], ma[1], ma[2], ma[3],
        ga[0], ga[1], ga[2], ga[3],
        dn[0], dn[1], dn[2], dn[3]);
}

#define DEFAULT_TFTPD_PORT 69 /* TFTPD */

// Operations defined by TFTP protocol:
#define OP_READ 1
#define OP_WRITE 2
#define OP_DATA 3
#define OP_ACK 4
#define OP_ERROR 5

void TftpRequest(quad host, word port, word opcode, const char* filename) {
  char* p = (char*)packet;
  *(word*)p = opcode;
  p += 2;

  int n = strlen(filename);
  strcpy(p, filename);
  p += n+1;

  const char* mode = "octet";
  n = strlen(mode);
  strcpy(p, mode);
  p += n+1;

  errnum err = udp_send(packet, p-(char*)packet, host, port);
  if (err) {
    printk("cannot udp_send request: errnum %x", err);
    Fatal();
  }
}

void Boot() {
    memset(VARS_RAM, 0, sizeof (struct vars));

    Vars->vdg_ptr = VDG_RAM;
    PutStr("Hello BootRom ");
    
    wiz_reset();
    PutStr("wiz_reset ");
    MemCpy(Vars->hostname, DHCP_HOSTNAME, 4);
    wiz_configure_for_DHCP(DHCP_HOSTNAME, Vars->mac_addr);
    PutStr("wiz_configure_for_DHCP ");

    RunDhcp();
    RunTftpGet();
    udp_close();
    char* addr = Var->packet;
    asm {
        lbra [addr]
    }
}
