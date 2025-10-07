#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Constantes y definiciones manuales
#define AF_INET 2
#define SOCK_DGRAM 2
#define INADDR_ANY ((unsigned long int) 0x00000000)
#define PUERTO 9001
#define MAX_SUSCRIPTORES 100
#define TAM_BUFFER 1024

// Estructura para direccion IP
struct in_addr {
    unsigned long s_addr;
};

// Estructura para direccion completa
struct sockaddr_in {
    short sin_family;
    unsigned short sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

// Estructura generica de socket
struct sockaddr {
    unsigned short sa_family;
    char sa_data[14];
};

// Declaracion de funciones de sistema
extern int socket(int, int, int);
extern int bind(int, const void *, unsigned int);
extern int recvfrom(int, void *, unsigned long, int, void *, unsigned int *);
extern int sendto(int, const void *, unsigned long, int, const void *, unsigned int);
extern int close(int);

// Funcion para convertir a orden de red (sin usar htons real)
unsigned short convertir_puerto(unsigned short puerto) {
    return ((puerto & 0x00FF) << 8) | ((puerto & 0xFF00) >> 8);
}

// Funcion para llenar en ceros una estructura
void limpiar_memoria(void *ptr, int cantidad) {
    char *p = ptr;
    for (int i = 0; i < cantidad; i++) {
        p[i] = 0;
    }
}

// Estructura para guardar suscriptores
typedef struct {
    struct sockaddr_in direccion;
    char nombre_partido[50];
} Suscriptor;

Suscriptor lista_suscriptores[MAX_SUSCRIPTORES];
int cantidad_suscriptores = 0;

// Funcion para guardar nuevo suscriptor
void registrar_suscriptor(struct sockaddr_in direccion, char *nombre_partido) {
    if (cantidad_suscriptores < MAX_SUSCRIPTORES) {
        lista_suscriptores[cantidad_suscriptores].direccion = direccion;
        strcpy(lista_suscriptores[cantidad_suscriptores].nombre_partido, nombre_partido);
        cantidad_suscriptores++;
        write(1, "Nuevo suscriptor registrado\n", 29);
    }
}

// Funcion para reenviar mensaje a los suscriptores del partido indicado
void reenviar_mensaje(char *partido, char *mensaje, int socket_broker) {
    for (int i = 0; i < cantidad_suscriptores; i++) {
        if (strcmp(lista_suscriptores[i].nombre_partido, partido) == 0) {
            sendto(socket_broker, mensaje, strlen(mensaje), 0,
                   (struct sockaddr *)&lista_suscriptores[i].direccion,
                   sizeof(struct sockaddr_in));
        }
    }
}

int main() {
    int socket_broker;
    struct sockaddr_in direccion_broker, direccion_remitente;
    char buffer[TAM_BUFFER];

    // Crear socket UDP
    socket_broker = socket(AF_INET, SOCK_DGRAM, 0);

    // Configurar direccion del broker
    direccion_broker.sin_family = AF_INET;
    direccion_broker.sin_port = convertir_puerto(PUERTO);
    direccion_broker.sin_addr.s_addr = INADDR_ANY;
    limpiar_memoria(&(direccion_broker.sin_zero), 8);

    // Asociar el socket a la direccion
    bind(socket_broker, (struct sockaddr *)&direccion_broker, sizeof(direccion_broker));
    write(1, "Broker UDP iniciado\n", 21);

    while (1) {
        unsigned int tam_direccion = sizeof(direccion_remitente);
        limpiar_memoria(buffer, TAM_BUFFER);

        // Recibir mensaje
        int bytes_recibidos = recvfrom(socket_broker, buffer, TAM_BUFFER, 0,
                                       (struct sockaddr *)&direccion_remitente, &tam_direccion);

        if (bytes_recibidos > 0) {
            // Verificar si es mensaje de suscripcion
            if (strncmp(buffer, "TYPE:SUBSCRIBER", 15) == 0) {
                char *linea = strstr(buffer, "PARTIDO:");
                if (linea != NULL) {
                    char nombre_partido[50];
                    sscanf(linea, "PARTIDO:%s", nombre_partido);
                    registrar_suscriptor(direccion_remitente, nombre_partido);
                }
            } else {
                // Es un mensaje de publicador
                char nombre_partido[50];
                char mensaje[1000];
                char *separador = strstr(buffer, "::");

                if (separador != NULL) {
                    strncpy(nombre_partido, buffer, separador - buffer);
                    nombre_partido[separador - buffer] = '\0';
                    strcpy(mensaje, separador + 2);
                    reenviar_mensaje(nombre_partido, mensaje, socket_broker);
                }
            }
        }
    }

    close(socket_broker);
    return 0;
}