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
/*******************************************************************************
        Samples\tcpip\POP3\pop_tls.c

        A program that will connect to a POP3 server and download
        e-mail from it, optionally deleting the messages after they have
        been read. This version extends the "parse_extend.c" sample to
        add TLS security.



        WARNING: This sample suggests use of some freely available POP3
        services (GMail and Hotmail).  This is for purpose of demonstration
        only and does not imply that use of those services for anything other
        than test/development is permitted by those service providers.
        Please read and abide by the provider's relevant service agreements.


        Before running:

        1) Make sure your basic network configuration is OK, including a
           DNS (name server) and a route to the outside Internet.  The
           default in this sample expects DHCP.  You can use a different
           TCPCONFIG macro and set alternative parameters if necessary.
           (If necessary, use a simpler sample to get this working).
        2) Set up a Google Gmail account for test purposes.  Follow Google's
           instructions for "enabling POP" on this account.
           See http://gmail.com.
        3) Modify the POP_USER and POP_PASS macros to be the account name and
           password that you used in step (2).  It is most convenient to
           put these in the Options->ProjectOptions->Defines panel e.g.
              POP_USER="my_account@gmail.com"
              POP_PASS="myPassw0rd"
        5) The POP_SERVER and POP_PORT macros are set appropriately for
           Gmail as of the time this sample was constructed, but you may wish
           to check this if the sample seems to fail in spite of everything.
           You can override these macros in the Defines panel.

        NOTE: you can also use a Hotmail/Outlook.com account once Microsoft
        upgrades their servers from TLS 1.0 to TLS 1.2 (still not the case
        in February 2016).
        
        In this case, #define POP_SERVER "pop-mail.outlook.com", and use your
        Hotmail/Outlook.com credentials in POP_USER and POP_PASS.  You may
        also need to enable POP access for your account in Options:
           https://outlook.live.com/owa/#path=/options/popandimap
        
        Unfortunately, Microsoft in their wisdom use 4096 bit RSA keys in
        some of their certificates, thus you need to #define MP_SIZE 514.
        GMail uses 2048-bit keys, and requires a MP_SIZE of at least 258.
        
        To date, Yahoo does not allow POP3/SMTP access with their free email
        accounts.
*******************************************************************************/

#class auto

// Import the certificate file(s).  This is the CA used at the time of writing
// this sample.  It is subject to change (beyond Digi's control).  You can
// #define SSL_CERT_VERBOSE and X509_VERBOSE in order to find out the
// certificates in use.
#ximport "../sample_certs/EquifaxSecureCA.crt"  ca_pem1

// This one for Hotmail/Outlook.com (POP3 and SMTP)
#ximport "../sample_certs/GTECyberTrustGlobalRoot.crt"  ca_pem2
#define MP_SIZE 258			// necessary for GMail's RSA keys

//#define MP_SIZE 514			// Recommended to support 4096-bit RSA keys used by
									// some Microsoft certs.

// Comment this out if the Real-Time Clock is set accurately.
#define X509_NO_RTC_AVAILABLE



/***********************************
 * Configuration                   *
 * -------------                   *
 * All fields in this section must *
 * be altered to match your local  *
 * network settings.               *
 ***********************************/

/*
 * NETWORK CONFIGURATION
 * Please see the function help (Ctrl-H) on TCPCONFIG for instructions on
 * compile-time network configuration.
 */
#define TCPCONFIG 5		// 5 for DHCP

/*
 * POP3 settings
 */

/*
 *	Enter the name and TCP port of your POP3 server here.
 */
#ifndef POP_SERVER
	#define POP_SERVER	"pop.gmail.com"	// GMail
	//#define POP_SERVER	"pop-mail.outlook.com"		// Hotmail
#endif
#ifndef POP_PORT
	#define POP_PORT	995	// Port 995 used for POP3 tunneled through TLS
#endif

/*
 * This is the username and password for the account on the
 * pop3 server.
 */
