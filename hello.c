#include <stdio.h>
#include "TutorialConfig.h"
// #include "deps.h"

#ifdef USE_MYMATH
    #include "deps.h"
#endif

int main(void) {

#ifdef USE_MYMATH
  int sq = mysqrt();
#else
  int sq =1;
#endif
    printf("hello, %d",sq);
    return 0;
}