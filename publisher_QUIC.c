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

// Definiciones de funciones de red
extern int socket(int, int, int);
extern int bind(int, const void *, unsigned int);
extern int sendto(int, const void *, unsigned long, int, const void *, unsigned int);
extern int recvfrom(int, void *, unsigned long, int, void *, unsigned int *);
extern int close(int);

// Protocolo fiable simplificado
#define FLAG_SYN  0x01
#define FLAG_ACK  0x02
#define FLAG_DATA 0x08

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
    broker.sin_addr.s_addr = INADDR_ANY; // simplificación, usar ip=0.0.0.0 

    char mensaje[MAX_TAM - 8];
    unsigned int seq = 0;
    unsigned int conn_id = (unsigned int)getpid(); // identificador simple

    while (1) {
        printf("Escribe mensaje (o 'salir'): ");
        fflush(stdout);

        if (!fgets(mensaje, sizeof(mensaje), stdin)) break;
        if (strncmp(mensaje, "salir", 5) == 0) break;

        // Construir payload "TOPIC:xxx| MESSAGE:..."
        char payload[MAX_TAM];
        snprintf(payload, sizeof(payload), "T: %s\nM: %s", topic, mensaje);

        // header bien formado
        hdr_t h; mi_bzero(&h, sizeof(h));
        h.conn_id = conn_id;
        h.stream_id = 0;
        h.seq = seq++;
        h.ack = 0;
        h.wnd = 8;
        h.flags = FLAG_DATA;

        hdr_t hn; hdr_hton(&hn, &h);

        char paquete[sizeof(hdr_t) + MAX_TAM];
        memcpy(paquete, &hn, sizeof(hn));
        strcpy(paquete + sizeof(hn), payload);

        int total = sizeof(hn) + strlen(payload);
        sendto(sock, paquete, total, 0, (const void*)&broker, sizeof(broker));

        printf("Publicado [%s] %s", topic, mensaje);

        // Esperar ACK
        char buffer[MAX_TAM];
        unsigned int blen = sizeof(broker);
        int n = recvfrom(sock, buffer, sizeof(buffer), 0, (void*)&broker, &blen);
        if (n > 0) {
            printf("ACK recibido del broker\n");
        }
    }

    close(sock);
    return 0;
}
