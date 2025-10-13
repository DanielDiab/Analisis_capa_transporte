// Uso: ./broker_quic_hibrido <PUERTO>
// Ej:  ./broker_quic_hibrido 9002

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

/* =========================
   Helpers
   ========================= */
static unsigned short mi_htons(unsigned short x) {
    return (unsigned short)(((x & 0x00FFu) << 8) | ((x & 0xFF00u) >> 8));
}
static unsigned int mi_htonl(unsigned int x) {
    return ((x & 0x000000FFu) << 24) |
           ((x & 0x0000FF00u) << 8)  |
           ((x & 0x00FF0000u) >> 8)  |
           ((x & 0xFF000000u) >> 24);
}
static void mi_bzero(void *s, int n) {
    char *p = (char*)s;
    for (int i = 0; i < n; i++) p[i] = 0;
}

/* =========================
   Definiciones de sockets
   ========================= */
#define AF_INET          2
#define SOCK_DGRAM       2
#define INADDR_ANY       ((unsigned long int)0x00000000)

#ifndef PUERTO_DEF
#define PUERTO_DEF       9002
#endif

#define MAX_SUSCRIPTORES 100
#define MAX_TAM          1024
#define MAX_TOPIC        64

/* =========================
   Estructuras "básicas"
   ========================= */
struct in_addr {
    unsigned long s_addr;
};

struct sockaddr_in {
    short            sin_family;
    unsigned short   sin_port;
    struct in_addr   sin_addr;
    char             sin_zero[8];
};

struct sockaddr {
    unsigned short   sa_family;
    char             sa_data[14];
};

/* =========================
   Syscalls de red (extern)
   ========================= */
extern int socket(int, int, int);
extern int bind(int, const void *, unsigned int);
extern int sendto(int, const void *, unsigned long, int, const void *, unsigned int);
extern int recvfrom(int, void *, unsigned long, int, void *, unsigned int *);
extern int close(int);

/* =========================
   Protocolo "fiable" simple
   ========================= */
#define FLAG_SYN   0x01
#define FLAG_ACK   0x02
#define FLAG_FIN   0x04
#define FLAG_DATA  0x08
#define FLAG_SUB   0x10  /* informativo */

#pragma pack(push, 1)
typedef struct {
    /* header  */
    unsigned int   conn_id;    /* id logico por suscriptor */
    unsigned short stream_id;  /* usamos 0 */
    unsigned int   seq;        /* número de secuencia broker→sub */
    unsigned int   ack;        /* ack acumulativo sub→broker */
    unsigned short wnd;        /* ventana (paquetes) */
    unsigned char  flags;      /* FLAGS_* */
    unsigned char  rsv;        /* 0 */
} hdr_t;
#pragma pack(pop)

typedef struct {
    struct sockaddr_in direccion;
    char partido[MAX_TOPIC];
    int  activo;

    /* Estado fiable TX hacia el suscriptor */
    unsigned int conn_id;
    unsigned int tx_next;      /* proximo seq a enviar */
    unsigned int tx_acked;     /* ultimo ack confirmado */
    unsigned short peer_wnd;   /* ventana anunciada por el peer (paquetes) */
} Suscriptor;

/* =========================
   Variables globales
   ========================= */
static Suscriptor suscriptores[MAX_SUSCRIPTORES];
static int num_suscriptores = 0;

/* =========================
   Funciones auxiliares
   ========================= */
static int es_suscriptor(const char *buf) {
    return strncmp(buf, "TYPE:SUBSCRIBER", 15) == 0;
}

static int extraer_topic(const char *buf, char *out, int outlen) {
    const char *t = strstr(buf, "T:");
    if (!t) return 0;
    t += 3;
    const char *end = strpbrk(t, "\n");
    int n = end ? (int)(end - t) : (int)strlen(t);
    if (n >= outlen) n = outlen - 1;
    memcpy(out, t, n); out[n] = '\0';
    return 1;
}

static int extraer_mensaje(const char *buf, char *out, int outlen) {
    const char *m = strstr(buf, "M:");
    if (!m) return 0;
    m += 3; while (*m == ' ') m++;
    int n = (int)strcspn(m, "\r\n");
    if (n >= outlen) n = outlen - 1;
    memcpy(out, m, n); out[n] = '\0';
    return 1;
}

