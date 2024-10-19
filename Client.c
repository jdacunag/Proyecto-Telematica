#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define SERVER_PORT 67
#define BUFFER_SIZE 1024
#define RENEWAL_INTERVAL 30 

void send_dhcp_discover(int sock) {
    const char *dhcp_discover = "DHCPDISCOVER";
    send(sock, dhcp_discover, strlen(dhcp_discover), 0);
    printf("Solicitud DHCPDISCOVER enviada.\n");
}

void send_dhcp_request(int sock) {
    const char *dhcp_request = "DHCPREQUEST (Renovación)";
    send(sock, dhcp_request, strlen(dhcp_request), 0);
    printf("Solicitud DHCPREQUEST (Renovación) enviada.\n");
}

int receive_dhcp_offer(int sock, int *lease_time) {
    char buffer[BUFFER_SIZE];
    ssize_t len;

    len = recv(sock, buffer, sizeof(buffer), 0);
    if (len > 0) {
        printf("Información recibida del servidor:\n%s", buffer);
        
        sscanf(buffer, "IP:%*s\nMASK:%*s\nGATEWAY:%*s\nDNS:%*s\nLEASE:%d\n", lease_time);
        return 1;  
    } else {
        perror("Error al recibir el DHCPOFFER");
        return 0; 
    }
}

int main() {
    int sock;
    struct sockaddr_in server_addr;
    int lease_time = 0; 
    time_t lease_expiry;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Error creando socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en connect");
        exit(EXIT_FAILURE);
    }

    send_dhcp_discover(sock);
    if (receive_dhcp_offer(sock, &lease_time)) {
        lease_expiry = time(NULL) + lease_time;  

        
        while (1) {
            sleep(RENEWAL_INTERVAL);
            send_dhcp_request(sock);
            
            if (time(NULL) >= lease_expiry) {
                printf("El tiempo de concesión ha expirado. Saliendo...\n");
                break;  
            }
        }
    }

    close(sock);
    return 0;
}
