/*
NAME: Chaoran Lin
EMAIL: linmc@ucla.edu
ID: 004674598
*/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

/* Handler function for SIGSEGV */
void sigsegvHandler(int signo)
{
  if (signo == SIGSEGV)
    fprintf(stderr, "Caught a segmentation fault\n");
  /* Must exit immediately, SIGSEGV continues to fire since the process is
  still faulty and never terminated */
  exit (2);
}

int main (int argc, char **argv)
{
  int c;

  static struct option long_options[] =
    {
      {"input", required_argument, 0, 'i'},
      {"output", required_argument, 0, 'o'},
      {"segfault", no_argument, 0, 's'},
      {"catch", no_argument, 0, 'c'},
      {0, 0, 0, 0}
    };

  /*chars for storing input and output files*/
  char * inputFile = NULL;
  char * outputFile = NULL;
  int segfaultFlag = 0;
  int catchFlag = 0;

  /* getopt_long stores the option index here. */
  int option_index = 0;

  while ((c = getopt_long (argc, argv, "i:o:sc",
			   long_options, &option_index)) != -1 || (c = getopt (argc, argv, "i:o:sc")) != -1) {
    switch (c)
      {
      case 0:
	printf ("option %s", long_options[option_index].name);
	if (optarg)
	  printf (" with arg %s", optarg);
	printf ("\n");
	break;

      case 'i':
	inputFile = optarg;
	printf("option --input with value '%s', reading into inputFile\n", optarg);
	printf("inputFile: '%s'\n", inputFile);
	break;

      case 'o':
	outputFile = optarg;
	printf ("option --output with value '%s', writing into outputFile\n", optarg);
	printf("outputFile: '%s'\n", outputFile);
	break;

      case 's':
	segfaultFlag = 1;
	puts ("option --segfault specified");
	break;

      case 'c':
	catchFlag = 1;
	puts ("option --catch specified");
	break;

      case '?':
	/* getopt_long already printed an error message. */
	exit (2);
	return 1;

      default:
	abort ();
      }
  }

  /* If input and output are specified */
    /* Attempt to open the input file */
  if (segfaultFlag == 0) {
    int inputfd = 0;
    if (inputFile != NULL) {
        inputfd = open(inputFile, O_RDONLY);
        if (inputfd == -1) {
          fprintf(stderr, "Unable to open file '%s'!\n", inputFile);
          exit (2);
          return 2;
        }
    }

    /* Attempt to create the output file (using creat()) */
    /* Give user read, write, and execute permissions with S_IRXWU mode */
    int outputfd = 1;
    if (outputFile != NULL) {
        outputfd = creat(outputFile, S_IRWXU);
        if (outputfd == -1) {
          fprintf(stderr, "Unable to create file '%s'!\n", outputFile);
          exit (2);
          return 3;
        }
    }
    else {
        dup2(inputfd, outputfd);
    }

    /* Read input file and write to output file using buffer */
    const size_t BUFFER_SIZE = 8192;
    char buffer[BUFFER_SIZE];
    ssize_t readByte;

    /* while the read byte is not EOF */
    while ((readByte = read(inputfd, &buffer, BUFFER_SIZE)) > 0) {
      write(outputfd, &buffer, readByte);
    }

    /* Close the file descriptors so they may be reused*/

    close(inputfd);
    close(outputfd);
    exit (2);
    return 0;
  }
    /* Create segfault immediately */
    /* Assign a ptr to point to null */
  	char * ptr = NULL;
    int caughtSig = 0;
    if (catchFlag == 1) {
      /* Register the SIGSEGV handler */
      if (signal(SIGSEGV, sigsegvHandler) == SIG_ERR) {
        printf("Could not catch SIGSEGV\n");
      }
      else {
        caughtSig = 1;
      }
    }
    /* Try to dereference a nullptr */
    char ptr2 = *ptr;
    if (caughtSig == 1)
      return 4;
}
