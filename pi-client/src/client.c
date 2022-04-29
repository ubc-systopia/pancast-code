#include "client.h"

int main(int argc, char *argv[]) 
{
  uart_main((void *) 0);

  fprintf(stderr, "exiting main! should not get here\r\n");

  return 0;
}
