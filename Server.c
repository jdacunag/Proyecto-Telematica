#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define SERVER_PORT 67
#define MAX_CLIENTS 10
#define IP_POOL_START 100
#define IP_POOL_END 110
#define LEASE_TIME 60  // 1 minuto de tiempo de concesión

typedef struct {
    char ip[16];
    time_t lease_start;
    int is_assigned;
} IP;

IP ip_pool[IP_POOL_END - IP_POOL_START + 1];
pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;

void log_lease(const char *ip, const char *action) {
    FILE *file = fopen("leases.txt", "a");
    if (file == NULL) {
        perror("Error abriendo el archivo");
        return;
    }

    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);

    fprintf(file, "[%s] IP: %s - %s\n", time_str, ip, action);
    fclose(file);
    printf("Registro de IP: %s - %s\n", ip, action);
}

void initialize_ip_pool() {
    for (int i = 0; i <= IP_POOL_END - IP_POOL_START; i++) {
        snprintf(ip_pool[i].ip, sizeof(ip_pool[i].ip), "192.168.0.%d", IP_POOL_START + i);
        ip_pool[i].is_assigned = 0;
    }
}

char* assign_ip() {
    pthread_mutex_lock(&pool_mutex);
    for (int i = 0; i <= IP_POOL_END - IP_POOL_START; i++) {
        if (!ip_pool[i].is_assigned) {
            ip_pool[i].is_assigned = 1;
            ip_pool[i].lease_start = time(NULL);
            char* assigned_ip = ip_pool[i].ip;
            pthread_mutex_unlock(&pool_mutex);
            log_lease(assigned_ip, "Asignada");
            return assigned_ip;
        }
    }
    pthread_mutex_unlock(&pool_mutex);
    return NULL;
}

void release_ip(char* ip) {
    pthread_mutex_lock(&pool_mutex);
    for (int i = 0; i <= IP_POOL_END - IP_POOL_START; i++) {
        if (strcmp(ip_pool[i].ip, ip) == 0) {
            ip_pool[i].is_assigned = 0;
            log_lease(ip_pool[i].ip, "Liberada");
            printf("IP %s liberada.\n", ip);
            break;
        }
    }
    pthread_mutex_unlock(&pool_mutex);
}

void build_dhcp_options(char response[], const char* ip, const char* subnet_mask, const char* gateway, const char* dns, int lease_time) {
    sprintf(response, "IP:%s\nMASK:%s\nGATEWAY:%s\nDNS:%s\nLEASE:%d\n", ip, subnet_mask, gateway, dns, lease_time);
}

void *handle_client(void *client_socket) {
    int sock = *(int *)client_socket;
    char buffer[1024];
    ssize_t len;
    char *assigned_ip = NULL;
    time_t lease_expiry;

    len = recv(sock, buffer, sizeof(buffer), 0);
    if (len > 0) {
        printf("Solicitud DHCPDISCOVER recibida.\n");

        assigned_ip = assign_ip();
        if (assigned_ip != NULL) {
            printf("Asignando IP: %s\n", assigned_ip);

            // Crear respuesta con las opciones DHCP
            char dhcp_offer[128];
            build_dhcp_options(dhcp_offer, assigned_ip, "255.255.255.0", "192.168.0.1", "8.8.8.8", LEASE_TIME);
            send(sock, dhcp_offer, strlen(dhcp_offer), 0);
            printf("DHCPOFFER enviado con IP: %s\n", assigned_ip);
            lease_expiry = time(NULL) + LEASE_TIME;
        } else {
            const char *no_ip_available = "No hay más IPs disponibles.";
            send(sock, no_ip_available, strlen(no_ip_available), 0);
            printf("No hay más IPs disponibles.\n");
        }

        while (time(NULL) < lease_expiry) {
            len = recv(sock, buffer, sizeof(buffer), 0);
            if (len > 0 && strstr(buffer, "DHCPREQUEST (Renovación)") != NULL) {
                printf("Renovando IP: %s\n", assigned_ip);
                lease_expiry = time(NULL) + LEASE_TIME;
                pthread_mutex_lock(&pool_mutex);
                for (int i = 0; i <= IP_POOL_END - IP_POOL_START; i++) {
                    if (strcmp(ip_pool[i].ip, assigned_ip) == 0) {
                        ip_pool[i].lease_start = time(NULL);
                        break;
                    }
                }
                pthread_mutex_unlock(&pool_mutex);
                printf("IP %s renovada.\n", assigned_ip);
            }
            sleep(5);  // Comprobación cada 5 segundos
        }

        printf("Liberando IP %s (tiempo de concesión terminado).\n", assigned_ip);
        release_ip(assigned_ip);
    }

    close(sock);
    return NULL;
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    pthread_t threads[MAX_CLIENTS];
    int thread_count = 0;

    initialize_ip_pool();

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creando socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Error en listen");
        exit(EXIT_FAILURE);
    }

    printf("Servidor DHCP escuchando en el puerto %d...\n", SERVER_PORT);

    while ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len)) >= 0) {
        printf("Cliente conectado.\n");
        pthread_create(&threads[thread_count++], NULL, handle_client, &client_socket);
    }

    close(server_socket);
    return 0;
}