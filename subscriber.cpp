#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <map>
#include <bits/stdc++.h>

using namespace std;

#define BUFLEN 2000

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    int ret, socket_fd ,n, i;
    struct sockaddr_in adr_server;
    char buffer[BUFLEN];
    // File descriptori
    fd_set read_fds;
    fd_set temporar_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&temporar_fds);

    // Socket-ul clientului
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("Eroare la crearea socket-ului");
        exit(EXIT_FAILURE);
    }

    // Dezactivez nagle
    int nagle = 1;
    ret = setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &nagle, sizeof(nagle));
    if (ret == -1) {
        perror("Eroare la dezactivarea Nagle");
        exit(EXIT_FAILURE);
    }

    adr_server.sin_family = AF_INET;
    adr_server.sin_port = htons(atoi(argv[3]));
    adr_server.sin_addr.s_addr = inet_addr(argv[2]);

    // Conectez la server
    ret = connect(socket_fd, (struct sockaddr *) &adr_server, sizeof(adr_server));
    if (ret == -1) {
        perror("Eroare la conectarea la server");
        exit(EXIT_FAILURE);
    }

    // Trimit id-ul clientului la server
    n = send(socket_fd, argv[1], strlen(argv[1]), 0);
    if (n == -1) {
        perror("Eroarea la trimitere id la server");
        exit(EXIT_FAILURE);
    }

    FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(socket_fd, &read_fds);

    while (1) {
        temporar_fds = read_fds;
        ret = select(socket_fd + 1, &temporar_fds, NULL, NULL, NULL);
        
        // Citesc din stdin
        if (FD_ISSET(STDIN_FILENO, &temporar_fds)) {
            memset(buffer, 0, BUFLEN);
            fgets(buffer, BUFLEN - 1, stdin);
            n = send(socket_fd, buffer, strlen(buffer), 0);
            if (n == -1) {
                perror("Eroare la trimiterea datelor la server");
                exit(EXIT_FAILURE);
            }
            // Verifica daca am dat exit
            if (strncmp(buffer, "exit", 4) == 0 || n == 0) {
                break;
            }
        }

        // Primesc date de la server
        if (FD_ISSET(socket_fd, &temporar_fds)) {
            memset(buffer, 0, BUFLEN);
            n = recv(socket_fd, buffer, BUFLEN, 0);
            if (n == -1) {
                perror("Eroare la primirea datelor de la server");
                exit(EXIT_FAILURE);
            }
            // Verifica daca primeste exit de la server
            if (strncmp(buffer, "exit", 4) == 0 || n == 0) {
                break;
            }
            // Afisez mesajul primit de la server
            cout << buffer;
        }
    }
    close(socket_fd);
    return 0;
}

