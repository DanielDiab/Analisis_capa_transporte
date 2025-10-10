#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// -----------------------------
// DEFINICIONES MANUALES
// -----------------------------

#define AF_INET 2
#define SOCK_DGRAM 2
#define INADDR_ANY ((unsigned long int) 0x00000000)

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

// -----------------------------
// DECLARACIONES FUNCIONES
// -----------------------------

extern int socket(int, int, int);
extern int bind(int, const void *, unsigned int);
extern int sendto(int, const void *, unsigned long, int, const void *, unsigned int);
extern int recvfrom(int, void *, unsigned long, int, void *, unsigned int *);
extern int close(int);

// -----------------------------
// FUNCIONES AUXILIARES
// -----------------------------

unsigned short mi_htons(unsigned short x) {
    return ((x & 0x00FF) << 8) | ((x & 0xFF00) >> 8);
}

unsigned long mi_htonl(unsigned long x) {
    return ((x & 0x000000FF) << 24) |
           ((x & 0x0000FF00) << 8)  |
           ((x & 0x00FF0000) >> 8)  |
           ((x & 0xFF000000) >> 24);
}

void mi_bzero(void *s, int n) {
    char *p = s;
    for (int i = 0; i < n; i++) {
        p[i] = 0;
    }
}

// -----------------------------
// MAIN
// -----------------------------

int main() {
    int sockfd;
    struct sockaddr_in broker_addr, local_addr;
    char buffer[1024];
    char partido[50];

    // 1. Crear socket UDP
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        write(1, "Error al crear socket\n", 23);
        exit(1);
    }

    // 2. Configurar direccion local 
    mi_bzero(&local_addr, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = mi_htons(0);       // Puerto aleatorio
    local_addr.sin_addr.s_addr = INADDR_ANY; // Cualquier interfaz
    if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        write(1, "Error en bind local\n", 21);
        exit(1);
    }

    // 3. Configurar direccion del broker
    mi_bzero(&broker_addr, sizeof(broker_addr));
    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port = mi_htons(9001);
    broker_addr.sin_addr.s_addr = mi_htonl(0x7F000001); // 127.0.0.1

    // 4. Pedir partido
    printf("Ingresa el nombre del partido (ej. A_vs_B): ");
    scanf("%s", partido);

    // 5. Enviar mensaje de suscripción
    char mensaje[100];
    snprintf(mensaje, sizeof(mensaje), "TYPE:SUBSCRIBER\nPARTIDO:%s\n", partido);
    sendto(sockfd, mensaje, strlen(mensaje), 0,
           (struct sockaddr *)&broker_addr, sizeof(broker_addr));

    // 6. Esperar mensajes
    printf("Esperando actualizaciones del partido %s...\n", partido);

    while (1) {
        mi_bzero(buffer, sizeof(buffer));
        struct sockaddr_in remitente;
        unsigned int tam_remitente = sizeof(remitente);

        int bytes = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                             (struct sockaddr *)&remitente, &tam_remitente);

        if (bytes > 0) {
            printf("%s\n", buffer);
        }
    }

    close(sockfd);
    return 0;
}