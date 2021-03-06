/*
   Copyright (c) 2015, Digi International Inc.

   Permission to use, copy, modify, and/or distribute this software for any
   purpose with or without fee is hereby granted, provided that the above
   copyright notice and this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
   WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
   MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
   ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
/*
	https_client.c

	Description
	===========
	This sample program demonstrates the use of the HTTP client library to
	request files from a remote web server and display them to stdout.

	This is similar to the "http client.c" sample, except that it also
	enables use of secure HTTP (HTTPS).  When you enter a URL to retrieve,
	you can type something like
	  https://www.example.com
	in order to use HTTPS.  It is the initial "https://" part of the URL
	which is significant in this case.  If omitted, the protocol defaults
	to ordinary HTTP (like "http://").

	Of course, the actual parsing and extraction of data from the web page
	is left as an exercise...

	Instructions
	============
	Run sample on a Rabbit with an Internet connection.  Enter URLs in the
	STDIO window, and the sample will download and display them to stdout.

*/


// Import the certificate files.  These are the CAs used at the time of writing
// this sample.  It is subject to change (beyond Digi's control).  You can
// #define SSL_CERT_VERBOSE and X509_VERBOSE in order to find out the
// certificates in use.

#ximport "../sample_certs/EquifaxSecureCA.crt"  ca_pem1
#ximport "../sample_certs/ThawtePremiumServerCA.crt"  ca_pem2
#ximport "../sample_certs/GTECyberTrustGlobalRoot.crt"  ca_pem3
#ximport "../sample_certs/VerisignClass3PublicPrimaryCA.crt"  ca_pem4

#define MP_SIZE 258			// Recommended to support up to 2048-bit RSA keys.

// Comment this out if the Real-Time Clock is set accurately.
#define X509_NO_RTC_AVAILABLE

///// Configuration Options /////

// define SHOW_HEADERS to display the HTTP headers in addition to the body
//#define SHOW_HEADERS

// define HTTPC_VERBOSE to turn on verbose output from the HTTP Client library
//#define HTTPC_VERBOSE

// Override the default number of redirections allowed (1).  Don't set this
// too high, since it potentially causes that amount of recursion.  Anything
// over 5 would probably indicate some sort of configuration error in the
// servers (e.g. two servers bouncing a request back and forth).
#define HTTPC_MAX_REDIRECT 5

// define UPDATE_RTC to sync the Rabbit's real-time clock to the web server's
// if more than 10 seconds out of sync.
//#define UPDATE_RTC

/*
 * NETWORK CONFIGURATION
 * Please see the function help (Ctrl-H) on TCPCONFIG for instructions on
 * compile-time network configuration.
 */
#define TCPCONFIG 5

/*
 * Uncomment these macros to enable single-stepping and debug messages.
 */
//#define HTTPC_VERBOSE
//#define SSL_SOCK_VERBOSE
//#define _SSL_PRINTF_DEBUG 1
//#define SSL_CERT_VERBOSE
//#define X509_VERBOSE

//#define DCRTCP_DEBUG
//#define HTTPC_DEBUG
//#define X509_DEBUG
//#define RSA_DEBUG
//#define SSL_CERT_DEBUG
//#define SSL_TPORT_DEBUG
//#define SSL_SOCK_DEBUG


///// End of Configuration Options /////

#define SSPEC_NO_STATIC		// Required because we're not using any static
									// Zserver resources.

#use "dcrtcp.lib"
#use "ssl_sock.lib"			// It is the inclusion of this library before
									// http_client.lib which basically enables use
									// of HTTPS.  Use of any other SSL client or server
									// will also enable HTTPS.
#use "http_client.lib"


/*
 *  Server certificate policy callback
 */
