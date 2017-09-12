/* client.c - go-back-n client implementation in C
 * by Elijah Jordan Montgomery <elijah.montgomery@uky.edu>
 * based on code by Kenneth Calvert
 *
 * This implements a go-back-n client that implements reliable data
 * transfer over UDP using the go-back-n ARQ with variable chunk size
 *
 * for debug purposes, a loss rate can also be specified in the accompanying
 * server program
 * compile with "gcc -o client client.c"
 * tested on UKY CS Multilab
 */

#include <stdio.h>		/* for printf() and fprintf() */
#include <sys/socket.h>		/* for socket(), connect(), sendto(), and recvfrom() */
#include <arpa/inet.h>		/* for sockaddr_in and inet_addr() */
#include <stdlib.h>		/* for atoi() and exit() */
#include <string.h>		/* for memset() */
#include <unistd.h>		/* for close() and alarm() */
#include <errno.h>		/* for errno and EINTR */
#include <signal.h>		/* for sigaction() */
#include "gbnpacket.c"

#define TIMEOUT_SECS    3	/* Seconds between retransmits */
#define MAXTRIES        10	/* Tries before giving up */

int tries = 0;			/* Count of times sent - GLOBAL for signal-handler access */
int base = 0;
int windowSize = 0;
int sendflag = 1;

void DieWithError (char *errorMessage);	/* Error handling function */
void CatchAlarm (int ignored);	/* Handler for SIGALRM */
int max (int a, int b);		/* macros that most compilers include - used for calculating a few things */
int min(int a, int b);		/* I think gcc includes them but this is to be safe */

