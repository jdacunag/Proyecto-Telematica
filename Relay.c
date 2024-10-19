#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define RELAY_PORT 1067          
#define SERVER_PORT 67            
#define CLIENT_PORT 68            
#define SERVER_IP "127.0.0.1"     
#define RELAY_IP "127.0.0.1"      

typedef struct {
    int message_type;
    char client_mac[18];
    char requested_ip[16];
    uint8_t options[312];
    uint32_t giaddr;            
} dhcp_message;

int main() {
    int sockfd;
    struct sockaddr_in client_addr, server_addr, relay_addr;
    dhcp_message msg;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("No se pudo crear el socket");
        exit(EXIT_FAILURE);
    }

    
    memset(&relay_addr, 0, sizeof(relay_addr));
    relay_addr.sin_family = AF_INET;
    relay_addr.sin_addr.s_addr = inet_addr(RELAY_IP); 
    relay_addr.sin_port = htons(RELAY_PORT);

    
    if (bind(sockfd, (struct sockaddr*)&relay_addr, sizeof(relay_addr)) < 0) {
        perror("No se pudo enlazar el socket del relay");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    printf("Relay Agent DHCP iniciado y escuchando en el puerto %d...\n", RELAY_PORT);

    while (1) {
        memset(&msg, 0, sizeof(msg));

        
        if (recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&client_addr, &addr_len) < 0) {
            perror("Error al recibir mensaje del cliente");
            continue;
        }

        printf("Recibida solicitud DHCP del cliente %s\n", msg.client_mac);

        
        inet_pton(AF_INET, RELAY_IP, &msg.giaddr); 

        printf("Reenviando solicitud al servidor DHCP %s:%d\n", SERVER_IP, SERVER_PORT);
        if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&server_addr, addr_len) < 0) {
            perror("Error al reenviar mensaje al servidor");
            continue;
        }

        
        if (recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&server_addr, &addr_len) < 0) {
            perror("Error al recibir respuesta del servidor");
            continue;
        }

        printf("Respuesta recibida del servidor DHCP, reenviando al cliente...\n");

        
        if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&client_addr, addr_len) < 0) {
            perror("Error al reenviar respuesta al cliente");
            continue;
        }
    }

    close(sockfd);
    return 0;
}
