#ifndef ALERTS_H
#define ALERTS_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

// Messages
#define ERR    "ERROR:  "

char __ALERTS_BUFFER1[BUFSIZ],
     __ALERTS_BUFFER2[BUFSIZ];

#ifdef  MSGPID
  #define PRINT_ERR(...)\
  {\
    sprintf(__ALERTS_BUFFER1, "PID %d:  " ERR "@%d  ", getpid(), __LINE__);\
    sprintf(__ALERTS_BUFFER2, __VA_ARGS__);\
    fprintf(stderr, "%s%s\n", __ALERTS_BUFFER1, __ALERTS_BUFFER2);\
    exit   (EXIT_FAILURE);\
  }

  #define PRINT_ERRV(...)\
  {\
    sprintf(__ALERTS_BUFFER1,"PID %d:  " ERR "@%d   ", getpid(), __LINE__);\
    sprintf(__ALERTS_BUFFER2, __VA_ARGS__);\
    fprintf(stderr, "%s%s\n- %s\n", __ALERTS_BUFFER1, __ALERTS_BUFFER2, strerror(errno));\
    exit   (EXIT_FAILURE);\
  }

  #define PRINT(...)\
    {\
      sprintf(__ALERTS_BUFFER1, "PID %d:  ", getpid());\
      sprintf(__ALERTS_BUFFER2, __VA_ARGS__);\
      fprintf(stdout, "%s%s", __ALERTS_BUFFER1, __ALERTS_BUFFER2);\
    }

#else  // MSGPID
  #define PRINT_ERR(...)\
  {\
    sprintf(__ALERTS_BUFFER1, ERR "@%d  ", __LINE__);\
    sprintf(__ALERTS_BUFFER2, __VA_ARGS__);\
    fprintf(stderr, "%s%s\n", __ALERTS_BUFFER1, __ALERTS_BUFFER2);\
    exit   (EXIT_FAILURE);\
  }


  #define PRINT_ERRV(...)\
  {\
    sprintf(__ALERTS_BUFFER1, ERR "@%d  ", __LINE__);\
    sprintf(__ALERTS_BUFFER2, __VA_ARGS__);\
    fprintf(stderr, "%s%s\n- %s\n", __ALERTS_BUFFER1, __ALERTS_BUFFER2, strerror(errno));\
    exit   (EXIT_FAILURE);\
  }

  #define PRINT(...)\
  {\
    sprintf(__ALERTS_BUFFER2,  __VA_ARGS__);\
    fprintf(stdout, "%s", __ALERTS_BUFFER2);\
  }

#endif // MSGPID

#define PRINT_LINE(...) \
{\
  PRINT(__VA_ARGS__); \
  fprintf(stdout, "\n");\
  fflush(stdout);\
}


#ifdef DEBUG
    #define DBG_PRINT(...) PRINT_LINE(__VA_ARGS__)
#else  // DEBUG
    #define DBG_PRINT(...)
#endif // DEGUG


#define TRY_TO(...) \
    if ((__VA_ARGS__) == -1) \
        PRINT_ERRV("LINE: [" #__VA_ARGS__ "]")

#endif // ALERTS_H
