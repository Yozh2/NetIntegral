# NetIntegral
A program for distributed multi-core integral calculations

Purpose:  Compute a definite integral using the Simpson formula
          by distributing integration intervals to clients to calculate them

Input:    clients_max (number of clients)
Output:   Estimate of the integral from From to To of FUNCTION
          using the Simpson formula
          
How to          
compile:  It is better to compile via makefile

Usage:    ./server [number of clients]
          ./client [number of cores allowed to perform calculations]
Note:
 |    1.  f(x) is hardcoded as FUNCTION (predefined)
 |    2.  Integrate bounds are from From to To (hardcoded)
 |    3.  Number of steps is NUM_STEPS (hardcoded)
 |    4.  TurboBoost avoidance is realized using sort of crutch
 |        by setting taskss for the second core unused in computation.
 |        This task is the same as the first one, only to get the core busy.
 |    5.  The hyperthreading avoidance is realised. The program will try to 
 |        load all unused online physical cores at first and only then it'll
 |        load additional hyperthreads on each core.
 
 Author: Nikolai Gaiduchenko, MIPT 2017, 513 group