int
main (int argc, char *argv[])
{
  int sock;			/* Socket descriptor */
  struct sockaddr_in gbnServAddr;	/* Echo server address */
  struct sockaddr_in fromAddr;	/* Source address of echo */
  unsigned short gbnServPort;	/* Echo server port */
  unsigned int fromSize;	/* In-out of address size for recvfrom() */
  struct sigaction myAction;	/* For setting signal handler */
  char *servIP;			/* IP address of server */
   int respLen;			/* Size of received datagram */
  int packet_received = -1;	/* highest ack received */
  int packet_sent = -1;		/* highest packet sent */
  char buffer[8192] =		/* buffer - randomly generated by me */
    "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";
  const int datasize = 8192;	/* data buffer size */
  int chunkSize;		/* chunk size in bytes */
  int nPackets = 0;		/* number of packets to send */
  
  if (argc != 5)		/* Test for correct number of arguments */
    {
      fprintf (stderr,
	       "Usage: %s <Server IP> <Server Port No> <Chunk size> <Window Size>\n",
	       argv[0]);
      exit (1);
    }

  servIP = argv[1];		/* First arg:  server IP address (dotted quad) */
  chunkSize = atoi (argv[3]);	/* Third arg: string to echo */
  gbnServPort = atoi (argv[2]);	/* Use given port */
  windowSize = atoi (argv[4]);
  if(chunkSize >= 512)
  {
    fprintf(stderr, "chunk size must be less than 512\n");
    exit(1);
  }

  nPackets = datasize / chunkSize; 
  if (datasize % chunkSize)
    nPackets++;			/* if it doesn't divide cleanly, need one more odd-sized packet */
  // nPackets--;
  /* Create a best-effort datagram socket using UDP */
  if ((sock = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    DieWithError ("socket() failed");
  printf ("created socket");

  /* Set signal handler for alarm signal */
  myAction.sa_handler = CatchAlarm;
  if (sigfillset (&myAction.sa_mask) < 0)	/* block everything in handler */
    DieWithError ("sigfillset() failed");
  myAction.sa_flags = 0;

  if (sigaction (SIGALRM, &myAction, 0) < 0)
    DieWithError ("sigaction() failed for SIGALRM");

  /* Construct the server address structure */
  memset (&gbnServAddr, 0, sizeof (gbnServAddr));	/* Zero out structure */
  gbnServAddr.sin_family = AF_INET;
  gbnServAddr.sin_addr.s_addr = inet_addr (servIP);	/* Server IP address */
  gbnServAddr.sin_port = htons (gbnServPort);	/* Server port */

  /* Send the string to the server */
  while ((packet_received < nPackets-1) && (tries < MAXTRIES))
    {
     // printf ("in the send loop base %d packet_sent %d packet_received %d\n",
	//      base, packet_sent, packet_received);
      if (sendflag > 0)
	{
	sendflag = 0;
	  int ctr; /*window size counter */
	  for (ctr = 0; ctr < windowSize; ctr++)
	    {
	      packet_sent = min(max (base + ctr, packet_sent),nPackets-1); /* calc highest packet sent */
	      struct gbnpacket currpacket; /* current packet we're working with */
	      if ((base + ctr) < nPackets)
		{
		  memset(&currpacket,0,sizeof(currpacket));
 		  printf ("sending packet %d packet_sent %d packet_received %d\n",
                      base+ctr, packet_sent, packet_received);

		  currpacket.type = htonl (1); /*convert to network endianness */
		  currpacket.seq_no = htonl (base + ctr);
		  int currlength;
		  if ((datasize - ((base + ctr) * chunkSize)) >= chunkSize) /* length chunksize except last packet */
		    currlength = chunkSize;
		  else
		    currlength = datasize % chunkSize;
		  currpacket.length = htonl (currlength);
		  memcpy (currpacket.data, /*copy buffer data into packet */
			  buffer + ((base + ctr) * chunkSize), currlength);
		  if (sendto
		      (sock, &currpacket, (sizeof (int) * 3) + currlength, 0, /* send packet */
		       (struct sockaddr *) &gbnServAddr,
		       sizeof (gbnServAddr)) !=
		      ((sizeof (int) * 3) + currlength))
		    DieWithError
		      ("sendto() sent a different number of bytes than expected");
		}
	    }
	}
      /* Get a response */

      fromSize = sizeof (fromAddr);
      alarm (TIMEOUT_SECS);	/* Set the timeout */
      struct gbnpacket currAck;
      while ((respLen = (recvfrom (sock, &currAck, sizeof (int) * 3, 0,
				   (struct sockaddr *) &fromAddr,
				   &fromSize))) < 0)
	if (errno == EINTR)	/* Alarm went off  */
	  {
	    if (tries < MAXTRIES)	/* incremented by signal handler */
	      {
		printf ("timed out, %d more tries...\n", MAXTRIES - tries);
		break;
	      }
	    else
	      DieWithError ("No Response");
	  }
	else
	  DieWithError ("recvfrom() failed");

      /* recvfrom() got something --  cancel the timeout */
      if (respLen)
	{
	  int acktype = ntohl (currAck.type); /* convert to host byte order */
	  int ackno = ntohl (currAck.seq_no); 
	  if (ackno > packet_received && acktype == 2)
	    {
	      printf ("received ack\n"); /* receive/handle ack */
	      packet_received++;
	      base = packet_received; /* handle new ack */
	      if (packet_received == packet_sent) /* all sent packets acked */
		{
		  alarm (0); /* clear alarm */
		  tries = 0;
		  sendflag = 1;
		}
	      else /* not all sent packets acked */
		{
		  tries = 0; /* reset retry counter */
		  sendflag = 0;
		  alarm(TIMEOUT_SECS); /* reset alarm */

		}
	    }
	}
    }
  int ctr;
  for (ctr = 0; ctr < 10; ctr++) /* send 10 teardown packets - don't have to necessarily send 10 but spec said "up to 10" */
    {
      struct gbnpacket teardown;
      teardown.type = htonl (4);
      teardown.seq_no = htonl (0);
      teardown.length = htonl (0);
      sendto (sock, &teardown, (sizeof (int) * 3), 0,
	      (struct sockaddr *) &gbnServAddr, sizeof (gbnServAddr));
    }
  close (sock); /* close socket */
  exit (0);
}

void
CatchAlarm (int ignored)	/* Handler for SIGALRM */
{
  tries += 1;
  sendflag = 1;
}

void
DieWithError (char *errorMessage)
{
  perror (errorMessage);
  exit (1);
}

int
max (int a, int b)
{
  if (b > a)
    return b;
  return a;
}

int
min(int a, int b)
{
  if(b>a)
	return a;
  return b;
}
