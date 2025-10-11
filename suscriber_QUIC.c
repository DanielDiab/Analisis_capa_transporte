#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

unsigned short mi_htons(unsigned short x) {
    return ((x & 0x00FF) << 8) | ((x & 0xFF00) >> 8);
}
unsigned int mi_htonl(unsigned int x) {
    return ((x & 0x000000FF) << 24) |
           ((x & 0x0000FF00) << 8)  |
           ((x & 0x00FF0000) >> 8)  |
           ((x & 0xFF000000) >> 24);
}
void mi_bzero(void *s, int n) {
    char *p = s;
    for (int i = 0; i < n; i++) p[i] = 0;
}

// Definiciones de sockets
#define AF_INET 2
#define SOCK_DGRAM 2
#define INADDR_ANY ((unsigned long int) 0x00000000)
#define MAX_TAM 1024

// Estructuras básicas
struct in_addr {
    unsigned long s_addr;
};

struct sockaddr_in {
    short sin_family;
    unsigned short sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

struct sockaddr {
    unsigned short sa_family;
    char sa_data[14];
};

// Funciones de red externas
extern int socket(int, int, int);
extern int bind(int, const void *, unsigned int);
extern int sendto(int, const void *, unsigned long, int, const void *, unsigned int);
extern int recvfrom(int, void *, unsigned long, int, void *, unsigned int *);
extern int close(int);

// Protocolo fiable simplificado
#define FLAG_SYN  0x01
#define FLAG_ACK  0x02
#define FLAG_DATA 0x08
#define FLAG_SUB  0x10

#pragma pack(push, 1)
typedef struct {
    unsigned int   conn_id;
    unsigned short stream_id;
    unsigned int   seq;
    unsigned int   ack;
    unsigned short wnd;
    unsigned char  flags;
    unsigned char  rsv;
} hdr_t;
#pragma pack(pop)

void hdr_hton(hdr_t *out, const hdr_t *in) {
    out->conn_id  = mi_htonl(in->conn_id);
    out->stream_id= mi_htons(in->stream_id);
    out->seq      = mi_htonl(in->seq);
    out->ack      = mi_htonl(in->ack);
    out->wnd      = mi_htons(in->wnd);
    out->flags    = in->flags;
    out->rsv      = 0;
}

void hdr_ntoh(hdr_t *out, const hdr_t *in) {
    out->conn_id  = mi_htonl(in->conn_id);
    out->stream_id= mi_htons(in->stream_id);
    out->seq      = mi_htonl(in->seq);
    out->ack      = mi_htonl(in->ack);
    out->wnd      = mi_htons(in->wnd);
    out->flags    = in->flags;
    out->rsv      = 0;
}

// MAIN
int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <IP_BROKER> <PUERTO> <TOPIC>\n", argv[0]);
        return 1;
    }

    char *ip = argv[1];
    int puerto = atoi(argv[2]);
    char *topic = argv[3];

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); exit(1); }

    struct sockaddr_in broker;
    mi_bzero(&broker, sizeof(broker));
    broker.sin_family = AF_INET;
    broker.sin_port = mi_htons((unsigned short)puerto);
    broker.sin_addr.s_addr = INADDR_ANY; // simplificación

    // enviar suscripción inicial
    char payload[MAX_TAM];
    snprintf(payload, sizeof(payload), "TYPE:SUBSCRIBER|TOPIC:%s", topic);

    hdr_t h; mi_bzero(&h, sizeof(h));
    unsigned int conn_id = (unsigned int)getpid();
    h.conn_id = conn_id;
    h.stream_id = 0;
    h.seq = 0;
    h.ack = 0;
    h.wnd = 8;
    h.flags = FLAG_DATA | FLAG_SUB;

    hdr_t hn; hdr_hton(&hn, &h);

    char paquete[sizeof(hdr_t) + MAX_TAM];
    memcpy(paquete, &hn, sizeof(hn));
    strcpy(paquete + sizeof(hn), payload);

    int total = sizeof(hn) + strlen(payload);
    sendto(sock, paquete, total, 0, (const void*)&broker, sizeof(broker));

    printf("Suscrito al topic [%s]. Esperando mensajes...\n", topic);

    // bucle principal
    char buffer[MAX_TAM];
    while (1) {
        unsigned int blen = sizeof(broker);
        int n = recvfrom(sock, buffer, sizeof(buffer), 0, (void*)&broker, &blen);
        if (n <= 0) continue;

        if (n >= (int)sizeof(hdr_t)) {
            hdr_t hn_in; memcpy(&hn_in, buffer, sizeof(hn_in));
            hdr_t h_in; hdr_ntoh(&h_in, &hn_in);

            if (h_in.flags & FLAG_DATA) {
                char *msg = buffer + sizeof(hdr_t);
                printf("Mensaje recibido: %s\n", msg);

                // enviar ACK
                hdr_t ack; mi_bzero(&ack, sizeof(ack));
                ack.conn_id = conn_id;
                ack.stream_id = 0;
                ack.seq = 0;
                ack.ack = h_in.seq;  // confirmar último
                ack.wnd = 8;
                ack.flags = FLAG_ACK;

                hdr_t ackn; hdr_hton(&ackn, &ack);
                sendto(sock, &ackn, sizeof(ackn), 0,
                       (const void*)&broker, sizeof(broker));
            }
        }
    }

    close(sock);
    return 0;
}