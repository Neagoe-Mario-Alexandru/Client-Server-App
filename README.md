# Client-Server-App
TCP and UDP Client-Server App for Message Management

PCOM - TEMA 2
Neagoe Mario-Alexandru - 324CC

Am doua fisiere sursa, server si subscriber
Continut arhiva: server.cpp + subscriber.cpp + Makefile


Nu am implementat wildcard, in rest am implementat tot ce s-a cerut.


Structuri folosite:

Client:
Contine datele legate de client, cum ar fi:
- ip
- port
-connected, ca sa retin daca e conectat sau nu
 1 = conectat, 0 = neconectat
- socket

Udp_packet:
Contine datele primite de la clientul udp, care trebuie impachetate
si trimise clientului tcp.
- port
- ip
- numele topicului
- type: INT, SHORT_REAL, FLOAT sau STRING
- content
- mesaj, ex: 127.0.0.1:48835 - a_negative_int - INT - -10
Mesajul este format din datele de la udp si este trimis la tcp

Topic:
Contine numele topicului si un vector in care retin ce clienti sunt
abonati la acel topic.
- name
- subscribers

De asemenea, mai folosesc un vector unde retin
topicurile si doua map, unul in care retin
clientii conectati, si unul in care retin clientii dupa id.
- vector<topic> topicuri
- map<int, client *> clienti_conectati (int este socket, client este clientul)
- map<string, client *> clienti_id (string este id, client este clientul)


Subscriber:
Declar variabilele necesare si dezactive Nagle. Declar un socket tcp, il
conectez la server si trimit id-ul clientului la server.
Apoi intru intr-o bucla permanenta, in care am 2 variante:

1) Citesc ceva din stdin/tastatura, aici pot sa primesc
si exit si astfel ies din while(1). Ce primesc (in afara de exit),
trimit ca si comanda la server (ex: "subscribe ana")

2) Primesc date de la server, din nou si aici pot sa primesc exit.
Altfel, primesc mesajele care sunt trimise de clientul udp server-ului
si apoi trimise de server catre client tcp sau mesajele de subscribe si
unsubscribe in urma comenzilor primite.


Server:
Declar variabilele necesare si dezactive Nagle. Declar socketii pentru udp si
tcp, dau reuse la port, dau bind la socketi, iar apoi dau  listen pe tcp.
Apoi intru, la fel ca la subscriber, intr-o  bucla permanenta. Aleg valoarea
de input mai mare cu select, de la file descriptor. In bucla, am 4 variante:

1) Prima data verific daca primesc ceva de la stdin. Daca primesc exit, inchid
toti clientii prin functia 'close_all' si inchid serverul. 
Daca nu, intru intr-un for unde gestionez ce primesc de la clientii udp si tcp
2) Conexiune la tcp. Primesc o cerere noua de conectare a unui client tcp.
Adaug acel socket la file descriptori,iau id-ul clientului si apelez functia
'connecting', care se ocupa de conectarea clientului, inclusiv sa verifice daca
acel client deja exista. Daca exista si e conectat, nu ii face nimic. Daca
exista dar nu e conectat, il conecteaza. Daca nu exista inainte, il conecteaza
si il retine.

3) Primesc comenzi de la tcp. Prima data verific daca vrea sa se deconecteze,
caz in care apelez functia 'disconnecting', care gestioneaza acest lucru. Daca
este alta comanda, precum 'subscribe' sau 'unsubscribe', apelez functia
'command_tcp' care gestioneaza acest caz.

4) Primesc mesaje de la udp. Preiau informatia intr-uun buffer si apelez
functia 'new_udp_packet', care preia informatia si creaza un packet cu ea.
Dupa ce am creat packetul, verific daca are un mesaj. Acel packet poate sa nu
aiba un mesaj, ceva de transmis, si atunci nu are sens sa il trimit
clientului/clientilor tcp care sunt destinatari. Daca are un mesaj, atunci
apelez functia 'send_udp_packet', care ia pachetul si il trimite
clientului/clientilor tcp care sunt abonati la topicul cu care are legatura
pachetul.
