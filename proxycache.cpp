// Christian Quintero
// COEN 162 - Web Infrastructure
// FILE: proxycache.c
//
// This program creates a proxy cache by initializing a TCP connection on a 
// specified port and listens for a GET request from a browser client on the same
// port. If the page requested is in the cache, the program sends an if-modified-since
// GET request to the server and receives the updated page if page has been modified, 
// otherwise returns page from cache to client.
// If page is not in cache, the program retrieves it from the server and adds it to cache.


#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <vector>
#include <ostream>
#include <fstream>
#include <ctime>
#include <cstddef>
#include <cstdio>
#define handle_error(msg) \
	do { perror(msg); exit(EXIT_FAILURE); } while (0)
#define PORT_NUM 6005
#define BUFF_SIZE 1024
#define MAX_REQ_LENGTH 1024
#define MAX_CACHE_LENGTH 200
#define LISTEN_BACKLOG 20

using namespace std;

typedef struct{ // cache structure to store web page info
    string filename;
    string URL;
    string date;
}CACHEDATA;

/************************
 * function declarations
 ***********************/
int main (int, char *[]);
string retrieveDate();
string retrieveFilename(const vector<CACHEDATA> &cache);
int checkCache(const vector <CACHEDATA> &cache, string filepath);
void savePage(vector <CACHEDATA> &cache, string filename, string URL);



/********************
 * main
 ********************/
