#include "client.h"
#include "util.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>  
#include <errno.h>
#include <termios.h>

// #define TEST_MODE
// #define SERVER_TEST_MODE

#define TERMINAL    "/dev/cu.usbmodem0004401986121"
#define INTERVAL 20

const char domain[] = "https://127.0.0.1:8081/"; // TODO: update to server URL when server is online
const char request[] = "update";

struct memory {
   char *response;
   size_t size;
 };

 struct broadcast_data {
    int64_t broadcast_len; // 8 bytes = sizeof(data)
    char *data;
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
    data - pointer to delivered data
    size - size is always 1
    nmemb - size of *data
    userdata - argument set by CURLOPT_WRITEDATA
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
    char *bigxstr = "Potato salad is a dish made from potatoes\n";  

    char *biggerxstr = "It is generally considered a side dish, as it usually accompanies the main course. Potato salad is widely believed \
     to have originated in Germany, spreading largely throughout Europe, European colonies and later Asia. American \
     potato salad\n";


    int xlen = strlen(xstr);
    int bigxlen = strlen(bigxstr);
    int biggerxlen = strlen(biggerxstr);

    int64_t test_size = 11*3;
    int64_t big_test_size = 42;
    int64_t bigger_test_size = 250;

    char* request_data;

    uint8_t testarray[250];
    for (int i = 0; i < 250; i++) {
        testarray[i]=i;
    }

    while (1) {

        struct memory response = {0};

        // handle HTTP request to pancast server
        int req = handle_request(&response);

        #ifdef SERVER_TEST_MODE
        for (int i = 0; i < response.size; i++) {
            printf("%x,", response.response[i]);
        }
        #endif

        if (req == 0) {

            printf("Writing data to serial port: %s\r\n", portname);

            // open serial terminal port
            if (!fd) {
                fd = open(portname, O_RDWR);
            }

            if (fd < 0)
            {
                printf("Error opening %s: %s\n", portname, strerror(errno));
                continue;
            }

            /*baudrate 115200, 8 bits, no parity, 1 stop bit */
            set_interface_attribs(fd, B115200);

            #ifdef TEST_MODE
            // // write data length
            // wlen = write(fd, &bigger_test_size, sizeof(int64_t));
            // if (wlen != sizeof(int64_t)) {
            //     printf("Error from write: %d, %d\n", wlen, errno);
            // }
            // printf("%d bytes written!\r\n", wlen);

            // // write data
            // wlen = write(fd, biggerxstr, biggerxlen);
            // if (wlen != biggerxlen) {
            //     printf("Error from write: %d, %d\n", wlen, errno);
            // }
            // tcdrain(fd);    /* delay for output */

            // printf("%d bytes written!\r\n", wlen);

            // write data length
            wlen = write(fd, &bigger_test_size, sizeof(int64_t));
            if (wlen != sizeof(int64_t)) {
                printf("Error from write: %d, %d\n", wlen, errno);
            }
            printf("%d bytes written!\r\n", wlen);

            // write data
            wlen = write(fd, &testarray, 250);
            if (wlen != 250) {
                printf("Error from write: %d, %d\n", wlen, errno);
            }
            tcdrain(fd);    /* delay for output */

            printf("%d bytes written!\r\n", wlen);

            char* readbuf = malloc(500);

            read(fd, readbuf, 500);

            printf("Received Print:\r\n %s\r\n", readbuf);

            #else

           // int64_t b_data_size = bytearr_to_uint64(response.response);
            int64_t data_sent = 0;

            // write data to serial port in CHUNK_SIZE increments
            while (data_sent < response.size) {
                printf("ready to receive\r\n");
                uint8_t ready = 0;
                while (ready == 0) {
                    printf("about to read\r\n");
                    int readmsg = read(fd, &ready, sizeof(uint8_t));
                    printf("readmsg: %d\r\n", readmsg);
                }
                printf("recieved ready!\r\n");
                // write data length
                wlen = write(fd, &response.response[data_sent], CHUNK_SIZE);
                if (wlen != CHUNK_SIZE) {
                    printf("Error from write: %d, %d\n", wlen, errno);
                }
                printf("%d bytes written!\r\n", wlen);
                data_sent = data_sent + CHUNK_SIZE; // assumption: b_data_size mod CHUNK_SIZE = 0
            }
            #endif
        }

        sleep(INTERVAL);
    }

    return 0;
}

