/* File:     server.c
 * Purpose:  Compute a definite integral using the Simpson formula
 |           by distributing integration intervals to clients to calculate them
 * Input:    clients_max (number of clients)
 | Output:   Estimate of the integral from From to To of FUNCTION
 |           using the Simpson formula
 * Compile:  Better to compile via makefile
 * Usage:    ./server <number of clients>
 * Note:
 |    1.  f(x) is hardcoded as FUNCTION (predefined)
 |    2.  Integrate bounds are from From to To (hardcoded)
 |    3.  Number of steps is NUM_STEPS (hardcoded)
 |    4.  TurboBoost avoidance is realized using sort of crutch
 |        by setting taskss for other cores unused in computation.
 |        These taskss are the same as the first one, only to get cores busy.
 * Author: Nikolai Gaiduchenko, MIPT 2017, 513 group
*/
/*==============================================================================
// ToDO LIST
//==============================================================================
    1. Everything is done
*/
//==============================================================================
// INCLUDE SECTION
//==============================================================================

#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "alerts.h"
    // used macros: ERROR   - print error with code line and exit
    //              TRY_TO - if (... == -1) then ERROR it
    //              PRINT   - printf macro
    //              PRINT_LINE - printf line macro

//==============================================================================
// DEFINE SECTION
//==============================================================================

// Define integral parameters
#define FUNCTION(x)    x*x*x/(x*x + x + 1/x - 2)
#define NUM_STEPS      2000000000

// Define errors
#define ERROR_INPUT             -1

// Debug definitions
#define DEBUG
#define LINE "===========================================\n"

// Define network parameters
#define BROADCAST_PORT  31123

//==============================================================================
// CLIENT TASK STRUCTURE SECTION
//==============================================================================

struct net_msg {
    int tcp_port;
    unsigned cores, steps;
    long double local_from,
                local_to,
                distance;
};

//==============================================================================
// FUNCTION PROToTYPES SECTION
//==============================================================================

    void* broadcast(void* args);
    // PURPOSE:     UDP broadcast
    void wait_for_clients();
    // PURPOSE:     obvious
    void enable_keepalive(int sock);
    // PURPOSE:     check if the connection is alive

//==============================================================================
// GLOBAL VARIABLES
//==============================================================================

    int bsock;          // broadcast socket
    int clients_max;    // maximum number of clients
    int boss;         // boss
    int *client;        // clients array
    int *cores;         // cores array
    int bytes;          // temp bytes variable
    int cores_all;    // total number of cores

    // Network variables
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    struct net_msg broadcast_msg;

    // integration interval
    long double From = 0;
    long double To = 1;

//==============================================================================
// MAIN CODE SECTION
//==============================================================================

int main(int argc, char** argv)
{
    // ARGS CHECK
    if (argc != 2)
        PRINT_ERR("USAGE: %s [NUMBER OF CLIENTS]", argv[0]);

    if (!sscanf(argv[1], "%d", &clients_max))
        PRINT_ERR("ERROR: The number of clients should be positive integer");

    if (!(client = calloc(clients_max, sizeof(int))) ||
        !(cores  = calloc(clients_max, sizeof(int))))
        PRINT_ERR("Memory allocation");

    // SETUP TCP PORT
    TRY_TO(boss = socket(PF_INET, SOCK_STREAM, 0));
    memset(&addr, 0, addr_len);
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(0);
    addr.sin_family = AF_INET;

    // Bind to port and listen to clients
    TRY_TO(bind(boss, (struct sockaddr*)&addr, addr_len));
    TRY_TO(listen(boss, clients_max));
    TRY_TO(getsockname(boss, (struct sockaddr*)&addr, &addr_len));

    // Setup broadcast message
    memset(&broadcast_msg, 0, sizeof(struct net_msg));
    broadcast_msg.tcp_port = addr.sin_port;

    // Start the broadcast via another thread
    pthread_t bthread;
    if (pthread_create(&bthread, NULL, &broadcast, &broadcast_msg))
        PRINT_ERR("Cannot create the broadcasting thread");

    // Wait for the clients
    wait_for_clients();

    // Finish the broadcast
    pthread_cancel(bthread);
    pthread_join(bthread, NULL);
    shutdown(bsock, SHUT_RDWR);
    close(bsock);

    // Distribute intervals
    struct net_msg request;
    long double distance = (To - From) / NUM_STEPS;
    unsigned cores_count = 0;
    for (int i = 0; i < clients_max; ++i) {
        request = (struct net_msg) {
            0,
            cores[i],
            NUM_STEPS / cores_all * cores[i],
            From + (To - From) / cores_all * cores_count,
            From + (To - From) / cores_all * (cores_count + cores[i]),
            distance
        };
        // Send the Task created to the client
        TRY_TO(bytes = write(client[i], &request, sizeof(struct net_msg)));
        if (bytes != sizeof(struct net_msg))
            PRINT_ERR("Cannot send a task to the client %d", i);
        cores_count += cores[i];
    }

    PRINT_LINE("Tasks sent");

    // Collecting partial sums and keeping alive the connection
    // until the end of all calculations
    for (int i = 0; i < clients_max; ++i) {
        enable_keepalive(client[i]);
    }

    // Filling fd_set
    fd_set fds;
    int maxsd = 0;
    long double S = 0;

    FD_ZERO(&fds);
    for (int i = 0; i < clients_max; ++i) {
            if (client[i]) {
                FD_SET(client[i], &fds);
                if (client[i] > maxsd)
                    maxsd = client[i];
            }
            else {
                PRINT_ERR("A weird shit just have happened nearby: some file descriptors are NULL");
            }
    }

    // getting calculations results
    int ready = 0;
    while (ready < clients_max) {
        if ((select(maxsd+1, &fds, NULL, NULL, NULL) == -1) && (errno != EINTR)) {
            PRINT_ERRV("SELECT");
        }
        else {
            PRINT_LINE("SELECT happened");
        }

        for (int i = 0; i < clients_max; ++i) {
            if (FD_ISSET(client[i], &fds)) {
                PRINT_LINE("Event @%d", client[i]);
                TRY_TO(bytes = read(client[i], &request, sizeof(struct net_msg)));
                if (!bytes)
                    PRINT_ERR("The connection is closed")
                else if (bytes != sizeof(struct net_msg))
                    PRINT_ERR("Cannot receive net_msg");
                S += request.distance;
                ++ready;

                PRINT_LINE("Client_%d := %Lf", i, request.distance);
            }
        }
    }

    // printing result
    printf (LINE);
    printf ("The integral of f(x) == %.6Lf\n", S);
    printf (LINE);

    // exiting
    free(client);
    free(cores);
    return 0;
}

