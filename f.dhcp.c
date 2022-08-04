// f.dhcp NAME

#include "frobio/frobio.h"
#include "frobio/nylib.h"
#include "frobio/os9call.h"

#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67

typedef struct dhcp {
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

    byte options[100];
} DHCP;

char* Name;      // 4-char name.
byte MacAddr[6];  // 6-byte MAC addr.

DHCP Discover;
DHCP Offer;
DHCP Request;
DHCP Ack;

void DumpDHCP(DHCP* p) {
    printf("opcode=%x htype=%x hlen=%x hops=%x  ",
        p->opcode, p->htype, p->hlen, p->hops);
    printf("xid=%02x %02x %02x %02x  ",
        p->xid[0], p->xid[1], p->xid[2], p->xid[3]); 
    printf("secs=%x flags=%x  ", p->secs, p->flags);
    printf("ci=%lx yi=%lx si=%lx gi=%lx\n",
        p->ciaddr, p->yiaddr, p->siaddr, p->giaddr);
    printf("chaddr: ");
    for (byte i=0; i<16; i++) printf("%02x ", p->chaddr[i]);
    //printf("\n sname: ");
    //for (byte i=0; i<64; i++) printf("%02x ", p->sname[i]);
    //printf("\n bname: ");
    //for (byte i=0; i<128; i++) printf("%02x ", p->bname[i]);
    printf("\noptions: ");
    for (byte i=0; i<66; i++) printf("%02x ", p->options[i]);
    printf("\n");
}

quad ip_mask;
quad ip_gateway;
quad ip_dns_server;

void UseOptions(byte* o) {
    if (o[0]!=99 || o[1]!=130 || o[2]!=83 || o[3]!=99) {
        printf("bad magic cookie\n");
        return;
    }
    byte* p = o+4;
    while (*p != 255) {
        byte opt = *p++;
        byte len = *p++;
        switch (opt) {
            case 1: // subnet mask
                ip_mask = *(quad*)p;
                printf("ip_mask %lx ", ip_mask);
                break;
            case 3: // Gateway
                ip_gateway = *(quad*)p;
                printf("ip_gateway %lx ", ip_gateway);
                break;
            case 6: // DNS Server
                ip_dns_server = *(quad*)p;
                printf("ip_dns_server %lx ", ip_dns_server);
                break;
            default:
                printf("(opt %d len %d) ", opt, len);
        }
        p += len;
    }
    printf("(opt END)\n");
}