/* =========================
   Gestión de suscriptores
   ========================= */
static int iguales_dir(const struct sockaddr_in *a, const struct sockaddr_in *b) {
    return a->sin_family == b->sin_family &&
           a->sin_port   == b->sin_port   &&
           a->sin_addr.s_addr == b->sin_addr.s_addr;
}

static int buscar_suscriptor(const struct sockaddr_in *dir) {
    for (int i = 0; i < num_suscriptores; i++) {
        if (suscriptores[i].activo && iguales_dir(&suscriptores[i].direccion, dir))
            return i;
    }
    return -1;
}

static int agregar_suscriptor(const struct sockaddr_in *dir, const char *partido) {
    if (num_suscriptores >= MAX_SUSCRIPTORES) return -1;
    Suscriptor *s = &suscriptores[num_suscriptores];
    mi_bzero(s, sizeof(*s));
    s->direccion = *dir;
    s->activo = 1;
    s->conn_id = (unsigned int)(getpid() ^ (unsigned int)num_suscriptores * 1103515245u);
    s->tx_next = 0;
    s->tx_acked = (unsigned int)(-1);
    s->peer_wnd = 16; /* ventana por defecto */
    strncpy(s->partido, partido, MAX_TOPIC-1);
    num_suscriptores++;
    printf("Nuevo suscriptor para %s\n", s->partido);
    return num_suscriptores - 1;
}

/* =========================
   Funciones protocolo fiable
   ========================= */
static void hdr_hton(hdr_t *out, const hdr_t *in) {
    out->conn_id  = mi_htonl(in->conn_id);
    out->stream_id= mi_htons(in->stream_id);
    out->seq      = mi_htonl(in->seq);
    out->ack      = mi_htonl(in->ack);
    out->wnd      = mi_htons(in->wnd);
    out->flags    = in->flags;
    out->rsv      = 0;
}

static void hdr_ntoh(hdr_t *out, const hdr_t *in) {
    out->conn_id  = mi_htonl(in->conn_id);
    out->stream_id= mi_htons(in->stream_id);
    out->seq      = mi_htonl(in->seq);
    out->ack      = mi_htonl(in->ack);
    out->wnd      = mi_htons(in->wnd);
    out->flags    = in->flags;
    out->rsv      = 0;
}

static void enviar_data(int sock, Suscriptor *s, const char *payload, int plen) {
    char paquete[sizeof(hdr_t) + MAX_TAM];
    hdr_t h; mi_bzero(&h, sizeof(h));
    h.conn_id   = s->conn_id;
    h.stream_id = 0;
    h.seq       = s->tx_next;
    h.ack       = 0;                  /* sin piggyback */
    h.wnd       = 0;                  /* broker no anuncia aquí */
    h.flags     = FLAG_DATA;

    hdr_t hn; hdr_hton(&hn, &h);
    memcpy(paquete, &hn, sizeof(hn));
    memcpy(paquete + sizeof(hn), payload, (unsigned long)plen);

    int total = (int)sizeof(hn) + plen;
    sendto(sock, paquete, (unsigned long)total, 0,
           (const void*)&s->direccion, (unsigned int)sizeof(s->direccion));

    s->tx_next++;
}

static void reenviar_a_suscriptores(int sock, const char *mensaje, const char *partido) {
    /* payload de app (igual que UDP/TCP) */
    char payload[MAX_TAM];
    memset(payload, 0, MAX_TAM);
    int plen = snprintf(payload, sizeof(payload), "[%s] %s", partido, mensaje);
    if (plen <= 0) return;

    for (int i = 0; i < num_suscriptores; i++) {
        if (!suscriptores[i].activo) continue;
        if (strncmp(suscriptores[i].partido, partido, MAX_TOPIC) != 0) continue;
        /* envio por UDP */
        enviar_data(sock, &suscriptores[i], payload, plen);
    }
}

/* =========================
   MAIN
   ========================= */