int my_server_policy(ssl_Socket far * state, int trusted,
	                       struct x509_certificate far * cert,
	                       httpc_Socket far * s)
{
	printf("\nChecking server certificate...\n");
	if (trusted)
		printf("This server's certificate is trusted\n");
	else
		printf("There was no list of CAs, so cannot verify this server's certificate\n");

	printf("Certificate issuer:\n");
	if (cert->issuer.c)
		printf("       Country: %ls\n", cert->issuer.c);
	if (cert->issuer.l)
		printf("      Location: %ls\n", cert->issuer.l);
	if (cert->issuer.st)
		printf("         State: %ls\n", cert->issuer.st);
	if (cert->issuer.o)
		printf("  Organization: %ls\n", cert->issuer.o);
	if (cert->issuer.ou)
		printf("          Unit: %ls\n", cert->issuer.ou);
	if (cert->issuer.email)
		printf("       Contact: %ls\n", cert->issuer.email);
	if (cert->issuer.cn)
		printf("            CN: %ls\n", cert->issuer.cn);
	printf("Certificate subject:\n");
	if (cert->subject.c)
		printf("       Country: %ls\n", cert->subject.c);
	if (cert->subject.l)
		printf("      Location: %ls\n", cert->subject.l);
	if (cert->subject.st)
		printf("         State: %ls\n", cert->subject.st);
	if (cert->subject.o)
		printf("  Organization: %ls\n", cert->subject.o);
	if (cert->subject.ou)
		printf("          Unit: %ls\n", cert->subject.ou);
	if (cert->subject.email)
		printf("       Contact: %ls\n", cert->subject.email);
	printf("Server claims to be CN='%ls'\n", cert->subject.cn);
	printf("We are looking for  CN='%ls'\n", s->hostname);

	if (x509_validate_hostname(cert, s->hostname)) {
		printf("Mismatch!\n\n");
		return 1;
	}
	printf("We'll let that pass...\n\n");
	return 0;
}


void print_time()
{
	struct tm		rtc;					// time struct

   mktm( &rtc, read_rtc());
   printf( "The current RTC date/time is: %s", asctime( &rtc));
}

void httpc_demo(tcp_Socket *sock)
{
	char	url[256];
	char	body[65];		// buffer for reading body
	long	curr_skew;

	httpc_Socket hsock;
	int retval;
	int is_text;
	char far *value;

	// last clock skew setting
	curr_skew = 0;

   retval = httpc_init (&hsock, sock);
   if (retval)
   {
      printf ("error %d calling httpc_init()\n", retval);
   }
   else
   {
   	is_text = 0;
      printf ("\nEnter a URL to retrieve using the following format:\n");
      printf ("[http://][user:pass@]hostname[:port][/file.html]\n");
      printf ("Items in brackets are optional.  Examples:\n");
      printf ("  http://www.google.com/\n");
      printf ("  www.google.com\n");
      printf ("  google.com\n");
      printf ("For secure HTTP:\n");
      printf ("  https://www.google.com/accounts/CreateAccount\n");
      printf ("  https://www.unionbank.com\n");
      while (1)
      {
         printf ("\n\nEnter URL (blank to exit): ");
         gets (url);
         if (*url == '\0')
         {
         	// To save typing and create a default, comment out the 'break'
         	// and uncomment the next line...
         	break;
         	//strcpy(url, "https://www.google.com/accounts/CreateAccount");
         }

			// clear screen (first string) and print name of URL to download
         printf ("\x1B[2J" "Retrieving [%s]...\n", url);
         
         // Turn on auto redirect to follow 3xx redirect responses
         httpc_set_mode(HTTPC_AUTO_REDIRECT);
         
         retval = httpc_get_url (&hsock, url);
         if (retval)
         {
            printf ("error %d calling httpc_get_url()\n", retval);
         }
         else
         {
            while (hsock.state == HTTPC_STATE_HEADER)
            {
               retval = httpc_read_header (&hsock, url, sizeof(url));
               if (retval > 0)
               {
	               if ( (value = httpc_headermatch( url, "Content-Type")) )
	               {
	                  is_text = (0 == strncmpi( value, "text/", 5));
	               }
                  #ifdef SHOW_HEADERS
                     #ifndef HTTPC_VERBOSE
                        // echo headers if HTTP client didn't already do so
                        printf (">%s\n", url);
                     #endif
                  #endif
               }
               else if (retval < 0)
               {
                  printf ("error %d calling httpc_read_header()\n", retval);
               }
            }
            printf ("Headers were parsed as follows:\n");
            printf ("  HTTP/%s response = %d, filesize = %lu\n",
               (hsock.flags & HTTPC_FLAG_HTTP10) ? "1.0" :
               (hsock.flags & HTTPC_FLAG_HTTP11) ? "1.1" : "???",
               hsock.response, hsock.filesize);
            if (hsock.flags & HTTPC_FLAG_CHUNKED)
            {
               printf ("  body will be sent chunked\n");
            }
            printf ("  Rabbit's RTC is %ld second(s) off of server's time\n",
               hsock.skew - curr_skew);

            #ifdef UPDATE_RTC
            	if (labs (hsock.skew - curr_skew) > 10)
            	{
            		// only update if off by more than 10 seconds
            		print_time();
	               printf ("  Updating Rabbit's RTC to match web server.\n");
	               write_rtc (SEC_TIMER + hsock.skew);
	               curr_skew = hsock.skew;
	               print_time();
	            }
            #endif

            printf ("\nBody:\n");
            while (hsock.state == HTTPC_STATE_BODY)
            {
               retval = httpc_read_body (&hsock, body, 64);
               if (retval < 0)
               {
                  printf ("error %d calling httpc_read_body()\n", retval);
               }
               else if (retval > 0)
               {
	               if (is_text)
	               {
	                  body[retval] = '\0';
	                  printf ("%s", body);
	               }
	               else
	               {
							mem_dump( body, retval);
	               }
               }
            }
            httpc_close (&hsock);
            tcp_tick(NULL);
         }
      }
   }

}

