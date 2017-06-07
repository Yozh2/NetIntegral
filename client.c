/* File:     client.c
 * Purpose:  Compute a definite integral using the Simpson formula
 * Input:    num_threads_req (number of threads)
 | Output:   Estimate of the integral at selected interval of FUNCTION
 |           using the Simpson formula
 * Compile:  Better to compile via makefile
 * Usage:    ./client <number of threads>
 * Note:
 |    1.  f(x) is hardcoded as FUNCTION (predefined)
 |    2.  Number of steps is NUM_STEPS (hardcoded)
 |    3.  TurboBoost avoidance is realized using sort of crutch
 |        by setting taskss for other cores unused in computation.
 |        These taskss are the same as the first one, only to get cores busy.
 * Author: Nikolai Gaiduchenko, MIPT 2017, 513 group
*/
/*==============================================================================
// TODO LIST
//==============================================================================
    1. Everything is done
*/
//==============================================================================
// INCLUDE SECTION
//==============================================================================
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "alerts.h"
// used macros: PRINT_ERR   - print error with code line and exit
//              TRY_TO      - if (... == -1) then ERROR it
//              PRINT       - printf macro
//              PRINT_LINE     - printf line macro

//==============================================================================
// DEFINE SECTION
//==============================================================================

// Define integral parameters
#define FUNCTION(x)    x*x*x/(x*x + x + 1/x -2)
#define NUM_STEPS      4790016000   // 12! * 10 rectangles

// Define errors
#define ERROR_INPUT             -1
#define ERROR_CONVERT_TO_INT    -2
#define ERROR_PTHREAD_CREATE    -3
#define ERROR_PTHREAD_JOIN      -4
#define ERROR_MEMORY_ALLOC      -5

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
// FUNCTION PROTOTYPES SECTION
//==============================================================================


    void waitBroadcast();
    // PURPOSE:     Receive broadcast from the server
    void waitTask();
    // PURPOSE:     Wait for task to calculate from the server
    void* integrateThread(void* data);
    // PURPOSE:     The same thread integration function
    //              as in the previous Lunev's problem

//==============================================================================
// GLOBAL VARIABLES
//==============================================================================

    // Network structures
    struct sockaddr_in baddr;
    struct sockaddr_in addr;
    struct net_msg msg;

    // Network variables
    int sock, bytes;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    // Calculation process variables and parameters
    long double distance, distance2;
    long double* data;
    pthread_t* threads;
    int num_threads_req;    // Number of threads required from the server

//==============================================================================
// MAIN CODE SECTION
//==============================================================================

int main(int argc, char** argv) {
    // ARGS CHECK
    if (argc != 2)
    {
        printf("USAGE: %s [NUMBER OF THREADS]", argv[0]);
        exit(ERROR_INPUT);
    }

    if (!sscanf(argv[1], "%d", &num_threads_req))
    {
        printf("ERROR: The number of clients should be positive integer");
        exit(ERROR_INPUT);
    }

    // Get interval via net from the server
    waitBroadcast();
    waitTask();

    // Calculating integral
    if (!(threads = malloc(msg.cores * sizeof(pthread_t))) ||
        !(data = malloc(msg.cores * sizeof(long double))))
        PRINT_ERR("Memory allocation failed");

    distance = msg.distance;
    distance2 = distance / 2;
    long double local_from = msg.local_from,
                local_dist = (msg.local_to - msg.local_from) / msg.cores;

    PRINT_LINE("Calculating integral at [%.6Lf:%.6Lf] with %u steps)", msg.local_from, msg.local_to, msg.steps);

    for (unsigned i = 0; i < msg.cores; ++i, local_from += local_dist) {
        data[i] = local_from;
        if (pthread_create(&threads[i], NULL, &integrateThread, &data[i]))
            PRINT_ERR("Cannot create thread");
    }

    long double S = 0, *r;
    for (unsigned i = 0; i < msg.cores; ++i) {
        if (pthread_join(threads[i], (void**) &r))
            PRINT_ERR("Cannot join thread");
        S += *r;
    }
    PRINT_LINE("Partial sum == %.6Lf", S);

    msg.distance = S;
    TRY_TO(bytes = write(sock, &msg, sizeof(struct net_msg)));
        if (bytes != sizeof(struct net_msg))
            PRINT_ERR("Cannot send net_msg");

    shutdown(sock, SHUT_RDWR);
    close(sock);
    free(threads);
    free(data);
    exit(EXIT_SUCCESS);
}