int main (int argc, char *argv[])
{
    char request[1024];
    // initialize values
    int sockfd = 0, n = 0, b, l;
    int listenfd=0, acc = 0;
    char req[BUFF_SIZE];
    struct sockaddr_in serv_addr, host_addr;
    struct hostent* hostent;
    char get_template[] = "GET %s HTTP/1.1\r\nHost: %s\r\n\r\nConnection: close\r\n\r\n";
    char get_if_template[] = "GET %s HTTP/1.1\r\nHost: %s\r\nIf-Modified-Since: %s\r\nConnection: close\r\n\r\n";
    char objpath[100];
    char filename[100];
    char host[100];
    in_addr_t in_addr;
    ssize_t allnbytes, finalnbyte;
    FILE *cacheFile;
    vector <CACHEDATA> cache;
    ofstream fOut;    
    ifstream fIn;
    size_t foundFile;
    int cacheIndex;
    string cacheDate;
    CACHEDATA tempCache; 

    // open socket
    if ((listenfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf ("Error : Could not create socket \n");
        return 1;
    }

    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons (PORT_NUM);

    // bind
    if (bind(listenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
    {
      perror("ERROR on binding");
      exit(1);
  	}
    else
        printf("binded Successfully\n");

    // listen
    if (listen(listenfd, LISTEN_BACKLOG) == -1)
    {
        printf("listen error\n");
        return 1;
    }
    else
    {
    	printf("listening\n")
    }
    while(1)
    {
        memset(request, '\0', sizeof (request));
        memset(req, '\0', sizeof(req));
        memset(host, '\0', sizeof(host));
        memset(objpath, '\0', sizeof(objpath));
        memset(filename, '\0', sizeof(filename));
        printf("waiting for new connection\n");

        // if connection is found
        acc = accept(listenfd, (struct sockaddr *) NULL, NULL);
        printf("client no. %d connected\n",listenfd);
        n = read(acc,req, BUFF_SIZE-1);
        if (n < 0) 
        {
            perror("ERROR reading from socket");
            exit(1);
        }
        if (n > 0)
        {   // parse request for host and path
            int i, j;
            for (i = 0; i < strlen(req); i++)
            {
                j = 0;
                if (req[i] == 'G' && req[i+1] == 'E' && req[i+2] == 'T')
                {
                    i = i+4;
                }
                while(req[i] != ' ')
                {
                    objpath[j] = req[i];
                    j++;
                    i++;
                }
                break;
            }
            for (i=i; i < strlen(req); i++)
            {
                j = 0;
                if (req[i] == 'H' && req[i+1] == 'o' && req[i+2] == 's' && req[i+3] == 't')
                {
                    i = 11;
                    while(req[i] != ' ' && req[i] != '/')
                    {
                        host[j] = req[i];
                        j++;
                        i++;
                    }
                    break;
                }
            }
            printf("path: %s \nhostname: %s \n", objpath, host);
            printf("buffer is: %s\n", req);
            printf("GET is: %s\n", objpath);
            printf("Host is: %s\n", host);
            cacheIndex = checkCache(cache, objpath);
            if (cacheIndex != -1)
            {
                tempCache = cache[cacheIndex];
                cacheDate = tempCache.date;
                char cacheArray[cacheDate.length()]; 
                strcpy(cacheArray, cacheDate.c_str()); 

				//set up
                int request_length = snprintf(request, MAX_REQ_LENGTH, get_if_template, objpath, cacheArray);
                if (request_length >= MAX_REQ_LENGTH)
                {
                    fprintf(stderr, "request length large: %d\n", request_length);
                    exit(EXIT_FAILURE);
                }


                //Open Socket for requested server
                if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                {
                    printf("Error: could not create socket \n");
                    exit(EXIT_FAILURE);
                }

                //retrieve host server
                hostent = gethostbyname(host);
                if(hostent == NULL)
                {
                    printf("Error: Unable to get host by name\n");
                    exit(EXIT_FAILURE);
                }

                // setup address
                in_addr = inet_addr(inet_ntoa(*(struct in_addr*)*(hostent->h_addr_list)));
                if (in_addr == (in_addr_t)-1)
                {
                    fprintf(stderr, "error: inet_addr(\"%s\")\n", *(hostent->h_addr_list));
                    exit(EXIT_FAILURE);
                }
                host_addr.sin_addr.s_addr = in_addr;
                host_addr.sin_family = AF_INET;
                host_addr.sin_port = htons(80);

                // connect with requested server
                if (connect (sockfd, (struct sockaddr *)&host_addr, sizeof (host_addr)) < 0)
                {
                    printf ("Error : Connect Failed \n");
                    return 1;
                }
                printf("connected\n");

                // send http request
                allnbytes = 0;
                while (allnbytes < request_length) 
                {
                    finalnbyte = write(sockfd, request + allnbytes, request_length - allnbytes);
                    if (finalnbyte == -1)
                    {
                        perror("write");
                        exit(EXIT_FAILURE);
                    }
                    allnbytes += finalnbyte;
                }
                printf("sent request\n");
                string fname = retrieveFilename(cache);
                fOut.open(fname);
                savePage(cache, objpath, fname);
                if (!(foundFile == std::string::npos))
                {
                    string fName = tempCache.filename;
                    char newCacheArray[fName.length()];
                    strcpy(newCacheArray, fName.c_str());
                    if ((cacheFile = fopen(newCacheArray, "r")) == NULL)
                    {
                	    perror("fopen");
                        exit(EXIT_FAILURE);
                    }
                    fseek(cacheFile, 0, SEEK_END);
                    uint32_t size = ftell(cacheFile);
                    rewind(cacheFile);
                    for (i = size; i > 0; i -= BUFF_SIZE)
                    {
                        fread(req, 1, BUFF_SIZE, cacheFile);
                        send(acc, req, BUFF_SIZE, 0);
                    }
                    fclose(cacheFile);
                }
                else
                {
                    tempCache.date = retrieveDate();
                    string fName = tempCache.filename;
                    fOut.open(fName);

                    //send the file to the browser and update the stored copy
                    while ((allnbytes = read(sockfd, req, BUFF_SIZE - 1)) > 0) 
                    {
                        write(acc, req, allnbytes);
                        fOut << req;
                        memset(req, '\0', sizeof (req));
                    }
                    fOut.close();
                }
            }
            else
            {
                int request_length = snprintf(request, MAX_REQ_LENGTH, get_template, objpath, host);
                if (request_length >= MAX_REQ_LENGTH)
                {
                    fprintf(stderr, "request length large: %d\n", request_length);
                    exit(EXIT_FAILURE);
                }

                //Open Socket for requested server
                if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                {
                    printf("Error: could not create socket \n");
                    exit(EXIT_FAILURE);
                }

                //retrieve host server
                hostent = gethostbyname(host);
                if(hostent == NULL)
                {
                    printf("Error: Unable to get host by name\n");
                    exit(EXIT_FAILURE);
                }

                // setup address
                in_addr = inet_addr(inet_ntoa(*(struct in_addr*)*(hostent->h_addr_list)));
                if (in_addr == (in_addr_t)-1)
                {
                    fprintf(stderr, "error: inet_addr(\"%s\")\n", *(hostent->h_addr_list));
                    exit(EXIT_FAILURE);
                }
                host_addr.sin_addr.s_addr = in_addr;
                host_addr.sin_family = AF_INET;
                host_addr.sin_port = htons(80);
                printf("setupaddress\n");

                // connect with requested server
                if (connect (sockfd, (struct sockaddr *)&host_addr, sizeof (host_addr)) < 0)
                {
                    printf ("Error : Connect Failed \n");
                    return 1;
                }
                printf("connected\n");

                // send http request
                allnbytes = 0;
                while (allnbytes < request_length) 
                {
                    finalnbyte = write(sockfd, request + allnbytes, request_length - allnbytes);
                    if (finalnbyte == -1)
                    {
                        perror("write");
                        exit(EXIT_FAILURE);
                    }
                    allnbytes += finalnbyte;
                }
                printf("sent request\n");
                string fname = retrieveFilename(cache);
                fOut.open(fname);
                savePage(cache, objpath, fname);

                // print response back to browser
                while ((allnbytes = read(sockfd, req, BUFF_SIZE - 1)) > 0)
                {
                    write(acc, req, allnbytes);
                    fOut << req;
                    memset(req, '\0', sizeof (req));
                }
                fOut.close();
            }
        }
        close(sockfd);
    }
    close (listenfd);
    return 0;
}


string retrieveDate()
{
    time_t t;
    struct tm * date;
    char buff[80];
    time (&t);
    date = localtime(&t);
    strftime(buff,sizeof(buff),"%a, %d %b %Y %T GMT",date);
    string theDate(buff);
    return theDate;
}

string retrieveFilename(const vector<CACHEDATA> &cache)
{
    string cName = "cache";
    cName += to_string(cache.size() + 1);
    return cName;
}

int checkCache(const vector<CACHEDATA> &cache, string filepath)
{
    for (int i = 0; i < cache.size(); i++)
        if (cache[i].URL == filepath)
            return i;
    return -1;
}


void savePage(vector<CACHEDATA> &cache, string filepath, string filename)
{
    CACHEDATA myCache;
    myCache.filename = filename;
    myCache.URL = filepath;
    myCache.date = retrieveDate();
    cache.push_back(myCache);
}


