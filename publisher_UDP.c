#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

// -----------------------------
// DEFINICIONES MANUALES
// -----------------------------

#define AF_INET 2
#define SOCK_DGRAM 2

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
extern int sendto(int, const void *, unsigned long, int, const void *, unsigned int);
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
    struct sockaddr_in broker_addr;
    char buffer[1024];
    char partido[50];

    // 1. Crear socket UDP
    printf("Creando socket...\n");
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error al crear socket");
        exit(1);
    }

    // 2. Configurar dirección del broker
    printf("Configurando dirección del broker...\n");
    mi_bzero(&broker_addr, sizeof(broker_addr));
    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port = mi_htons(9001);
    broker_addr.sin_addr.s_addr = mi_htonl(0x7F000001); // 127.0.0.1

    // 3. Enviar mensaje de identificación
    printf("Enviando mensaje de configuración...\n");
    char inicio[] = "TYPE:PUBLISHER\n";
    sendto(sockfd, inicio, strlen(inicio), 0,
           (struct sockaddr *)&broker_addr, sizeof(broker_addr));

    // 4. Pedir nombre del partido
    printf("Ingresa el nombre del partido (ej. A_vs_B): ");
    scanf("%s", partido);
    getchar(); // limpiar '\n' después de scanf

    // 5. Bucle de mensajes
    printf("Escribe los mensajes (escribe SALIR para terminar):\n");

    while (1) {
        char mensaje[1024];
        printf("> ");
        fgets(mensaje, sizeof(mensaje), stdin);

        // Eliminar el '\n' al final
        mensaje[strcspn(mensaje, "\n")] = 0;

        if (strcmp(mensaje, "SALIR") == 0) break;

        char completo[1100];
        snprintf(completo, sizeof(completo), "%s::%s", partido, mensaje);

        sendto(sockfd, completo, strlen(completo), 0,
               (struct sockaddr *)&broker_addr, sizeof(broker_addr));
    }

    close(sockfd);
    return 0;
}
