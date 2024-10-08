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

#define BUFLEN 1561

// Un client
struct client
{
    int port;
    int socket;
    int connected;
    string id;
    string ip;
};

// Forma in care este primit un pachet UDP
struct udp_packet
{
    int port;
    string ip;
    // Port + topic_name + type + content
    string mesaj;
    // Topic_name si topic.name (in struct topic)
    string topic_name;
    string type;
    string content;
};

// Ca sa retin fiecare topic si cine e abonat la el
struct topic
{
    string name;
    vector<client *> subscribers;
};

// Functii

// Creez pachet UDP
udp_packet new_udp_packet(char *buffer, string ip, int port)
{
    udp_packet new_packet;

    // Maxim 50 de octeti pentru topic, deci type e pe pozitia 50
    uint8_t type = buffer[50];

    uint8_t neg_or_poz;

    // Iau numele topicului
    char temporar_buffer[51];
    memset(temporar_buffer, 0, 51);
    memcpy(temporar_buffer, buffer, 50);
    string name(temporar_buffer);
    new_packet.topic_name = name;

    // Type: INT
    if (type == 0)
    {
        // Semnul numarului
        // Daca e negativ sau pozitiv
        neg_or_poz = buffer[51];

        // Obtinem payload
        uint32_t payload;
        memcpy(&payload, buffer + 52, sizeof(uint32_t));
        payload = ntohl(payload);

        string payload_string;
        payload_string = to_string(payload);

        if (neg_or_poz == 1)
        {
            new_packet.content = "-" + payload_string;
        }
        else
        {
            new_packet.content = payload_string;
        }
        new_packet.type = "INT";
    }

    // Type: SHORT_REAL
    if (type == 1)
    {
        // Obtinem payload
        uint16_t payload;
        memcpy(&payload, buffer + 51, sizeof(uint16_t));
        payload = ntohs(payload);
        char temporar_buffer[BUFLEN];
        snprintf(temporar_buffer, BUFLEN, "%.2f", (float)payload / 100);
        string payload_string(temporar_buffer);

        new_packet.content = payload_string;
        new_packet.type = "SHORT_REAL";
    }

    // Type: FLOAT
    if (type == 2)
    {
        // Obtin semnul
        uint8_t neg_or_poz = buffer[51];
        uint32_t payload_first;
        uint8_t payload_second;

        // Prima parte a payload
        memcpy(&payload_first, buffer + 52, sizeof(uint32_t));
        payload_first = ntohl(payload_first);

        // A doua parte a payload
        memcpy(&payload_second, buffer + 52 + sizeof(uint32_t), sizeof(uint8_t));

        // Obtinem payload
        stringstream payload_stream;
        float divizor = 1.0f;
        for (int i = 0; i < payload_second; ++i)
        {
            divizor = divizor * 10.0f;
        }
        payload_stream << fixed << setprecision((int)payload_second)
                       << (float)payload_first / divizor;

        if (neg_or_poz == 1)
        {
            new_packet.content = "-";
        }

        new_packet.content = new_packet.content + payload_stream.str();
        new_packet.type = "FLOAT";
    }

    // Type: STRING
    if (type == 3)
    {
        // Obtinem payload
        string payload(buffer + 51);

        new_packet.content = payload;
        new_packet.type = "STRING";
    }
    new_packet.ip = ip;
    new_packet.port = port;
    string port_string = to_string(port);

    // Construire mesaj
    new_packet.mesaj = new_packet.ip + ":" + port_string + " - " +
                       new_packet.topic_name + " - " + new_packet.type +
                       " - " + new_packet.content;

    return new_packet;
}

// 'Decodez' mesajul, de aici si denumirea functiei
// O impart in mai multi tokens
int decode_message(char *mesaj, vector<string> &tokens)
{
    stringstream str_stream(mesaj);
    string token;

    // Impart mesajele in functie de spatii
    while (getline(str_stream, token, ' '))
    {
        tokens.push_back(token);
    }

    return 0;
}