#ifndef POP_USER
	#warnt "Using bogus account.  Set POP_USER=... and POP_PASS=..."
	#warnt "in the Options->Project->Defines panel."
	#define POP_USER  "me@gmail.com"
	#define POP_PASS  "my_password"
#endif


/* comment this out to delete the messages off the server after they are read */
#define POP_NODELETE



/*
 *   The SMTP_VERBOSE macro logs the communications between the mail
 *   server and your controller.  Uncomment this define to begin
 *   logging.  The other macros add more info from other components.
 */
//#define POP_VERBOSE
//#define SSL_SOCK_VERBOSE
//#define _SSL_PRINTF_DEBUG 1
//#define SSL_CERT_VERBOSE
//#define X509_VERBOSE
//#define TCP_VERBOSE

//#define POP_DEBUG
//#define X509_DEBUG
//#define RSA_DEBUG
//#define SSL_CERT_DEBUG
//#define SSL_TPORT_DEBUG
//#define SSL_SOCK_DEBUG

/********************************
 * End of configuration section *
 ********************************/



/*
 * When this is defined, the POP3 library will do extra parsing of the
 * incoming e-mails, separating the 'to:', 'from:', 'subject:' and body
 * fields from the rest of the header, and provide this data in a nicer
 * manner.
 * NOTE: Changes the parameters passed to storemsg() .
 */
#define POP_PARSE_EXTRA

#define SSPEC_NO_STATIC		// Required because we're not using any static
									// Zserver resources.
#define POP_AUTH_FAIL_IF_NO_AUTH
#define POP_AUTH_TLS 1		// Required to include TLS
#memmap xmem
#use "dcrtcp.lib"
#use "ssl_sock.lib"
#use "pop3.lib"

/*
 *  Server certificate policy callback
 */
int pop_server_policy(ssl_Socket far * state, int trusted,
	                       struct x509_certificate far * cert,
                          void __far * data)
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
	printf("We are looking for  CN='%s'\n", pop3_getserver());

	if (x509_validate_hostname(cert, pop3_getserver())) {
		printf("Mismatch!\n\n");
		return 1;
	}
	printf("We'll let that pass...\n\n");
	return 0;
}



/*
 * 	This is the POP_PARSE_EXTRA calling style.
 */
int n;
int storemsg(int num, char *to, char *from, char *subject, char *body, int len)
{
	#GLOBAL_INIT { n = -1; }

	if(n != num) {
		n = num;
		printf("RECEIVING MESSAGE <%d>\n", n);
		printf("\tFrom: %s\n", from);
		printf("\tTo: %s\n", to);
		printf("\tSubject: %s\n", subject);
	}

	printf("MSG_DATA> '%s'\n", body);

	return 0;
}

void main()
{
	static long address;
	static int retval;
	char buf[30];
	auto SSL_Cert_t trusted;
	auto int rc;

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

	// Start network and wait for interface to come up (or error exit).
	sock_init_or_exit(1);

	/* As of DC10.60, the POP3 library can resolve host name for us...*/
	//printf("Resolving name %s\n", POP_SERVER);
	//address = resolve(POP_SERVER);
	//printf("...is at %s\n", inet_ntoa(buf, address));

	pop3_init(storemsg);

	pop3_set_tls(SSL_F_REQUIRE_CERT,		// Check POP3 server certificate
						NULL,			// We don't have a cert to offer
						&trusted,	// Have a trusted CA!
						pop_server_policy,	// Test policy callback
						0);

	pop3_setserver(POP_SERVER);

	pop3_getmail(POP_USER, POP_PASS, 0);

	while((retval = pop3_tick()) == POP_PENDING)
		continue;

	printf("============= Completed: ===============\n");
	if(retval == POP_SUCCESS)
		printf("POP was successful\n");
	else if(retval == POP_TIME)
		printf("POP timed out\n");
	else if(retval == POP_ERROR)
		printf("POP could not open TCP socket\n");
	else
		printf("DNS failed to resolve server domain name\n");

}