#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <math.h>
#include "helpers.h"

void usage(char *file)
{
    fprintf(stderr, "Usage: %s <ID_CLIENT> <IP_SERVER> <PORT_SERVER>\n", file);
    exit(0);
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    if (argc != 4)
    {
        usage(argv[0]);
    }

    fd_set my_set;
    int sockfd, n, ret;
    struct sockaddr_in serv_addr;
    char buffer[BUFLEN];

    // Initializare socket client
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "Error client socket\n");

    // Setare campuri
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));
    ret = inet_aton(argv[2], &serv_addr.sin_addr);
    DIE(ret == 0, "Error inet_aton function\n");

    // Conectare la server
    ret = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(ret < 0, "Error connect\n");

    // Dezactivare algoritm Nagle
    int yes = 1;
    int result = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&yes, sizeof(int));
    DIE(result < 0, "Error disable Nagle algorithm\n");

    // Reutilizarea portului
    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1)
    {
        perror("setsocketopt\n");
        exit(1);
    }
    // se goleste multimea de descriptori de citire (read_fds)
    FD_ZERO(&my_set);

    // se adauga noii file descriptori in multimea read_fds
    FD_SET(STDIN_FILENO, &my_set);
    FD_SET(sockfd, &my_set);
    int fd_max = STDIN_FILENO > sockfd ? STDIN_FILENO : sockfd;

    // Verificare lungime id
    DIE(strlen(argv[1]) > 10, "Invalid id.(10 characters max)\n");

    //Se trimite id-ul clientului, serverului
    n = send(sockfd, argv[1], strlen(argv[1]), 0);
    DIE(n < 0, "Error send id\n");

    while (1)
    {
        fd_set tmp = my_set;

        int ret = select(fd_max + 1, &tmp, NULL, NULL, NULL);
        DIE(ret < 0, "Error select\n");

        // se citeste de la tastatura
        if (FD_ISSET(STDIN_FILENO, &tmp))
        {
            memset(buffer, 0, BUFLEN);
            fgets(buffer, BUFLEN, stdin);
            int flag = 0;

            // Comanda exit
            if (strncmp(buffer, "exit", 4) == 0)
            {
                break;
            }

            // Comanda subscribe
            if (strncmp(buffer, "subscribe", 9) == 0)
            {
                mesaj_TCP comanda;
                memset(&comanda, 0, sizeof(comanda));
                char word1[50], word2[50], word3[50];
                sscanf(buffer, "%s%s%s", word1, word2, word3);

                // Verificare lungime topic
                if (strlen(word2) > 50)
                {
                    perror("Error topic length.(50 characters max)\n");
                    continue;
                }

                // Verificare SF
                if (word3[0] == '0' || word3[0] == '1')
                {
                    // Se trimite comanda serverului
                    comanda.type = 's';
                    memcpy(comanda.topic, word2, sizeof(word2));
                    comanda.SF = (int)(word3[0] - 48);
                    n = send(sockfd, &comanda, sizeof(struct mesaj_TCP), 0);
                    DIE(n < 0, "Error send subscribe");
                    printf("Subscribed to topic.\n");
                    flag = 1;
                    continue;
                }
                else
                {
                    perror("Invalid SF");
                    continue;
                }
            }

            // Comanda unsubscribe
            if (strncmp(buffer, "unsubscribe", 11) == 0)
            {
                // Se trimite comanda serverului
                mesaj_TCP comanda;
                memset(&comanda, 0, sizeof(comanda));
                char word1[50], word2[50];
                sscanf(buffer, "%s%s", word1, word2);
                comanda.type = 'u';
                memcpy(comanda.topic, word2, sizeof(word2));
                comanda.SF = -1;
                n = send(sockfd, &comanda, sizeof(struct mesaj_TCP), 0);
                DIE(n < 0, "Error send unsubscribe\n");
                printf("Unsubscribed from topic.\n");
                flag = 1;
                continue;
            }

            // Comanda invalida
            if (flag == 0)
            {
                perror("Invalid command Usage subscribe <TOPIC> <SF>, unsubscribe <TOPIC>\n");
            }
        }

        // Clientul a primit un mesaj de la server
        if (FD_ISSET(sockfd, &tmp))
        {
            mesaj_UDP mesaj;
            memset(&mesaj, 0, sizeof(mesaj_UDP));
            int n = recv(sockfd, &mesaj, sizeof(mesaj_UDP), 0);
            if (n == 0)
                break;
            DIE(n < 0, "Error receive\n");
            // Mesaj de eroare
            // Se inchide conexiunea
            if (strncmp(mesaj.topic, "error", 5) == 0)
            {
                break;
            }
            else
            {
                // Interpretarea mesajelor
                // INT
                if (mesaj.type == 0)
                {
                    uint32_t int_num = ntohl(*((uint32_t *)(mesaj.payload + 1)));
                    if (mesaj.payload[0] == 1)
                    {

                        int_num *= -1;
                    }

                    printf("%s - INT - %d\n", mesaj.topic, int_num);
                }

                // SHORT_REAL
                if (mesaj.type == 1)
                {
                    double nr = ntohs(*(uint16_t *)mesaj.payload);
                    nr /= 100;
                    printf("%s - SHORT_REAL - %.2f\n", mesaj.topic, nr);
                }

                // FLOAT
                if (mesaj.type == 2)
                {

                    double nr = ntohl((*(uint32_t *)(mesaj.payload + 1)));
                    double putere = 1;
                    for (int i = 0; i < (uint8_t)mesaj.payload[5]; i++)
                    {
                        putere *= 10;
                    }
                    nr /= putere;
                    if (mesaj.payload[0] == 1)
                        nr = -nr;
                    printf("%s - FLOAT - %lf\n", mesaj.topic, nr);
                }

                // STRING
                if (mesaj.type == 3)
                {
                    printf("%s - STRING - %s\n", mesaj.topic, mesaj.payload);
                }
            }
        }
    }

    close(sockfd);

    return 0;
}
