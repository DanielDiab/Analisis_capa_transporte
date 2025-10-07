#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// ----------------------------
// Constantes y definiciones
// ----------------------------

#define AF_INET 2
#define SOCK_DGRAM 2
#define PUERTO_BROKER 9001
#define TAM_BUFFER 1024

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

// ----------------------------
// Funciones del sistema
// ----------------------------

extern int socket(int, int, int);
extern int sendto(int, const void *, unsigned long, int, const void *, unsigned int);
extern int connect(int, const void *, unsigned int);
extern int close(int);

// ----------------------------
// Funciones auxiliares
// ----------------------------

unsigned short convertir_puerto(unsigned short puerto) {
    return ((puerto & 0x00FF) << 8) | ((puerto & 0xFF00) >> 8);
}

void limpiar_memoria(void *ptr, int cantidad) {
    char *p = ptr;
    for (int i = 0; i < cantidad; i++) {
        p[i] = 0;
    }
}

// ----------------------------
// Programa principal
// ----------------------------

int main() {
    int socket_emisor;
    struct sockaddr_in direccion_broker;
    char nombre_partido[50];
    char mensaje[1000];
    char buffer[TAM_BUFFER];

    // Crear socket UDP
    socket_emisor = socket(AF_INET, SOCK_DGRAM, 0);

    // Configurar direccion del broker
    direccion_broker.sin_family = AF_INET;
    direccion_broker.sin_port = convertir_puerto(PUERTO_BROKER);
    direccion_broker.sin_addr.s_addr = 0x7F000001; // 127.0.0.1
    limpiar_memoria(&(direccion_broker.sin_zero), 8);

    // Leer nombre del partido
    printf("Nombre del partido que vas a cubrir (ej: A_vs_B): ");
    scanf("%s", nombre_partido);

    // Ciclo de envio de mensajes
    while (1) {
        printf("Mensaje para enviar (ej: Gol al minuto 32, 'salir' para terminar): ");
        limpiar_memoria(mensaje, 1000);
        fgets(mensaje, 1000, stdin); // usar fgets para leer incluso con espacios

        // Verificar si el usuario quiere salir
        if (strncmp(mensaje, "salir", 5) == 0) {
            break;
        }

        // Formar el mensaje en formato: PARTIDO::MENSAJE
        limpiar_memoria(buffer, TAM_BUFFER);
        snprintf(buffer, TAM_BUFFER, "%s::%s", nombre_partido, mensaje);

        // Enviar al broker
        sendto(socket_emisor, buffer, strlen(buffer), 0,
               (struct sockaddr *)&direccion_broker,
               sizeof(direccion_broker));
    }

    close(socket_emisor);
    return 0;
}