/*
 * The Generic UDP Scanner (generic-udp-scan) is Copyright (C) 2003 Roy Hills,
 * NTA Monitor Ltd.
 *
 * $Id$
 *
 * generic-udp-scan -- The Generic UDP Scanner
 *
 * Author:	Roy Hills
 * Date:	16 September 2003
 *
 * Usage:
 *    generic-udp-scan [options] [host...]
 *
 * Description:
 *
 * generic-udp-scan sends the specified UDP packet to the specified UDP port
 * on the specified hosts and displays any responses received.
 * 
 */

#include "udp-scan-engine.h"
#include "generic-udp-scan.h"

static char const rcsid[] = "$Id$";   /* RCS ID for ident(1) */

/* Global variables */
int dest_port=DEFAULT_DEST_PORT;	/* UDP destination port */
unsigned interval = DEFAULT_INTERVAL;	/* Interval between packets */
unsigned retry = DEFAULT_RETRY;		/* Number of retries */
unsigned timeout = DEFAULT_TIMEOUT;	/* Per-host timeout */
float backoff_factor = DEFAULT_BACKOFF_FACTOR;	/* Backoff factor */
int source_port = DEFAULT_SOURCE_PORT;	/* UDP source port */
char const scanner_name[] = "generic-udp-scan";
char const scanner_version[] = "1.1";
unsigned data_len;
unsigned char *udp_data;

extern int verbose;	/* Verbose level */
extern int debug;	/* Debug flag */
extern char *local_data;		/* Local data from --data option */
extern struct host_entry *rrlist;	/* Round-robin linked list "the list" */
extern unsigned num_hosts;		/* Number of entries in the list */

/*
 *	display_packet -- Check and display received packet
 *
 *	Inputs:
 *
 *	n		The length of the received packet in bytes
 *	packet_in	The received packet
 *	he		The host entry corresponding to the received packet
 *	recv_addr	IP address that the packet was received from
 *
 *      Returns:
 *
 *      None.
 *
 *      This should check the received packet and display details of what
 *      was received in the format: <IP-Address><TAB><Details>.
 */
void
display_packet(int n, char *packet_in, struct host_entry *he,
               struct in_addr *recv_addr) {
   char ip_str[MAXLINE];	/* IP address(es) to display at start */
   char *cp;
   int i;

/*
 *	Write the IP addresses to the output string.
 */
   cp = ip_str;
   cp += sprintf(cp, "%s\t", inet_ntoa(he->addr));
   if ((he->addr).s_addr != recv_addr->s_addr)
      cp += sprintf(cp, "(%s) ", inet_ntoa(*recv_addr));
   *cp = '\0';
/*
 *	We assume that any response is valid.
 *	Display IP address, packet length and packet data (both as hex and
 *	text).
 */
   cp = packet_in;
   printf("%sResponse: len=%d, data=",
             ip_str, n);
   cp = packet_in;
   for (i=0; i<n; i++) {
      printf("%.2x", (unsigned char) *cp);
      cp++;
   }
   printf(" (");
   cp = packet_in;
   for (i=0; i<n; i++) {
      printf("%c", isprint(*cp)?*cp:'.');
      cp++;
   }
   printf(")\n");
}

/*
 *	send_packet -- Construct and send a packet to the specified host
 *
 *	Inputs:
 *
 *	s		UDP socket file descriptor
 *	he		Host entry to send to
 *	dest_port	Destination UDP port
 *	last_packet_time	Time when last packet was sent
 *
 *      Returns:
 *
 *      None.
 *
 *      This must construct an appropriate packet and send it to the host
 *      identified by "he" and UDP port "dest_port" using the socket "s".
 *      It must also update the "last_send_time" field for this host entry.
 */