// Functie care ma ajuta sa caut topic-ul de care am nevoie
// in vectorul topicuri
int caut_topic(string nume_topic, vector<topic> topicuri)
{
    int i;
    for (i = 0; i < topicuri.size(); i++)
    {
        // Compar numele unui topic cu numele dat
        if (topicuri[i].name.compare(nume_topic) == 0)
        {
            return i;
        }
    }

    return -1;
}

// Cand un client vrea subscribe
void subscribe(client *client, vector<topic> &topicuri, string nume_topic)
{
    // Verificam daca exista deja
    int ok = caut_topic(nume_topic, topicuri);
    topic new_topic;

    if (ok == -1)
    {
        // Nu exista, deci e topic nou
        new_topic.name = nume_topic;
        topicuri.push_back(new_topic);
        ok = topicuri.size() - 1;
    }

    // Gasesc daca e abonat clientul la topic
    int okay, i;
    okay = -1;
    for (i = 0; i < topicuri[ok].subscribers.size(); i++)
    {
        if (!topicuri[ok].subscribers[i]->id.compare(client->id))
        {
            okay = i;
        }
    }
    // Clientul e deja abonat la acel topic
    if (okay != -1)
    {
        return;
    }

    // Daca nu e abonat, il adaugam la abonati
    topicuri[ok].subscribers.push_back(client);
}

// Cand un client vrea unsubscribe
void unsubscribe(client *client, vector<topic> &topicuri, string nume_topic)
{
    // Verific daca acel topic exista
    for (auto &t : topicuri)
    {
        if (t.name == nume_topic)
        {
            // Verific daca acel client e subscribed la topic
            auto c = find(t.subscribers.begin(), t.subscribers.end(), client);
            // Daca acel client se afla in lista de subscriberi pentru topic
            if (c != t.subscribers.end())
            {
                t.subscribers.erase(c);
            }
        }
    }
}

// Trimit mesajul care pleaca din udp, in tcp
void send_udp_packet(vector<topic> &topicuri, udp_packet packet)
{
    string nume = packet.topic_name;
    int i, ok, n;
    ok = 0;

    // Caut topicul dupa nume
    ok = caut_topic(nume, topicuri);

    // Fac unul nou daca nu exista
    if (ok == -1)
    {
        topic new_topic;
        new_topic.name = packet.topic_name;
        topicuri.push_back(new_topic);
        return;
    }

    // Pun clientii abonati intr-un vector, ca sa imi fie mai usor
    vector<client *> clienti_abonati = topicuri[ok].subscribers;

    // Adaug un newline la mesak si il
    // fac const char *
    string mes = packet.mesaj + "\n";
    const char *mesaj = mes.c_str();

    // Trimit mesajul catre abonati
    for (i = 0; i < clienti_abonati.size(); i++)
    {
        if (clienti_abonati[i]->connected == 1)
        {
            n = send(clienti_abonati[i]->socket, mesaj, strlen(mesaj), 0);
            if (n == -1)
            {
                cerr << "Eroare la trimiterea mesajului: " << strerror(errno) << endl;
            }
        }
    }
}