void waitBroadcast() {
    // Prepare for socket creation
    int bsock, recv_bytes;
    socklen_t baddr_len = sizeof(struct sockaddr_in);

    // Create a socket
    TRY_TO(bsock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP));
    baddr.sin_addr.s_addr = htonl(INADDR_ANY);
    baddr.sin_port = htons(BROADCAST_PORT);
    baddr.sin_family = AF_INET;

    // Try to bind a socket to the port if it is available
    int ld1 = 1;
    TRY_TO(setsockopt(bsock, SOL_SOCKET, SO_REUSEPORT, &ld1, sizeof(ld1)));
    TRY_TO(bind(bsock, (struct sockaddr*)&baddr, baddr_len));

    PRINT_LINE("Waiting for server...");

    // Receive net_msg from the server
    TRY_TO(recv_bytes = recvfrom(bsock, &msg, sizeof(struct net_msg),
        0, (struct sockaddr *)&baddr, &baddr_len));
    if (recv_bytes != sizeof(struct net_msg))
        PRINT_ERR("Cannot receive net_msg");
    PRINT_LINE("Received %d bytes from server port %d", recv_bytes, msg.tcp_port);
    baddr.sin_port = msg.tcp_port;

    // Close the broadcast socket gently
    shutdown(bsock, SHUT_RDWR);
    close(bsock);
}


void waitTask() {

    // Try to create a TCP socket
    TRY_TO(sock = socket(PF_INET, SOCK_STREAM, 0));
    memset(&addr, 0, addr_len);
    addr.sin_addr.s_addr = baddr.sin_addr.s_addr;
    addr.sin_port = baddr.sin_port;
    addr.sin_family = AF_INET;

    // Try to connect to the server via TCP
    int ld1 = 1;
    TRY_TO(setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &ld1, sizeof(ld1)));
    TRY_TO(connect(sock, (struct sockaddr*)&addr, addr_len));
    getsockname(sock, (struct sockaddr*)&baddr, &addr_len);
    PRINT_LINE("Connected to server via port %d", ntohs(baddr.sin_port));

    msg.cores = num_threads_req;
    TRY_TO(bytes = write(sock, &msg, sizeof(struct net_msg)));
    if (bytes != sizeof(struct net_msg))
        PRINT_ERR("Cannot send net_msg")
    else
        PRINT_LINE("Waiting for the task to calculate");

    TRY_TO(bytes = read(sock, &msg, sizeof(struct net_msg)));
    if (!bytes)
        PRINT_ERR("The connection is closed")
    else if (bytes != sizeof(struct net_msg))
        PRINT_ERR("Cannot receive net_msg");
}


void* integrateThread(void* data) {
    long double local_dist  = distance;
    long double local_dist2 = distance2;
    long double srt         = *(long double*)data;
    long double mid         = srt + local_dist2;
    long double end         = srt + local_dist;
    long double f_srt       = FUNCTION(srt);
    long double f_end;
    long double local_res   = 0;
    unsigned stepsPerThread = msg.steps / msg.cores;

    PRINT_LINE("Hello thread at [%.6Lf:%.6Lf] with %u steps", srt, srt + distance * stepsPerThread, stepsPerThread);
    for (unsigned i = 0; i < stepsPerThread; ++i) {
        f_end = FUNCTION(end);
        local_res += f_srt + 4 * FUNCTION(mid) + f_end;
        f_srt = f_end;          // next dx
        srt = end;              //
        mid += local_dist;      //
        end += local_dist;      //
    }

    *(long double*)data = local_res * distance / 6;
    pthread_exit(data);
}