int main(int argc, char **argv) {
    int puerto = (argc >= 2) ? atoi(argv[1]) : PUERTO_DEF;

    /* socket UDP */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in dir; mi_bzero(&dir, sizeof(dir));
    dir.sin_family = AF_INET;
    dir.sin_port   = mi_htons((unsigned short)puerto);
    dir.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (const void*)&dir, (unsigned int)sizeof(dir)) < 0) {
        perror("bind"); close(sock); return 1;
    }

    printf("Broker QUIC-híbrido (UDP confiable) escuchando en 0.0.0.0:%d\n", puerto);

    char buffer[MAX_TAM];
    while (1) {
        struct sockaddr_in cli; unsigned int clen = sizeof(cli);
        int n = recvfrom(sock, buffer, (unsigned long)(sizeof(buffer)-1), 0,
                         (void*)&cli, &clen);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("recvfrom");
            break;
        }
        buffer[n] = '\0';

        /* ¿Trae cabecera fiable? */
        if (n >= (int)sizeof(hdr_t)) {
            hdr_t hn; memcpy(&hn, buffer, sizeof(hn));
            hdr_t h;  hdr_ntoh(&h, &hn);

            /* Si flags DATA: payload inicia tras el header */
            if (h.flags & FLAG_DATA) {
                const char *pl = buffer + sizeof(hdr_t);
                int pln = n - (int)sizeof(hdr_t);

                /* Puede ser SUB o PUB en capa de app */
                char tmp[MAX_TAM];
                int cpy = (pln < (int)sizeof(tmp)-1) ? pln : (int)sizeof(tmp)-1;
                memcpy(tmp, pl, (unsigned long)cpy);
		tmp[cpy] = '\0';

		printf("\n");
                int idx = buscar_suscriptor(&cli);
                if (idx < 0 && es_suscriptor(tmp)) {
                    char top[MAX_TOPIC];
                    if (extraer_topic(tmp, top, sizeof(top))) {
                        agregar_suscriptor(&cli, top);
                        /* responder ACK simple (opcional) */
                        hdr_t rh; mi_bzero(&rh, sizeof(rh));
                        rh.conn_id = suscriptores[num_suscriptores-1].conn_id;
                        rh.flags   = FLAG_ACK;
                        rh.ack     = 0;
                        hdr_t rhn; hdr_hton(&rhn, &rh);
                        sendto(sock, &rhn, (unsigned long)sizeof(rhn), 0,
                               (const void*)&cli, (unsigned int)sizeof(cli));
                    }
                } else {
                    /* Publicación fiable desde un publisher “QUIC-like” */
		    printf("Mensaje recibido desde publicador\n");
                    char top[MAX_TOPIC], msg[MAX_TAM];
		    memset(top, 0, MAX_TOPIC);
		    memset(msg, 0, MAX_TOPIC);
                    if (extraer_topic(tmp, top, sizeof(top)) && extraer_mensaje(tmp, msg, sizeof(msg))) {
                        printf("Publicador (fiable): [%s] %s\n", top, msg);
                        reenviar_a_suscriptores(sock, msg, top);
                    }
                    /* Enviar ACK acumulativo hacia el emisor */
                    hdr_t ackh; mi_bzero(&ackh, sizeof(ackh));
                    ackh.conn_id = (idx >= 0) ? suscriptores[idx].conn_id : 0;
                    ackh.flags   = FLAG_ACK;
                    ackh.ack     = h.seq;
                    hdr_t ackhn; hdr_hton(&ackhn, &ackh);
                    sendto(sock, &ackhn, (unsigned long)sizeof(ackhn), 0,
                           (const void*)&cli, (unsigned int)sizeof(cli));
                }
                continue;
            }
        }

        /* Si no trae header → modo legacy UDP (tus binarios actuales) */
        {
            /* Suscriptor legacy: "TYPE:SUBSCRIBER|TOPIC:..." */
            if (es_suscriptor(buffer)) {
                char top[MAX_TOPIC];
                if (extraer_topic(buffer, top, sizeof(top))) {
                    if (buscar_suscriptor(&cli) < 0) agregar_suscriptor(&cli, top);
                }
            } else {
                /* Publicador legacy: "TOPIC:...| MESSAGE:..." */
                char top[MAX_TOPIC], msg[MAX_TAM];
		memset(top, 0, MAX_TOPIC);
		memset(msg, 0, MAX_TOPIC);
                if (extraer_topic(buffer, top, sizeof(top)) && extraer_mensaje(buffer, msg, sizeof(msg))) {
                    printf("Publicador (legacy UDP): [%s] %s\n", top, msg);
                    reenviar_a_suscriptores(sock, msg, top);
                }
            }
        }
    }

    close(sock);
    return 0;
}
