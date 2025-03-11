#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <time.h>
#include <ifaddrs.h>

// funkcja do sprawdzania czy gracz podal liczbe czy napis
int is_number(const char *str)
{
    char *endptr;
    strtol(str, &endptr, 10);
    return *endptr == '\0';
}

// funkcja do pobierania lokalnego IP
char *get_local_ip()
{
    struct ifaddrs *interfaces = NULL;
    struct ifaddrs *ifa = NULL;
    char *local_ip = NULL;

    if (getifaddrs(&interfaces) == -1)
    {
        perror("getifaddrs");
        return NULL;
    }

    for (ifa = interfaces; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr->sa_family == AF_INET)
        {
            struct sockaddr_in *sockaddr = (struct sockaddr_in *)ifa->ifa_addr;
            local_ip = inet_ntoa(sockaddr->sin_addr);
            if (strcmp(local_ip, "127.0.0.1") != 0)
                break;
        }
    }

    freeifaddrs(interfaces);
    return local_ip;
}

struct my_msg
{
    char nick1[32];
    char nick2[32];
    int number;
    int turn;
    int points;
    char text[255];
    int start;
};

int main(int argc, char *argv[])
{
    struct my_msg msg;
    memset(&msg, 0, sizeof(msg));

    if (argc < 3)
    {
        fprintf(stderr, "Za malo argumentow\n");
        exit(0);
    }
    if (argc > 4)
    {
        fprintf(stderr, "Za duzo argumentow\n");
        exit(0);
    }

    char *host = argv[1];
    char *port = argv[2];

    int sockfd;
    int rv;
    char t[32];
    struct addrinfo server, *server_result, *server_rp;
    struct addrinfo client, *client_result, *client_rp;

    memset(&server, 0, sizeof(struct addrinfo));
    memset(&client, 0, sizeof(struct addrinfo));

    server.ai_family = AF_INET;
    server.ai_socktype = SOCK_DGRAM;
    server.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, port, &server, &server_result)) != 0)
    {
        perror("Blad getaddrinfo");
        exit(0);
    }
    // tworzenie i bindowanie socketu
    for (server_rp = server_result; server_rp != NULL; server_rp = server_rp->ai_next)
    {
        if ((sockfd = socket(server_rp->ai_family, server_rp->ai_socktype, server_rp->ai_protocol)) == -1)
        {
            perror("listener: socket");
            continue;
        }
        if (bind(sockfd, server_rp->ai_addr, server_rp->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("listener: bind");
            continue;
        }
        break;
    }

    if (server_rp == NULL)
    {
        fprintf(stderr, "Nie udalo sie zbindowac\n");
        exit(0);
    }

    // przygotowanie drugiego gracza do podlaczenia
    client.ai_family = AF_INET;
    client.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(host, port, &client, &client_result) != 0)
    {
        perror("getaddrinfo client");
        exit(0);
    }
    for (client_rp = client_result; client_rp != NULL; client_rp = client_rp->ai_next)
    {
        struct sockaddr_in *addr_in = (struct sockaddr_in *)client_rp->ai_addr;
        char ip_str[16];
        inet_ntop(AF_INET, &(addr_in->sin_addr), ip_str, 16);
        if (strcmp(ip_str, "127.0.0.1") != 0)
            break;
    }

    if (client_rp == NULL)
    {
        fprintf(stderr, "Nie znaleziono poprawnego adresu klienta.\n");
        exit(1);
    }

    struct sockaddr_in *client_addr = (struct sockaddr_in *)client_result->ai_addr;

    if (argc == 4)
    {
        strncpy(msg.nick1, argv[3], sizeof(msg.nick1) - 1);
        msg.nick1[sizeof(msg.nick1) - 1] = '\0';
    }
    else
    {
        char *server_ip = get_local_ip();
        strncpy(msg.nick1, server_ip, sizeof(msg.nick1) - 1);
        msg.nick1[sizeof(msg.nick1) - 1] = '\0';
    }

    printf("Gra w 50, wersja B.\n");
    printf("Rozpoczynam gre z %s. Napisz 'koniec' by zakonczyc\n", inet_ntoa(client_addr->sin_addr));

    msg.turn = 1;
    msg.start = 1;
    msg.points = 0;

    struct sockaddr_storage client_storage;
    socklen_t client_addr_len = sizeof(client_storage);

    if (sendto(sockfd, &msg, sizeof(msg), 0, client_result->ai_addr, client_result->ai_addrlen) == -1)
    {
        perror("Blad sendto");
        close(sockfd);
        exit(0);
    }

    while (1)
    {
        if (msg.turn == 1)
        {
            // obsluga gracza oczekujacego
            if (recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&client_storage, &client_addr_len) == -1)
            {
                perror("Blad recvfrom");
                close(sockfd);
                exit(0);
            }
            if (msg.points == 50)
            {
                printf("%s podal wartosc 50.\n", msg.nick1);
                printf("Przegrana!\n");
                close(sockfd);
                exit(0);
            }
            if (strcmp(msg.text, "koniec") == 0)
            {
                printf("%s zakonczyl gre.\n", msg.nick2);
                close(sockfd);
                exit(0);
            }

            if (is_number(msg.text) == 0)
            {
                printf("%s napisal: %s\n", msg.nick2, msg.text);
                msg.turn = 1;
                continue;
            }
            if (msg.points != 0)
            {
                printf("%s podal wartosc %d, podaj kolejna wartosc\n", msg.nick1, msg.points);
            }
            msg.turn = 0;
        }

        if (msg.turn == 0)
        {
            // poczatek rozgrywki
            if (msg.start == 1)
            {
                if (argc == 4)
                {
                    strncpy(msg.nick2, argv[3], sizeof(msg.nick2) - 1);
                    msg.nick2[sizeof(msg.nick2) - 1] = '\0';
                }
                else
                {
                    char *server_ip = get_local_ip();
                    strncpy(msg.nick2, server_ip, sizeof(msg.nick2) - 1);
                    msg.nick2[sizeof(msg.nick2) - 1] = '\0';
                }
                srand(time(NULL));
                msg.points = (rand() % 10) + 1;
                printf("%s dolaczyl do gry.\n", inet_ntoa(client_addr->sin_addr));
                printf("Losowa wartosc poczatkowa: %d\n", msg.points);
                msg.start = 0;
            }

            while (2)
            {
                // obsluga gracza wykonujacego ruch
                fgets(msg.text, sizeof(msg.text), stdin);
                size_t len = strlen(msg.text);
                if (len > 0 && msg.text[len - 1] == '\n')
                {
                    msg.text[len - 1] = '\0';
                }
                if (strcmp(msg.text, "koniec") == 0)
                {
                    printf("Konczymy, dziekuje za gre.\n");
                    if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&client_storage, client_addr_len) == -1)
                    {
                        perror("Blad sendto");
                    }
                    close(sockfd);
                    exit(0);
                }
                if (is_number(msg.text) == 0)
                {
                    if (sendto(sockfd, &msg, sizeof(msg), 0, client_result->ai_addr, client_result->ai_addrlen) == -1)
                    {
                        perror("Błąd sendto");
                        close(sockfd);
                        exit(0);
                    }
                    continue;
                }

                int num = atoi(msg.text);
                if (num <= msg.points || num > 50 || num > msg.points + 10)
                {
                    printf("Takiej wartosci nie mozesz wybrac.\n");
                    continue;
                }
                else
                {
                    strcpy(t, msg.nick1);
                    strcpy(msg.nick1, msg.nick2);
                    strcpy(msg.nick2, t);
                    msg.points = num;
                    msg.turn = 1;
                    break;
                }
            }

            if (msg.points == 50)
            {
                printf("Wygrana! Osiagnieto 50.\n");
                strcpy(msg.text, "koniec");
                sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&client_storage, client_addr_len);
                close(sockfd);
                exit(0);
            }

            if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&client_storage, client_addr_len) == -1)
            {
                perror("Blad sendto");
                close(sockfd);
                exit(0);
            }
        }
    }

    freeaddrinfo(server_result);
    freeaddrinfo(client_result);
    close(sockfd);
    return 0;
}
