#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// -----------------------------
// DEFINICIONES MANUALES
// -----------------------------

#define AF_INET 2
#define SOCK_STREAM 1

struct in_addr {
    unsigned long s_addr;
};

struct sockaddr_in {
    short sin_family;
    unsigned short sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

// -----------------------------
// DECLARACIONES FUNCIONES
// -----------------------------

extern int socket(int, int, int);
extern int connect(int, const void *, unsigned int);
extern int send(int, const void *, unsigned long, int);
extern int recv(int, void *, unsigned long, int);

unsigned short mi_htons(unsigned short x) {
    return ((x & 0x00FF) << 8) | ((x & 0xFF00) >> 8);
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
    struct sockaddr_in server_addr;
    char buffer[1024];
    char partido[50];

    // 1. Crear socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        write(1, "Error al crear socket\n", 23);
        exit(1);
    }

    // 2. Configurar dirección del broker
    mi_bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = mi_htons(9000);
    server_addr.sin_addr.s_addr = 0x0100007F; // 127.0.0.1

    // 3. Conectar
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        write(1, "Error al conectar\n", 18);
        exit(1);
    }

    // 4. Solicitar partido
    printf("Ingresa el nombre del partido (ej. A_vs_B): ");
    scanf("%s", partido);

    // 5. Enviar mensaje de suscripción
    char mensaje[100];
    snprintf(mensaje, sizeof(mensaje), "TYPE:SUBSCRIBER\nPARTIDO:%s\n", partido);
    send(sockfd, mensaje, strlen(mensaje), 0);

    // 6. Esperar mensajes del broker
    printf("Esperando actualizaciones del partido %s...\n", partido);
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(sockfd, buffer, sizeof(buffer), 0);
        if (bytes <= 0) {
            write(1, "Conexión cerrada por el broker\n", 32);
            break;
        } else {
            printf("%s\n", buffer);
        }
    }

    close(sockfd);
    return 0;
}
