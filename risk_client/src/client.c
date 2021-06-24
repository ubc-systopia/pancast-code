#include "client.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>  
#include <errno.h>
#include <termios.h>

#define TEST_MODE

#define TERMINAL    "/dev/cu.usbmodem0004401986121"
#define INTERVAL 20


const char domain[] = "https://127.0.0.1:8081/"; // TODO: update to server URL when server is online
const char request[] = "update";

struct memory {
   char *response;
   size_t size;
 };

/* Set attributes for serial communication, 
    from https://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c 
*/
int set_interface_attribs(int fd, int speed)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        printf("Error from tcgetattr: %s\n", strerror(errno));
        return -1;
    }

    cfsetospeed(&tty, (speed_t)speed);
    cfsetispeed(&tty, (speed_t)speed);

    tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;         /* 8-bit characters */
    tty.c_cflag &= ~PARENB;     /* no parity bit */
    tty.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */
    tty.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */

    /* setup for non-canonical mode */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    /* fetch bytes as they become available */
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error from tcsetattr: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

/* Write data from stream, from CURLOPT_WRITEFUNCTION example 
    https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
 */
size_t write_function(char *data, size_t size, size_t nmemb, void *userdata){

size_t realsize = size * nmemb;
   struct memory *mem = (struct memory *)userdata;
 
   char *ptr = realloc(mem->response, mem->size + realsize + 1);
   if(ptr == NULL) {
       printf("out of memory!");
     return 0;  /* out of memory! */
   }

   mem->response = ptr;
   memcpy(&(mem->response[mem->size]), data, realsize);
   mem->size += realsize;
   mem->response[mem->size] = 0;
 
   return realsize;
}

/* Handle HTTP request to pancast server
    adapted from https://github.com/CedricFauth/c-client-flask-server-test
*/
int handle_request(struct memory* chunk) {
    unsigned url_len = strlen(domain) + strlen(request);
    char *url;

    if(!(url = malloc(url_len + 1))) return 1;
    strcpy(url,domain);
    strcat(url,request);
    url[url_len] = '\0';

    printf("\n\rCreating request: \t%s\n\r", url);
    
    curl_global_init(CURL_GLOBAL_ALL);
    
    CURL *curl = curl_easy_init();
    if(curl) {
        
        CURLcode res;
        curl_easy_setopt(curl, CURLOPT_URL, url);

        // Disable SSL verification for now
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);

        char* response_string;
        FILE* response;
        char* header_string;
 
        /* send all data to this function  */
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_function);
        
        /* we pass our 'chunk' struct to the callback function */
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)chunk);

        res = curl_easy_perform(curl);
        if(res != CURLE_OK)
            fprintf(stderr, "error: %s\r\n", curl_easy_strerror(res));

        printf("Received data! Size: %d\r\n", (int)chunk->size);
        // printf("response: %s\r\n", chunk->response);
        
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    free(url);
    return 0;
}

int main(int argc, char *argv[]) {

    char *portname = TERMINAL;
    int fd;
    int wlen;
    char *xstr = "test write\n";
    int xlen = strlen(xstr);

    char* request_data;

    while (1) {

        struct memory chunk = {0};
        
        // handle HTTP request to pancast server
        int req = handle_request(&chunk);

        if (req == 0) {

            printf("Writing data to serial port: %s\r\n", portname);

            // open serial terminal port
            fd = open(portname, O_RDWR);

            if (fd < 0)
            {
                printf("Error opening %s: %s\n", portname, strerror(errno));
               // continue;
            }

            /*baudrate 115200, 8 bits, no parity, 1 stop bit */
            set_interface_attribs(fd, B115200);

            /* simple output */
            wlen = write(fd, xstr, xlen);
            if (wlen != xlen) {
                printf("Error from write: %d, %d\n", wlen, errno);
            }
            tcdrain(fd);    /* delay for output */

            printf("%d bytes written!\r\n", wlen);
        }

        sleep(INTERVAL);
    }

    return 0;
}