//==============================================================================
// SUPPORT FUNCTIONS SECTION
//==============================================================================

void* broadcast(void* args) {
    // Prepare to create a UDP socket
    struct sockaddr_in addr;
    int sent_bytes;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    // Creating a UDP socket
    TRY_TO(bsock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP));
    memset(&addr, 0, addr_len);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(0);
    addr.sin_family = AF_INET;

    // Bind the socket to UDP broadcast reserved port zero
    TRY_TO(bind(bsock, (struct sockaddr*)&addr, addr_len));
    int ld1 = 1;
    TRY_TO(setsockopt(bsock, SOL_SOCKET, SO_BROADCAST, &ld1, sizeof(ld1)));
    addr.sin_addr.s_addr = htonl(-1);
    addr.sin_port = htons(BROADCAST_PORT);

    // Broadcast in cycle every second
    while (1) {
        TRY_TO(sent_bytes = sendto(bsock, args, sizeof(struct net_msg), 0, (struct sockaddr*)&addr, addr_len));
        if (sent_bytes != sizeof(struct net_msg))
            PRINT_ERR("Cannot send net_msg")
        PRINT_LINE("Sent %d bytes", sent_bytes);
        sleep(1);
    }
    args = NULL;
}


void wait_for_clients() {
    // prepare variables
    fd_set fds;
    int clients_count = 0, cores_info = 0, maxsd;
    struct net_msg recv_msg;
    cores_all = 0;

    PRINT_LINE("Wait for clients on port %d", broadcast_msg.tcp_port);

    // Wait for clients in cycle until all of them are done
    while (cores_info < clients_max) {
        FD_ZERO(&fds);
        FD_SET(boss, &fds);
        maxsd = boss;
        for (int i = 0; i < clients_max; ++i) {
            if (client[i]) {
                FD_SET(client[i], &fds);
                if (client[i] > maxsd)
                    maxsd = client[i];
            }
        }

        // If all clients are done, close the connection gently
        if (clients_count == clients_max) {
            FD_CLR(boss, &fds);
            shutdown(boss, SHUT_RDWR);
            close(boss);
        }

        // Select the wait event is happened
        if ((select(maxsd+1, &fds, NULL, NULL, NULL) == -1) && (errno != EINTR))
            PRINT_ERRV("Select failed");

        if (FD_ISSET(boss, &fds)) {
            PRINT_LINE("Event @%d", boss);
            int new;
            TRY_TO(new = accept(boss, (struct sockaddr*)&addr, &addr_len));
            PRINT_LINE("New connection %d [%s:%d]", clients_count, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
            for (int i = 0; i < clients_max; ++i)
                if (!client[i]) {
                    client[i] = new;
                    break;
                }
            clients_count += 1;
        }

        // read net_msg from clients in cycle
        for (int i = 0; i < clients_max; ++i)
            if (FD_ISSET(client[i], &fds)) {
                PRINT_LINE("Event @%d", client[i]);
                TRY_TO(bytes = read(client[i], &recv_msg, sizeof(struct net_msg)));
                if (!bytes)
                    PRINT_ERR("The connection is closed")
                else if (bytes != sizeof(struct net_msg))
                    PRINT_ERR("Cannot read net_msg")
                else {
                    cores[i] = recv_msg.cores;
                    cores_info += 1;
                    cores_all += recv_msg.cores;
                    PRINT_LINE("Client %d: %d core%s", i, cores[i], ((cores[i] > 1) ? "s" : ""));
                }
            }
    }

    PRINT_LINE("Prepared to integrate");
}

void enable_keepalive(int sock) {
    int yes = 1;
    TRY_TO(setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(int)));

    int idle = 1;

    // Compiled on Linux or MacOS
    #ifdef LINUX
        TRY_TO(setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(int)));
    #else
        TRY_TO(setsockopt(sock, IPPROTO_TCP, TCP_KEEPALIVE, &idle, sizeof(int)));
    #endif

    int interval = 1;
    TRY_TO(setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(int)));

    int max_packet = 1;
    TRY_TO(setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &max_packet, sizeof(int)));
}