// It's safer to keep sockets as globals, especially when using uC/OS-II.  If
// your socket is on the stack, and another task (with its own stack, instead
// of your task's stack) calls tcp_tick, tcp_tick won't find your socket
// structure in the other task's stack.
// Even though this sample doesn't use uC/OS-II, using globals for sockets is
// a good habit to be in.
tcp_Socket demosock;

void main()
{
	int rc;
	SSL_Cert_t trusted;

	// First, parse the trusted CA certificates.
	memset(&trusted, 0, sizeof(trusted));
	rc = SSL_new_cert(&trusted, ca_pem1, SSL_DCERT_XIM, 0);
	if (rc) {
		printf("Failed to parse CA certificate 1, rc=%d\n", rc);
		return;
	}
	rc = SSL_new_cert(&trusted, ca_pem2, SSL_DCERT_XIM, 1 /*append*/);
	if (rc) {
		printf("Failed to parse CA certificate 2, rc=%d\n", rc);
		return;
	}
	rc = SSL_new_cert(&trusted, ca_pem3, SSL_DCERT_XIM, 1 /*append*/);
	if (rc) {
		printf("Failed to parse CA certificate 3, rc=%d\n", rc);
		return;
	}
	rc = SSL_new_cert(&trusted, ca_pem4, SSL_DCERT_XIM, 1 /*append*/);
	if (rc) {
		printf("Failed to parse CA certificate 4, rc=%d\n", rc);
		return;
	}

	// Set TLS/SSL options.  These act globally, for all HTTPS connections
	// until chenged to some other setting.  Normally, this only needs to
	// be done once at start of program.
	httpc_set_tls(SSL_F_REQUIRE_CERT,	// Check HTTPS server certificate
						NULL,						// We don't have a cert to offer.  Not
													// normally needed for HTTPS.
						&trusted,				// Have a trusted CA!
						my_server_policy);	// Test policy callback


	// initialize tcp_Socket structure before use
	memset( &demosock, 0, sizeof(demosock));

	printf ("http client v" HTTPC_VERSTR "\n");

	printf ("Initializing TCP/IP stack...\n");
	sock_init_or_exit(1);

   httpc_demo(&demosock);
}