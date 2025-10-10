#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

// Definiciones de sockets
#define AF_INET 2
#define SOCK_DGRAM 2
#define INADDR_ANY ((unsigned long int) 0x00000000)
#define PUERTO 9001
#define MAX_SUSCRIPTORES 100
#define MAX_TAM 1024

// Definiciones de estructuras basicas
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

// Funciones auxiliares

unsigned short mi_htons(unsigned short x) {
    return ((x & 0x00FF) << 8) | ((x & 0xFF00) >> 8);
}

void mi_bzero(void *s, int n) {
    char *p = s;
    for (int i = 0; i < n; i++) {
        p[i] = 0;
    }
}

// Estructura para suscriptores UDP
typedef struct {
    struct sockaddr_in direccion;
    char partido[50];
} Suscriptor;

Suscriptor suscriptores[MAX_SUSCRIPTORES];
int num_suscriptores = 0;

// Verifica si es suscriptor
int es_suscriptor(char *buffer) {
    return strncmp(buffer, "TYPE:SUBSCRIBER", 16) == 0;
}

// Agrega un nuevo suscriptor a la lista
void agregar_suscriptor(struct sockaddr_in dir, char *partido) {
    if (num_suscriptores < MAX_SUSCRIPTORES) {
        suscriptores[num_suscriptores].direccion = dir;
        strcpy(suscriptores[num_suscriptores].partido, partido);
        num_suscriptores++;
        printf("Nuevo suscriptor para %s\n", partido);
    }
}

// Reenvía mensaje a todos los suscriptores que siguen ese partido
void reenviar_a_suscriptores(char *mensaje, char *partido, int socket_broker) {
    for (int i = 0; i < num_suscriptores; i++) {
        if (strcmp(suscriptores[i].partido, partido) == 0) {
            sendto(socket_broker, mensaje, strlen(mensaje), 0,
                   (struct sockaddr *)&suscriptores[i].direccion,
                   sizeof(struct sockaddr_in));
        }
    }
}

int main() {
    int socket_broker;
    struct sockaddr_in direccion, direccion_remitente;
    char buffer[MAX_TAM];

    // Crear socket UDP
    printf("Creando socket UDP...\n");
    socket_broker = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_broker < 0) {
        perror("socket");
        exit(1);
    }

    // Configurar direccion
    printf("Configurando direccion...\n");
    direccion.sin_family = AF_INET;
    direccion.sin_port = mi_htons(PUERTO);
    direccion.sin_addr.s_addr = INADDR_ANY;
    mi_bzero(&(direccion.sin_zero), 8);

    // Bind
    printf("Haciendo bind...\n");
    if (bind(socket_broker, (struct sockaddr *)&direccion, sizeof(direccion)) < 0) {
        perror("bind");
        exit(1);
    }

    printf("Broker UDP escuchando en el puerto %d...\n", PUERTO);

    while (1) {
        unsigned int tam_dir = sizeof(direccion_remitente);
        mi_bzero(buffer, MAX_TAM);

        int bytes = recvfrom(socket_broker, buffer, MAX_TAM, 0,
                             (struct sockaddr *)&direccion_remitente, &tam_dir);

        if (bytes > 0) {
            if (es_suscriptor(buffer)) {
                char *linea = strstr(buffer, "PARTIDO:");
                if (linea) {
                    char partido[50];
                    mi_bzero(partido, 50);
                    sscanf(linea, "PARTIDO:%s", partido);
                    agregar_suscriptor(direccion_remitente, partido);
                }
            } else {
                char partido[50];
                char mensaje[1000];
                mi_bzero(partido, 50);
                mi_bzero(mensaje, 1000);

                char *sep = strstr(buffer, "::");
                if (sep) {
                    strncpy(partido, buffer, sep - buffer);
                    partido[sep - buffer] = '\0';
                    strcpy(mensaje, sep + 2);
                    printf("Publicador: [%s] %s", partido, mensaje);
                    reenviar_a_suscriptores(mensaje, partido, socket_broker);
                }
            }
        }
    }

    close(socket_broker);
    return 0;
}