void Run() {
    DHCP* p = &Discover;
    p->opcode = 1; // request
    p->htype = 1; // ethernet
    p->hlen = 6; // 6-byte hw addrs
    p->hops = 0;
    memcpy(p->xid, Name, 4);
    p->flags = 0x80; // broadcast
    memcpy(p->chaddr, MacAddr, 6);
    strcpy(p->bname, "frobio");

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

    byte sock = 0;
    error e = udp_open(68, &sock);
    if (e) {
        printf("*** cannot udp_open(88): e=%d.\n", e);
        exit(e);
    }
    e = udp_send(sock, (byte*)p, sizeof *p, 0xFFFFFFFFL, 67);
    if (e) {
        printf("*** cannot udp_send(93): e=%d.\n", e);
        exit(e);
    }
    word size = sizeof Offer;
    quad recv_from = 0;
    word recv_port = 0;
    e = udp_recv(sock, (byte*)&Offer, &size, &recv_from, &recv_port);
    if (e) {
        printf("*** cannot udp_recv(101): e=%d.\n", e);
        exit(e);
    }
    DumpDHCP(&Offer);
    quad yiaddr = Offer.yiaddr;
    quad siaddr = Offer.siaddr;

    byte* yi = (byte*)&Offer.yiaddr;
    byte* si = (byte*)&Offer.siaddr;
    printf("yi=%d.%d.%d.%d  si=%d.%d.%d.%d\n",
        yi[0], yi[1], yi[2], yi[3],
        si[0], si[1], si[2], si[3]
        );

    UseOptions(Offer.options);

    ////////////////////////////////////////////////////////////////
    // TODO: correct mask & gateway.
    wiz_reconfigure_for_DHCP(yiaddr, ip_mask, ip_gateway);
    ////////////////////////////////////////////////////////////////

    p = &Request;
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
    strcpy(p->bname, "frobio");

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

    e = udp_send(sock, (byte*)p, sizeof *p, 0xFFFFFFFFL, 67);
    if (e) {
        printf("*** cannot udp_send(147): e=%d.\n", e);
        exit(e);
    }
    size = sizeof Ack;
    recv_from = 0;
    recv_port = 0;
    e = udp_recv(sock, (byte*)&Ack, &size, &recv_from, &recv_port);
    if (e) {
        printf("*** cannot udp_recv(155): e=%d.\n", e);
        exit(e);
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
    printf("yi=%d.%d.%d.%d  si=%d.%d.%d.%d ",
        yi[0], yi[1], yi[2], yi[3],
        si[0], si[1], si[2], si[3]
        );
    printf("mask=%d.%d.%d.%d  gateway=%d.%d.%d.%d  dns=%d.%d.%d.%d\n",
        ma[0], ma[1], ma[2], ma[3],
        ga[0], ga[1], ga[2], ga[3],
        dn[0], dn[1], dn[2], dn[3]
        );
}

void Usage() {
    printf("Usage:   f.dhcp NAME (must be unique 4 letter name)\n");
}

int main(int argc, char *argv[]) {
    --argc, ++argv; // skip unused argv[0]

    if (argc != 1 || strlen(argv[0]) != 4) {
        Usage();
        return 2;
    }

    Name = argv[0];
    wiz_reset();
    wiz_configure_for_DHCP(Name /*in*/, MacAddr /*out*/);
    Run();
    return 0;
}

/*

01:45:16.746400 IP (tos 0x0, ttl 128, id 1, offset 0, flags [none], proto UDP (17), length 364)
    0.0.0.0.68 > 255.255.255.255.67: BOOTP/DHCP, Request from 02:20:71:77:65:72, length 336, xid 0x71776572, Flags [none]
	  Client-Ethernet-Address 02:20:71:77:65:72
	  file "frobio"
	  Vendor-rfc1048 Extensions
	    Magic Cookie 0x63825363
	    DHCP-Message Option 53, length 1: Discover
	    Hostname Option 12, length 4: "qwer"
	0x0000:  4500 016c 0001 0000 8011 3981 0000 0000  E..l......9.....
	0x0010:  ffff ffff 0044 0043 0158 2a36 0101 0600  .....D.C.X*6....
	0x0020:  7177 6572 0000 0080 0000 0000 0000 0000  qwer............
	0x0030:  0000 0000 0000 0000 0220 7177 6572 0000  ..........qwer..
	0x0040:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0050:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0060:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0070:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0080:  0000 0000 0000 0000 6672 6f62 696f 0000  ........frobio..
	0x0090:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00a0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00b0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00c0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00d0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00e0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00f0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0100:  0000 0000 0000 0000 6382 5363 3501 010c  ........c.Sc5...
	0x0110:  0471 7765 72ff 0000 0000 0000 0000 0000  .qwer...........
	0x0120:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0130:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0140:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0150:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0160:  0000 0000 0000 0000 0000 0000            ............
01:45:16.746825 ARP, Ethernet (len 6), IPv4 (len 4), Request who-has 10.8.15.66 tell 10.8.15.65, length 28
	0x0000:  0001 0800 0604 0001 f0de f183 eb8c 0a08  ................
	0x0010:  0f41 0000 0000 0000 0a08 0f42            .A.........B
01:45:17.748193 IP (tos 0x10, ttl 128, id 0, offset 0, flags [none], proto UDP (17), length 328)
    10.8.15.65.67 > 10.8.15.66.68: BOOTP/DHCP, Reply, length 300, xid 0x71776572, Flags [none]
	  Your-IP 10.8.15.66
	  Server-IP 10.8.15.65
	  Client-Ethernet-Address 02:20:71:77:65:72
	  Vendor-rfc1048 Extensions
	    Magic Cookie 0x63825363
	    DHCP-Message Option 53, length 1: Offer
	    Server-ID Option 54, length 4: 10.8.15.65
	    Lease-Time Option 51, length 4: 462
	    Subnet-Mask Option 1, length 4: 255.255.255.192
	    Default-Gateway Option 3, length 4: 10.8.15.65
	    Domain-Name-Server Option 6, length 4: 10.8.0.1
	    Domain-Name Option 15, length 11: "example.org"
	0x0000:  4510 0148 0000 0000 8011 0703 0a08 0f41  E..H...........A
	0x0010:  0a08 0f42 0043 0044 0134 b549 0201 0600  ...B.C.D.4.I....
	0x0020:  7177 6572 0000 0080 0000 0000 0a08 0f42  qwer...........B
	0x0030:  0a08 0f41 0000 0000 0220 7177 6572 0000  ...A......qwer..
	0x0040:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0050:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0060:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0070:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0080:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0090:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00a0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00b0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00c0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00d0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00e0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00f0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0100:  0000 0000 0000 0000 6382 5363 3501 0236  ........c.Sc5..6
	0x0110:  040a 080f 4133 0400 0001 ce01 04ff ffff  ....A3..........
	0x0120:  c003 040a 080f 4106 040a 0800 010f 0b65  ......A........e
	0x0130:  7861 6d70 6c65 2e6f 7267 ff00 0000 0000  xample.org......
	0x0140:  0000 0000 0000 0000                      ........
01:45:17.773527 ARP, Ethernet (len 6), IPv4 (len 4), Request who-has 10.8.15.66 tell 10.8.15.65, length 28
	0x0000:  0001 0800 0604 0001 f0de f183 eb8c 0a08  ................
	0x0010:  0f41 0000 0000 0000 0a08 0f42            .A.........B
01:45:18.797562 ARP, Ethernet (len 6), IPv4 (len 4), Request who-has 10.8.15.66 tell 10.8.15.65, length 28
	0x0000:  0001 0800 0604 0001 f0de f183 eb8c 0a08  ................
	0x0010:  0f41 0000 0000 0000 0a08 0f42            .A.........B
01:45:21.224321 IP (tos 0x0, ttl 128, id 2, offset 0, flags [none], proto UDP (17), length 364)
    10.8.15.66.68 > 255.255.255.255.67: BOOTP/DHCP, Request from 02:20:71:77:65:72, length 336, xid 0x71776572, Flags [none]
	  Client-IP 10.8.15.66
	  Your-IP 10.8.15.66
	  Server-IP 10.8.15.65
	  Client-Ethernet-Address 02:20:71:77:65:72
	  file "frobio"
	  Vendor-rfc1048 Extensions
	    Magic Cookie 0x63825363
	    DHCP-Message Option 53, length 1: Request
	    Requested-IP Option 50, length 4: 10.8.15.66
	    Hostname Option 12, length 4: "qwer"
	0x0000:  4500 016c 0002 0000 8011 2036 0a08 0f42  E..l.......6...B
	0x0010:  ffff ffff 0044 0043 0158 74c3 0101 0600  .....D.C.Xt.....
	0x0020:  7177 6572 0000 0080 0a08 0f42 0a08 0f42  qwer.......B...B
	0x0030:  0a08 0f41 0000 0000 0220 7177 6572 0000  ...A......qwer..
	0x0040:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0050:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0060:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0070:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0080:  0000 0000 0000 0000 6672 6f62 696f 0000  ........frobio..
	0x0090:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00a0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00b0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00c0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00d0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00e0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00f0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0100:  0000 0000 0000 0000 6382 5363 3501 0332  ........c.Sc5..2
	0x0110:  040a 080f 420c 0471 7765 72ff 0000 0000  ....B..qwer.....
	0x0120:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0130:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0140:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0150:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0160:  0000 0000 0000 0000 0000 0000            ............
01:45:21.225061 ARP, Ethernet (len 6), IPv4 (len 4), Request who-has 10.8.15.66 tell 10.8.15.65, length 28
	0x0000:  0001 0800 0604 0001 f0de f183 eb8c 0a08  ................
	0x0010:  0f41 0000 0000 0000 0a08 0f42            .A.........B
01:45:21.225207 ARP, Ethernet (len 6), IPv4 (len 4), Reply 10.8.15.66 is-at 02:20:71:77:65:72, length 46
	0x0000:  0001 0800 0604 0002 0220 7177 6572 0a08  ..........qwer..
	0x0010:  0f42 f0de f183 eb8c 0a08 0f41 0000 0000  .B.........A....
	0x0020:  0000 0000 0000 0000 0000 0000 0000       ..............
01:45:21.225232 IP (tos 0x0, ttl 64, id 18558, offset 0, flags [DF], proto UDP (17), length 328)
    10.8.15.65.67 > 10.8.15.66.68: BOOTP/DHCP, Reply, length 300, xid 0x71776572, Flags [none]
	  Client-IP 10.8.15.66
	  Your-IP 10.8.15.66
	  Server-IP 10.8.15.65
	  Client-Ethernet-Address 02:20:71:77:65:72
	  Vendor-rfc1048 Extensions
	    Magic Cookie 0x63825363
	    DHCP-Message Option 53, length 1: ACK
	    Server-ID Option 54, length 4: 10.8.15.65
	    Lease-Time Option 51, length 4: 457
	    Subnet-Mask Option 1, length 4: 255.255.255.192
	    Default-Gateway Option 3, length 4: 10.8.15.65
	    Domain-Name-Server Option 6, length 4: 10.8.0.1
	    Domain-Name Option 15, length 11: "example.org"
	0x0000:  4500 0148 487e 4000 4011 be94 0a08 0f41  E..HH~@.@......A
	0x0010:  0a08 0f42 0043 0044 0134 33d8 0201 0600  ...B.C.D.43.....
	0x0020:  7177 6572 0000 0080 0a08 0f42 0a08 0f42  qwer.......B...B
	0x0030:  0a08 0f41 0000 0000 0220 7177 6572 0000  ...A......qwer..
	0x0040:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0050:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0060:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0070:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0080:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0090:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00a0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00b0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00c0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00d0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00e0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x00f0:  0000 0000 0000 0000 0000 0000 0000 0000  ................
	0x0100:  0000 0000 0000 0000 6382 5363 3501 0536  ........c.Sc5..6
	0x0110:  040a 080f 4133 0400 0001 c901 04ff ffff  ....A3..........
	0x0120:  c003 040a 080f 4106 040a 0800 010f 0b65  ......A........e
	0x0130:  7861 6d70 6c65 2e6f 7267 ff00 0000 0000  xample.org......
	0x0140:  0000 0000 0000 0000                      ........


*/
