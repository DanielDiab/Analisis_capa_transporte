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
extern int recvfrom(int, void *, unsigned long, int, void *, unsigned int *);
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
    int socket_receptor;
    struct sockaddr_in direccion_broker, direccion_local;
    char nombre_partido[50];
    char buffer[TAM_BUFFER];

    // Crear socket UDP
    socket_receptor = socket(AF_INET, SOCK_DGRAM, 0);

    // Configurar direccion local para recibir mensajes
    direccion_local.sin_family = AF_INET;
    direccion_local.sin_port = convertir_puerto(0); // Puerto asignado automáticamente
    direccion_local.sin_addr.s_addr = 0; // Cualquier interfaz
    limpiar_memoria(&(direccion_local.sin_zero), 8);

    bind(socket_receptor, (struct sockaddr *)&direccion_local, sizeof(direccion_local));

    // Configurar direccion del broker
    direccion_broker.sin_family = AF_INET;
    direccion_broker.sin_port = convertir_puerto(PUERTO_BROKER);
    direccion_broker.sin_addr.s_addr = 0x7F000001; // 127.0.0.1
    limpiar_memoria(&(direccion_broker.sin_zero), 8);

    // Pedir el nombre del partido
    printf("Nombre del partido que quieres seguir (ej: A_vs_B): ");
    scanf("%s", nombre_partido);

    // Enviar mensaje de suscripcion al broker
    char mensaje[100];
    limpiar_memoria(mensaje, 100);
    snprintf(mensaje, 100, "TYPE:SUBSCRIBER\nPARTIDO:%s\n", nombre_partido);

    sendto(socket_receptor, mensaje, strlen(mensaje), 0,
           (struct sockaddr *)&direccion_broker, sizeof(direccion_broker));

    // Esperar mensajes del broker
    printf("Esperando mensajes del partido %s...\n", nombre_partido);

    while (1) {
        limpiar_memoria(buffer, TAM_BUFFER);
        int tam_direccion = sizeof(struct sockaddr_in);
        int bytes = recvfrom(socket_receptor, buffer, TAM_BUFFER, 0,
                             (struct sockaddr *)&direccion_broker, &tam_direccion);

        if (bytes > 0) {
            printf("Mensaje recibido: %s", buffer);
        }
    }

    close(socket_receptor);
    return 0;
}