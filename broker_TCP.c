#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

// Definiciones de sockets
#define AF_INET 2
#define SOCK_STREAM 1
#define INADDR_ANY ((unsigned long int) 0x00000000)
#define PUERTO 9000
#define MAX_CLIENTES 100
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

// Definiciones de funciones de red (a mano)
extern int socket(int, int, int);
extern int bind(int, const void *, unsigned int);
extern int listen(int, int);
extern int accept(int, void *, unsigned int *);
extern int connect(int, const void *, unsigned int);
extern int send(int, const void *, unsigned long, int);
extern int recv(int, void *, unsigned long, int);
extern int setsockopt(int, int, int, const void *, unsigned int);
// Implementaciones manuales de htons y bzero

unsigned short mi_htons(unsigned short x) {
    return ((x & 0x00FF) << 8) | ((x & 0xFF00) >> 8);
}

void mi_bzero(void *s, int n) {
    char *p = s;
    for (int i = 0; i < n; i++) {
        p[i] = 0;
    }
}

typedef struct {
    int socket;
    char partido[50];
} Suscriptor;

Suscriptor suscriptores[MAX_CLIENTES];
int num_suscriptores = 0;

int es_suscriptor(char *buffer) {
    return strncmp(buffer, "TYPE:SUBSCRIBER", 16) == 0;
}

int es_publicador(char *buffer) {
    return strncmp(buffer, "TYPE:PUBLISHER", 15) == 0;
}

void agregar_suscriptor(int nuevo_socket, char *partido) {
    if (num_suscriptores < MAX_CLIENTES) {
        suscriptores[num_suscriptores].socket = nuevo_socket;
        strcpy(suscriptores[num_suscriptores].partido, partido);
        num_suscriptores++;
        printf("Nuevo suscriptor para %s\n", partido);
    }
}

void reenviar_a_suscriptores(char *mensaje, char *partido) {
    for (int i = 0; i < num_suscriptores; i++) {
        if (strcmp(suscriptores[i].partido, partido) == 0) {
            send(suscriptores[i].socket, mensaje, strlen(mensaje), 0);
        }
    }
}

int main() {
    int servidor, nuevo_socket, max_sd, actividad, valread, sd;
    int clientes[MAX_CLIENTES] = {0};
    char buffer[MAX_TAM];
    fd_set readfds;

    struct sockaddr_in direccion;

    // Crear socket TCP
    printf("Creando socket...\n");
    servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (servidor < 0) { perror("socket"); exit(1);}
    printf("Configurando dirección...\n");
    direccion.sin_family = AF_INET;
    direccion.sin_addr.s_addr = INADDR_ANY;
    direccion.sin_port = mi_htons(PUERTO);
    mi_bzero(&(direccion.sin_zero), 8);

    printf("Haciendo bind...\n");
    if (bind(servidor, (struct sockaddr *)&direccion, sizeof(direccion)) < 0) {
        perror("bind");
	exit(1);
    }
    listen(servidor, 5);
    printf("Broker esperando conexiones en el puerto %d...\n", PUERTO);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(servidor, &readfds);
        max_sd = servidor;

        for (int i = 0; i < MAX_CLIENTES; i++) {
            sd = clientes[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }
	printf("Esperando conexiones...\n");
        actividad = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(servidor, &readfds)) {
            unsigned int addr_len = sizeof(struct sockaddr_in);
            nuevo_socket = accept(servidor, NULL, &addr_len);

            memset(buffer, 0, MAX_TAM);
            read(nuevo_socket, buffer, MAX_TAM);

            if (es_suscriptor(buffer)) {
                char *linea = strstr(buffer, "PARTIDO:");
                if (linea) {
                    char partido[50];
                    sscanf(linea, "PARTIDO:%s", partido);
                    agregar_suscriptor(nuevo_socket, partido);
                    clientes[nuevo_socket] = nuevo_socket;
                }
            } else if (es_publicador(buffer)) {
                clientes[nuevo_socket] = nuevo_socket;
                printf("Publicador conectado\n");
            } else {
                close(nuevo_socket);
            }
        }

        for (int i = 0; i < MAX_CLIENTES; i++) {
            sd = clientes[i];
            if (FD_ISSET(sd, &readfds)) {
                memset(buffer, 0, MAX_TAM);
                valread = read(sd, buffer, MAX_TAM);
                if (valread <= 0) {
                    close(sd);
                    clientes[i] = 0;
                } else {
                    char partido[50], mensaje[1000];
                    char *sep = strstr(buffer, "::");
                    if (sep) {
                        strncpy(partido, buffer, sep - buffer);
                        partido[sep - buffer] = '\0';
                        strcpy(mensaje, sep + 2);
                        printf("Publicador: [%s] %s", partido, mensaje);
                        reenviar_a_suscriptores(mensaje, partido);
                    }
                }
            }
        }
    }

    return 0;
}