void
send_packet(int s, struct host_entry *he, int dest_port,
            struct timeval *last_packet_time) {
   struct sockaddr_in sa_peer;
   char buf[MAXUDP];
   int buflen;
   NET_SIZE_T sa_peer_len;
   int i;
   unsigned char *cp;
   static int first_time_through=1;
/*
 *	Initialise static packet data.
 *	We can't do this in initialise() because local_data is not available
 *	in that function.
 */
   if (first_time_through) {
      if (strlen(local_data) % 2) {   /* Length is odd */
         err_msg("Length of --data argument must be even (multiple of 2).");
      }
      data_len=strlen(local_data)/2;
      if ((udp_data = malloc(data_len)) == NULL)
         err_sys("malloc");
      cp = udp_data;
      for (i=0; i<data_len; i++)
         *cp++=hstr_i(&local_data[i*2]);
      first_time_through=0;
   }
/*
 *	Check that the host is live.  Complain if not.
 */
   if (!he->live) {
      warn_msg("***\tsend_packet called on non-live host entry: SHOULDN'T HAPPEN");
      return;
   }
/*
 *	Set up the sockaddr_in structure for the host.
 */
   memset(&sa_peer, '\0', sizeof(sa_peer));
   sa_peer.sin_family = AF_INET;
   sa_peer.sin_addr.s_addr = he->addr.s_addr;
   sa_peer.sin_port = htons(dest_port);
   sa_peer_len = sizeof(sa_peer);
/*
 *	Copy the required data into the output buffer "buf" and set "buflen"
 *	to the number of bytes in this buffer.
 */
   buflen=data_len;
   memcpy(buf, udp_data, buflen);
/*
 *	Update the last send times for this host.
 */
   if ((gettimeofday(last_packet_time, NULL)) != 0) {
      err_sys("gettimeofday");
   }
   he->last_send_time.tv_sec  = last_packet_time->tv_sec;
   he->last_send_time.tv_usec = last_packet_time->tv_usec;
   he->num_sent++;
/*
 *	Send the packet.
 */
   if (debug) {print_times(); printf("send_packet: #%u to host entry %u (%s) tmo %d\n", he->num_sent, he->n, inet_ntoa(he->addr), he->timeout);}
   if (verbose > 1)
      warn_msg("---\tSending packet #%u to host entry %u (%s) tmo %d", he->num_sent, he->n, inet_ntoa(he->addr), he->timeout);
   if ((sendto(s, buf, buflen, 0, (struct sockaddr *) &sa_peer, sa_peer_len)) < 0) {
      err_sys("sendto");
   }
}

/*
 *      initialise -- Protocol-specific initialisation routine.
 *
 *      Inputs:
 *
 *      None.
 *
 *      Returns:
 *
 *      None.
 *
 *      This is called once before any packets have been sent.  It can be
 *      used to perform any required initialisation.  It does not have to
 *      do anything.
 */
void
initialise(void) {
}

/*
 *      clean_up -- Protocol-specific Clean-Up routine.
 *
 *      Inputs:
 *
 *      None.
 *
 *      Returns:
 *
 *      None.
 *
 *      This is called once after all hosts have been processed.  It can be
 *      used to perform any tidying-up or statistics-displaying required.
 *      It does not have to do anything.
 */
void
clean_up(void) {
}

/*
 *      local_version -- Scanner-specific version function.
 *
 *      Inputs:
 *
 *      None.
 *
 *      Returns:
 *
 *      None.
 *
 *      This should output the scanner-specific version number to stderr.
 */
void
local_version(void) {
/* We use rcsid here to prevent it being optimised away */
   fprintf(stderr, "%s\n", rcsid);
}

/*
 *      local_help -- Scanner-specific help function.
 *
 *      Inputs:
 *
 *      None.
 *
 *      Returns:
 *
 *      None.
 *
 *      This should output the scanner-specific usage for the --data option
 *      (if any).
 */
void
local_help(void) {
   fprintf(stderr, "\n--data=<d> or -D <d>\tSpecify packet contents in hex.\n");
}

/*
 *      local_add_host -- Protocol-specific add host routine.
 *
 *      Inputs:
 *
 *      name = The Name or IP address of the host.
 *      timeout = The initial host timeout in ms.
 *
 *      Returns:
 *
 *      0 (Zero) if this function doesn't need to do anything, or
 *      1 (One) if this function replaces the generic add_host function.
 *
 *      This routine is called once for each specified host.
 *
 *      This protocol-specific add host routine can replace the generic
 *      udp-scan add-host routine if required.  If it is to replace the
 *      generic routine, then it must perform all of the add_host functions
 *      and return 1.  Otherwise, it must do nothing and return 0.
 */
int
local_add_host(char *name, unsigned timeout) {
   return 0;
}

/*
 *      Convert a two-digit hex string with to unsigned int.
 *      E.g. "0A" would return 10.
 *      Note that this function does no sanity checking, it's up to the
 *      caller to ensure that *cptr points to at least two hex digits.
 *      This function is a modified version of hstr_i at www.snippets.org.
 */
unsigned int hstr_i(char *cptr)
{
      unsigned int i;
      unsigned int j = 0;
      int k;

      for (k=0; k<2; k++) {
            i = *cptr++ - '0';
            if (9 < i)
                  i -= 7;
            j <<= 4;
            j |= (i & 0x0f);
      }
      return(j);
}