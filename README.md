# NetIntegral

A client-server based program to perform distributed multi-core integral calculations via local network.
The program will compute a definite integral using the Simpson formula by distributing integration intervals to clients to calculate them using multi-core programming (pthreads).

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. See deployment for notes on how to deploy the project on a live system.

### Prerequisites

To use this software you will need pthreads installed on each computer. Also, it is required to connect all the clients and the server to the same local network.

### Compilation

It is better to compile it via Makefile

```
Make
```

To get rid of useless files after build, use

```
make clean
```

## Usage

For the client (calculator) computers:

```
./client [number of cores allowed to perform calculations]
```

One computer will be the server (distributor).
```
./server [number of clients]
```

The server prints result of the calculations

## Note

1.  f(x) is hardcoded as FUNCTION (predefined)

2.  Integrate bounds are from From to To (hardcoded)

3.  Number of steps is NUM_STEPS (hardcoded)

4.  TurboBoost avoidance is realized using sort of crutch by setting taskss for the second core unused in computation. This task is the same as the first one, only to get the core busy.

5.  The hyperthreading avoidance is realised. The program will try to load all unused online physical cores at first and only then it'll load additional hyperthreads on each core.

## Authors

* **Nikolai Gaiduchenko** - *MIPT student, 513 group* - [Yozh2](https://github.com/Yozh2)

## Acknowledgments

Special thanks to Egor Korepanov for inspiration with alerts.h file and some other tips.