// Comenzi care pot sa vina de la tcp, cum ar fi
// subscribe sau unsubscribe sau exit
void command_tcp(char *mesaj, vector<topic> &topicuri,
                 map<int, client *> &clienti_conectati,
                 map<string, client *> &clienti_id, int socket)
{
    // Verific sa nu fie peste limita
    if (strlen(mesaj) >= BUFLEN)
    {
        cerr << "Mesaj peste limita buffer-ului" << endl;
        return;
    }

    // Gasesc clientul in functie de socket
    auto c = clienti_conectati.find(socket);

    // Daca nu il gasesc, nu are sens sa continui
    if (c == clienti_conectati.end())
    {
        return;
    }

    // Ce client avem
    client *client_tcp = c->second;

    // Decodez mesajul
    vector<string> comanda;
    // Daca nu avem ce procesa,nu merg mai departe
    if (decode_message(mesaj, comanda) != 0)
    {
        return;
    }

    char temporar_buffer[BUFLEN];
    if (comanda.size() >= 2)
    {   
        // Tai un \n
        comanda[1].resize(comanda[1].size() - 1);
        if (comanda[0] == "unsubscribe")
        {
            unsubscribe(client_tcp, topicuri, comanda[1]);

            // Trimit mesajul de unsubscribe
            size_t lungime = strlen("Unsubscribed from topic \n") + comanda[1].size() + 1;
            int n = snprintf(temporar_buffer, lungime, "Unsubscribed from topic %s\n", comanda[1].c_str());
            if (n < 0 || n >= BUFLEN)
            {
                cerr << "Eroare la mesajul de unsubscribe" << endl;
                return;
            }
            if (send(socket, temporar_buffer, n + 1, 0) == -1)
            {
                perror("send");
                return;
            }
        }
        else if (comanda[0] == "subscribe")
        {
            subscribe(client_tcp, topicuri, comanda[1]);

            // Trimit mesajul de subscribe
            size_t lungime = strlen("Subscribed to topic \n") + comanda[1].size() + 1;
            int n = snprintf(temporar_buffer, lungime, "Subscribed to topic %s\n", comanda[1].c_str());
            if (n < 0 || n >= BUFLEN)
            {
                cerr << "Eroare la mesajul de subscribe" << endl;
                return;
            }
            if (send(socket, temporar_buffer, n + 1, 0) == -1)
            {
                perror("send");
                return;
            }
        }
    }
}

// Inchid toti clientii
void close_all(char buffer[BUFLEN],
                       map<int, client *> &clienti_conectati, map<string, client *> &clienti_id)
{

    for (const auto &client_parse : clienti_id)
    {
        client *client_close = client_parse.second;

        if (client_close->connected == 1)
        {
            // Mesajul de exit
            int n = send(client_close->socket, buffer, strlen(buffer), 0);
            close(client_close->socket);
        }
        delete client_close;
    }
}

// Se conecteaza un client la server
void connecting(map<int, client *> &clienti_conectati,
                map<string, client *> &clienti_id,
                fd_set &read_fds, int port, int socket, string ip, string id)
{
    // Caut dupa id
    auto cautare_client = clienti_id.find(id);

    // Verific daca exista clientul deja
    if (cautare_client != clienti_id.end())
    {
        // Am gasit clientul
        client *client_found = cautare_client->second;

        // Acum verific daca e conectat
        if (client_found->connected == 1)
        {
            cout << "Client " << id << " "
                 << "already connected.";
            cout << "\n";

            // Verific sa nu fi primit alt socket
            if (client_found->socket != socket)
            {
                char temp_buffer[BUFLEN];
                memset(temp_buffer, 0, BUFLEN);
                memcpy(temp_buffer, "exit", strlen("exit"));
                // Inchid noul socket
                // Deja am unul
                int sending = send(socket, temp_buffer, strlen(temp_buffer), 0);
            }
            // E deja conectat, nu am ce sa ii fac
            // in afara de a inchide noul socket,
            // daca avem un socket nou
            return;
        }

        // Daca nu e conectat, o sa trebuiasca sa ii sterg
        // din lista vechoiul socket si sa il inlocuiesc
        // cu cel pe care se conecteaza
        int socket_old;
        socket_old = client_found->socket;

        // Update la informatii + update ca e conectat
        client_found->connected = 1;
        client_found->port = port;
        client_found->socket = socket;
        client_found->ip = ip;

        // Mesaj ca s-a conectat client nou
        cout << "New client " << client_found->id << " "
             << "connected from " << client_found->ip << ":" << client_found->port << ".";
        cout << "\n";

        // Update la clienti_id si clienti_conectati
        clienti_id.erase(id);
        clienti_id.insert(pair<string, client *>(id, client_found));

        clienti_conectati.erase(socket_old);
        clienti_conectati.insert(pair<int, client *>(client_found->socket, client_found));

        // Exista clientul, dar nu era conectat asa ca
        // pentru cazul asta am terminat
        return;
    }

    // Nu exista clientul, asa ca trebuie unul nou
    client *client_nou = new client;
    client_nou->connected = 1;
    client_nou->port = port;
    client_nou->socket = socket;
    client_nou->ip = ip;
    client_nou->id = id;

    // Il adaug la clienti_id si clienti_conectati
    clienti_id.insert(pair<string, client *>(id, client_nou));
    clienti_conectati.insert(pair<int, client *>(client_nou->socket, client_nou));

    // Mesaj ca s-a conectat client nou
    cout << "New client " << client_nou->id << " "
         << "connected from " << client_nou->ip << ":" << client_nou->port << ".";
    cout << "\n";
}

