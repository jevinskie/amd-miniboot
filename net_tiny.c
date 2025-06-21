/* rtl8139-stage0/net_tiny.c : ARP + minimal UDP/TFTP */
#include "net_tiny.h"
#include "rtl8139_fifo.h"

#define htons(x) __builtin_bswap16(x)
#define htonl(x) __builtin_bswap32(x)

/* ---- compile-time constants (works with qemu-usernet) ---- */
static const uint8_t srv_ip[4]  = {10,0,2,2};
static const uint8_t cli_ip[4]  = {10,0,2,15};
static       uint8_t cli_mac[6];
static       uint8_t srv_mac[6];

static uint16_t udp_port = 50000;   /* arbitrary src port */
static uint16_t tftp_sport;

/* ---- checksum helpers ---- */
static uint16_t csum16(const void *buf, uint32_t len, uint32_t sum)
{
    const uint16_t *p = buf;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len) sum += *(uint8_t *)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return ~sum;
}

/* ---- tiny memcpy/memset ---- */
static void mem_cpy(void *d,const void *s,uint32_t n)
{ uint8_t *D=d; const uint8_t *S=s; while(n--) *D++=*S++; }

static void mem_set(void *d,uint8_t v,uint32_t n)
{ uint8_t *D=d; while(n--) *D++=v; }

/* ---- Ethernet frame tx helper ---- */
static int eth_send(const uint8_t *dst_mac,uint16_t ethertype,
                    const void *payload,uint16_t len)
{
    uint8_t frame[1518];
    mem_cpy(frame,dst_mac,6);
    mem_cpy(frame+6,cli_mac,6);
    *(uint16_t *)(frame+12) = htons(ethertype);
    mem_cpy(frame+14,payload,len);
    return rtl8139_send(frame,len+14);
}

/* ---- ARP ---- */
struct arp_pkt {
    uint16_t htype, ptype;
    uint8_t  hlen, plen;
    uint16_t oper;
    uint8_t  sha[6]; uint8_t spa[4];
    uint8_t  tha[6]; uint8_t tpa[4];
} __attribute__((packed));

static int arp_resolve(void)
{
    struct arp_pkt req;
    req.htype = htons(1); req.ptype = htons(0x0800);
    req.hlen  = 6; req.plen = 4; req.oper = htons(1);
    mem_cpy(req.sha, cli_mac,6); mem_cpy(req.spa, cli_ip,4);
    mem_set(req.tha,0,6);        mem_cpy(req.tpa, srv_ip,4);

    uint8_t bcast[6]; mem_set(bcast,0xFF,6);
    eth_send(bcast,0x0806,&req,sizeof(req));

    /* poll up to ~1 s */
    uint8_t buf[1600]; uint16_t len;
    for (uint32_t i=0;i<1000000;i++) {
        if (rtl8139_read(buf,&len)) {
            if (len>=42 && *(uint16_t *)(buf+12)==htons(0x0806)) {
                struct arp_pkt *p=(void*)(buf+14);
                if (p->oper==htons(2) && !__builtin_memcmp(p->spa,srv_ip,4)) {
                    mem_cpy(srv_mac,p->sha,6);
                    return 0;
                }
            }
        }
    }
    return -1;
}

/* ---- UDP send ---- */
static int udp_send(uint16_t sport,uint16_t dport,
                    const void *data,uint16_t len)
{
    uint8_t pkt[1500]; uint16_t iplen = 20+8+len;
    /* IPv4 header */
    mem_set(pkt,0,20);
    pkt[0]=0x45; pkt[8]=64;
    *(uint16_t*)(pkt+2)=htons(iplen);
    *(uint16_t*)(pkt+10)=csum16(pkt,20,0);
    mem_cpy(pkt+12,cli_ip,4); mem_cpy(pkt+16,srv_ip,4);
    /* UDP */
    uint8_t *udp=pkt+20;
    *(uint16_t*)(udp+0)=htons(sport);
    *(uint16_t*)(udp+2)=htons(dport);
    *(uint16_t*)(udp+4)=htons(8+len);
    mem_cpy(udp+8,data,len);
    /* pseudo-header checksum */
    uint32_t sum=0;
    sum+= (cli_ip[0]<<8)|cli_ip[1]; sum+= (cli_ip[2]<<8)|cli_ip[3];
    sum+= (srv_ip[0]<<8)|srv_ip[1]; sum+= (srv_ip[2]<<8)|srv_ip[3];
    sum+= htons(17); sum+= htons(8+len);
    *(uint16_t*)(udp+6)=csum16(udp,8+len,sum);
    return eth_send(srv_mac,0x0800,pkt,20+8+len);
}

/* ---- net_init ---- */
void net_init(const uint8_t mac[6])
{ mem_cpy(cli_mac,mac,6); }

/* ---- RFC1350 TFTP download ---- */
static int tftp_rrq(const char *fname)
{
    char buf[2+128];
    uint16_t l=0;
    buf[l++]=0; buf[l++]=1;            /* RRQ */
    while(*fname) buf[l++]=*fname++;
    buf[l++]=0;
    const char mode[]="octet";
    for(int i=0;i<6;i++) buf[l++]=mode[i];
    buf[l++]=0;
    return udp_send(udp_port,69,buf,l);
}

int tftp_fetch(const char *fname,uint8_t *dst,uint32_t max_len)
{
    if (arp_resolve()) return -1;
    if (tftp_rrq(fname)) return -1;

    uint8_t frame[1600]; uint16_t flen;
    uint32_t off=0;

    while (1) {
        while(!rtl8139_read(frame,&flen)) ;

        /* verify IPv4/UDP */
        if (flen<42 || *(uint16_t*)(frame+12)!=htons(0x0800))
            continue;
        uint8_t *ip=frame+14;
        uint8_t *udp=ip+20;
        uint16_t sport = ntohs(*(uint16_t*)udp);
        uint16_t dport = ntohs(*(uint16_t*)(udp+2));
        if (dport!=udp_port) continue;

        uint8_t *tp=udp+8;
        uint16_t opcode = ntohs(*(uint16_t*)tp);
        if (opcode==3) {                      /* DATA */
            uint16_t blk = ntohs(*(uint16_t*)(tp+2));
            uint16_t dlen = flen-14-20-8-4;
            if (off+dlen>max_len) return -2;
            mem_cpy(dst+off,tp+4,dlen); off+=dlen;
            /* ACK */
            uint8_t ack[4]={0,4,(blk>>8)&0xFF,blk&0xFF};
            udp_send(udp_port,sport,ack,4);
            if (dlen<512) break;              /* last block */
        } else if (opcode==5) {               /* ERROR */
            return -3;
        } else if (opcode==4) continue;       /* ACK -> ignore */
        if (!tftp_sport) tftp_sport=sport;
    }
    return 0;
}
