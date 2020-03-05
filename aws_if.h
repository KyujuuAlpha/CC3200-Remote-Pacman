/*
 * aws_if.h
 *
 *  Created on: Feb 17, 2020
 *      Author: Troi-Ryan Stoeffler
 */

#ifndef AWS_IF_H_
#define AWS_IF_H_

#define MAX_URI_SIZE 128
#define URI_SIZE MAX_URI_SIZE + 1

#define APPLICATION_NAME        "SSL"
#define APPLICATION_VERSION     "1.1.1.EEC.Spring2018"
#define SERVER_NAME             "a1euv4eww1wx8z-ats.iot.us-west-2.amazonaws.com"
#define GOOGLE_DST_PORT         8443

#define SL_SSL_CA_CERT "/cert/ca.pem" //starfield class2 rootca (from firefox) // <-- this one works
#define SL_SSL_PRIVATE "/cert/private.key"
#define SL_SSL_CLIENT  "/cert/client.pem"

// DON'T MODIFY BELOW //

#define MONTH               2
#define DATE                17    /* Current Date */
#define YEAR                2020  /* Current year */
#define HOUR                8    /* Time - hours */
#define MINUTE              17    /* Time - minutes */
#define SECOND              0     /* Time - seconds */

#define ZEROCHAR '0'

#define POSTHEADER "POST /things/CC3200_Thing/shadow HTTP/1.1\n\r"
#define GETHEADER "GET /things/CC3200_Thing/shadow HTTP/1.1\n\r"
#define HOSTHEADER "Host: a26ypaoxj1nj7v-ats.iot.us-west-2.amazonaws.com\r\n"
#define CHEADER "Connection: Keep-Alive\r\n"
#define CTHEADER "Content-Type: application/json; charset=utf-8\r\n"
#define CLHEADER1 "Content-Length: "
#define CLHEADER2 "\r\n\r\n"

#define DATA_PREF "{\"state\": {\r\n\"desired\" : {\r\n\"var\" : \""
#define DATA_SUFF "\"\r\n}}}\r\n\r\n"

// Application specific status/error codes
typedef enum {
    // Choosing -0x7D0 to avoid overlap w/ host-driver's error codes
    LAN_CONNECTION_FAILED = -0x7D0,
    INTERNET_CONNECTION_FAILED = LAN_CONNECTION_FAILED - 1,
    DEVICE_NOT_IN_STATION_MODE = INTERNET_CONNECTION_FAILED - 1,

    STATUS_CODE_MAX = -0xBB8
} e_AppStatusCodes;

typedef struct {
   /* time */
   unsigned long tm_sec;
   unsigned long tm_min;
   unsigned long tm_hour;
   /* date */
   unsigned long tm_day;
   unsigned long tm_mon;
   unsigned long tm_year;
   unsigned long tm_week_day; //not required
   unsigned long tm_year_day; //not required
   unsigned long reserved[3];
} SlDateTime;

// local function prototypes
static long WlanConnect();
static int set_time();
static long InitializeAppVariables();
static int tls_connect();
static int connectToAccessPoint();
static int http_get(int iTLSSockID);
static int http_post(int iTLSSockID, char *text);

// "public" function prototypes
void networkConnect(void);
void networkKill(void);
void sendString(char *text);

long printErrConvenience(char * msg, long retVal);
int connectToAccessPoint();

#endif /* AWS_IF_H_ */