// Se deconecteaza un client de la server
void disconnecting(map<int, client *> &clienti_conectati,
                   map<string, client *> &clienti_id, int socket)
{

    // Caut dupa socket
    auto cautare_client = clienti_conectati.find(socket);

    // Daca gaseste clientul
    if (cautare_client != clienti_conectati.end())
    {
        client *client_found = cautare_client->second;

        // Schimbam la deconectat
        client_found->connected = 0;

        // Stergem din conectate
        // sau client_found->socket
        clienti_conectati.erase(socket);

        // Mesaj ca s-a deconectat un client
        cout << "Client " << client_found->id << " "
             << "disconnected.";
        cout << "\n";
    }
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    int udp_socket, tcp_socket, ret, port_nr, new_socket, n;
    char buffer[BUFLEN];
    vector<topic> topicuri;
    topic to_test;
    map<int, client *> clienti_conectati;
    map<string, client *> clienti_id;
    // Creare sockets
    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0)
    {
        perror("UDP socket creation");
        exit(EXIT_FAILURE);
    }
    tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket < 0)
    {
        perror("TCP socket creation");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in adr_server, adr_udp, adr_client;
    socklen_t len_udp, len_client;

    // File descriptors
    fd_set read_fds;
    fd_set temporar_fds;
    int fdmax;
    FD_ZERO(&read_fds);
    FD_ZERO(&temporar_fds);

    // Dezactivez Nagle
    int nagle = 1;
    ret = setsockopt(tcp_socket, IPPROTO_TCP, TCP_NODELAY, &nagle, sizeof(nagle));
    if (ret < 0)
    {
        perror("Eroare la dezactivarea Nagle");
        exit(EXIT_FAILURE);
    }

    // Numarul portului
    port_nr = atoi(argv[1]);

    // Stabilesc adr_server
    memset((char *)&adr_server, 0, sizeof(adr_server));
    adr_server.sin_addr.s_addr = INADDR_ANY;
    adr_server.sin_port = htons(port_nr);
    adr_server.sin_family = AF_INET;

    // Sectiune de bind pt tcp si udp
    // Bind tcp
    ret = bind(tcp_socket, (struct sockaddr *)&adr_server, sizeof(struct sockaddr));
    if (ret < 0)
    {
        perror("Eroare la tcp binding");
        exit(EXIT_FAILURE);
    }

    // Pentru reuse
    int port_reuse = 1;
    ret = setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &port_reuse, sizeof(port_reuse));
    if (ret < 0)
    {
        perror("Eroare la reuse");
        exit(EXIT_FAILURE);
    }

    // Bind udp
    ret = bind(udp_socket, (struct sockaddr *)&adr_server, sizeof(struct sockaddr));
    if (ret < 0)
    {
        perror("Eroare la udp binding");
        exit(EXIT_FAILURE);
    }

    // Ascult tcp
    // Nu vreau sa am un nr max de clienti, asa ca folosesc INT_MAX
    ret = listen(tcp_socket, INT_MAX);
    if (ret < 0)
    {
        perror("TCP listening");
        exit(EXIT_FAILURE);
    }

    // Adaugam la file descriptors
    FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(tcp_socket, &read_fds);
    FD_SET(udp_socket, &read_fds);
    fdmax = max(tcp_socket, udp_socket);

    int okay = 0;
    while (1)
    {
        temporar_fds = read_fds;

        ret = select(fdmax + 1, &temporar_fds, NULL, NULL, NULL);
        if (ret < 0)
        {
            perror("select");
            exit(EXIT_FAILURE);
        }
        if (FD_ISSET(STDIN_FILENO, &temporar_fds))
        {
            // Citire de la stdin
            memset(buffer, 0, BUFLEN);
            fgets(buffer, BUFLEN - 1, stdin);
            buffer[strlen(buffer) - 1] = '\0';

            // Verific daca am primit exit
            if (buffer == NULL || strlen(buffer) == 0 || strncmp(buffer, "exit", 4) == 0)
            {
                // Inchid toti clientii
                close_all(buffer, clienti_conectati, clienti_id);
                break;
            }

            continue;
        }
        int udp_or_tcp;
        for (udp_or_tcp = 0; udp_or_tcp <= fdmax; udp_or_tcp++)
        {

            if (FD_ISSET(udp_or_tcp, &temporar_fds))
            {
                if (udp_or_tcp != udp_socket)
                {
                    if (udp_or_tcp == tcp_socket)
                    {
                        // Conexiune pe tcp
                        len_client = sizeof(adr_client);
                        new_socket = accept(tcp_socket, (struct sockaddr *)&adr_client, &len_client);
                        if (new_socket < 0)
                        {
                            perror("accept");
                            exit(EXIT_FAILURE);
                        }

                        // Adaug socket-ul la file descriptori
                        FD_SET(new_socket, &read_fds);
                        if (new_socket > fdmax)
                        {
                            fdmax = new_socket;
                        }

                        // Iau id-ul clientului
                        memset(buffer, 0, BUFLEN);
                        n = recv(new_socket, buffer, sizeof(buffer), 0);
                        if (n < 0)
                        {
                            perror("recv");
                            exit(EXIT_FAILURE);
                        }

                        // Conectez clientul
                        connecting(clienti_conectati, clienti_id, read_fds, ntohs(adr_client.sin_port), new_socket, inet_ntoa(adr_client.sin_addr), buffer);
                    }
                    else
                    {
                        // Comenzi de la tcp
                        memset(buffer, 0, BUFLEN);
                        n = recv(udp_or_tcp, buffer, sizeof(buffer), 0);
                        if (n < 0)
                        {
                            perror("recv");
                            exit(EXIT_FAILURE);
                        }
                        if (n == 0)
                        {
                            // Daca vrea clientul sa se deconecteze
                            disconnecting(clienti_conectati, clienti_id, udp_or_tcp);
                            close(udp_or_tcp);
                            FD_CLR(udp_or_tcp, &read_fds);
                        }
                        else
                        {
                            // Daca primesc comenzile subscribe si unsubscribe
                            command_tcp(buffer, topicuri, clienti_conectati, clienti_id, udp_or_tcp);
                        }
                    }
                }
                else
                {
                    // Primesc mesaje de la udp
                    memset(buffer, 0, BUFLEN);
                    len_udp = sizeof(adr_udp);
                    ssize_t ret_udp;
                    ret_udp = recvfrom(udp_socket, buffer, BUFLEN, 0, (struct sockaddr *)&adr_udp, &len_udp);
                    if (ret_udp < 0)
                    {
                        perror("recvfrom");
                        exit(EXIT_FAILURE);
                    }
                    if (ret_udp == 0)
                    {
                        continue;
                    }
                    int port = ntohs(adr_udp.sin_port);
                    string ip(inet_ntoa(adr_udp.sin_addr));

                    // Pun informatia de la udp sub forma de packet
                    udp_packet new_pack = new_udp_packet(buffer, ip, port);
                    // Mesajul din packet
                    if (new_pack.mesaj.empty())
                    {
                        continue;
                    }
                    // O trimit la clientii tcp care sunt abonati
                    // la topicul mesajului
                    send_udp_packet(topicuri, new_pack);
                }
            }
        }
    }

    close(tcp_socket);
    return 0;
}