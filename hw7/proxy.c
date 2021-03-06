#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_LINE 1023

struct cache_block{
    char url[MAX_LINE]; // cached url
    char response[MAX_OBJECT_SIZE]; // the cached http response
    int timestamp; // last operation timestamp
    int readcnt;
    int write_request;
    sem_t mutex, w;
}cache_block;

/* methods */
void *thread(void *vargp);
int parse_url(char *url, char *hostname, char *port, char *query);
int connect_server(char *url, char *hostname, char *port, char *query);
void cache_init();
int cache_read(char *url, char *response, int timestamp);
void cache_write(char *response, char *url, int timestamp);
int read_http_url(int connfd, char *method, char *url, char *version);
int process_http_response(int clientfd, int connfd);

/* You won't lose style points for including these long lines in your code */
static char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";
static char *connection_hdr = "Connection: close\r\n";
static char *proxy_connection_hdr = "Proxy-Connection: close\r\n";

/* how many cache blocks */
static int cache_cnt;
/* pointer to caches */
static struct cache_block *caches;
/* global cnt to log how many request received, used for cache eviction */
int cur_time = 0;


/* main method */
int main(int argc, char **argv)
{
    int listenfd, clientlen, connfd;
    char *port;
    struct sockaddr_in clientaddr;
    pthread_t tid;
    printf("%s%s%s", user_agent_hdr, accept_hdr, accept_encoding_hdr);
    
    /* init cache blocks */
    cache_cnt = MAX_CACHE_SIZE / MAX_OBJECT_SIZE;
    cache_init();
    
    /* check port */
    if (argc != 2){
        exit(0);
    }
    port = argv[1];
    
    /* listen connection */
    listenfd = Open_listenfd(port);
    
    /* infinite loop for client connection */
    while(1) {
        /* accept connection */
		cur_time++;
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA*)(&clientaddr), (socklen_t *)(&clientlen));
        /* create new thread to handle the connection */
        Pthread_create(&tid, NULL, thread, (void *)&connfd);
    }
    return 0;
}

/* thread for one connection */
void *thread(void *vargp){
	/*
	* timestamp indicates when this request received, help cache eviction
	* race may happen, timestamp didn't log the true time
	* but not strict LRU, I ignore the issue
	*/
    int timestamp = cur_time; 
    int connfd = *((int *)vargp);
    rio_t rio;
    char method[MAX_LINE], version[MAX_LINE], url[MAX_LINE];
    char host[MAX_LINE], port[MAX_LINE], query[MAX_LINE];
    char *response;
    int clientfd;
	
	/* read url, if failed close connection */
	if (read_http_url(connfd, method, url, version) == 0){
		Close(connfd);
		return NULL;
	}
	
    /* cache hit */
    response=(char*)Malloc(MAX_OBJECT_SIZE);
    response[0]='\0';
    if (cache_read(url, response, timestamp)){
        rio_writen(connfd, response, strlen(response));
        Close(connfd);
        free(response);
        return NULL;
    }

    /* cache miss, create http request to real server*/
    /* parse url */
	parse_url(url, host, port, query);

    /* connect server failed */
    if((clientfd = connect_server(url, host, port, query)) < 0){
        Close(connfd);
        return NULL;
    }
	
	/* read response from server and send to client */
	process_http_response(clientfd, connfd);
	Close(clientfd);
    Close(connfd);
	return NULL;
}

/*
 * read response from real server
 * determine if need to add to cache,
 * send response back to client
 */
int process_http_response(int clientfd, int connfd){
    char *response = (char*)Malloc(MAX_OBJECT_SIZE);
	char buf[MAX_LINE+1];
	int content_length;
	int count;
	
	/* read http response head, store content length */
    Rio_readinitb(&rio, clientfd);
    content_length = 0;
    while ((count = Rio_readlineb(&rio, buf, MAX_LINE)) > 0){
        Rio_writen(connfd, buf, count);
        strncat(response, buf, count);
        if(strstr(buf, "Content-length")){
            sscanf(buf, "Content-length: %d\r\n", &content_length);
        }
        if (strcmp(buf,"\r\n") == 0){
            break;
        }
    }
    
    /* too big to cache */
    if(content_length + strlen(response) >= MAX_OBJECT_SIZE){
        while ((count = rio_readnb(&rio, buf, MAX_LINE)) > 0){
            rio_writen(connfd, buf, count);
        }
        free(response);
        return 0;
    }
    
    /* add to cache */
    while ((count = rio_readnb(&rio, buf, MAX_LINE)) > 0){
        strncat(response, buf, count);
        rio_writen(connfd, buf, count);
    }
    cache_write(response, url, timestamp);
    free(response);
    return 0;
}

/*
 * method to read data from socket,
 * it store the http request url for later use,
 * and check method is only GET, return 1
 * otherwise, return 0, to indicate a unsupported http request
 */
int read_http_url(int connfd, char *method, char *url, char *version){
    char buf[MAX_LINE+1];
	
	/* read socket */
    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAX_LINE); //read url
    sscanf(buf, "%s %s %s", method, url, version);
	
    /* read remaining request headers */
    while(1){
        Rio_readlineb(&rio, buf, MAX_LINE);
        printf("%s", buf);
        if (!strcmp(buf,"\r\n")){
            break;
        }
    }
    
    /* make sure only GET */
    if(strcasecmp(method, "GET")){
        return 0;
    }
	return 1;
}

/* 
 * parse url
 * return 0 if succeed, else failed
 */
int parse_url(char *url, char *hostname, char *port, char *query){
    char *temp;
    int i = 0;
    int j = 0;
    char c;
    char *pointer;
    int in_query = 0;
    
    /* skip http:// */
    temp = strstr(url, "//");
    if(temp != NULL) {
        temp = temp + 2;
    } else {
        temp = url;
    }

    /* parse */
    pointer = hostname;
    while ((c = temp[i]) != '\0'){
        if (c == ':'){
            pointer = port;
            j = -1;
        }else if ((c == '/') && (in_query == 0)){            
            in_query = 1;
            pointer = query;
            j = 0;
            pointer[0]='/';
        }else{
            pointer[j] = temp[i];
        }
        i++;
        j++;
    }
	
    /* default port */
    if (port[0] == '\0'){
        port[0] = '8';
        port[1] = '0';
    }
    return 0;
}

/* 
 * proxy as a client to connect server,
 * return clientfd, if clientfd < 0, means failed 
 */
int connect_server(char *url, char *hostname, char *port, char *query){
    char buf[MAXLINE];
    int clientfd = open_clientfd(hostname, port);

    /* connection failed */
    if(clientfd < 0)
        return clientfd;

    /* send http request */
    sprintf(buf, "GET %s HTTP/1.0\r\n", query);
    Rio_writen(clientfd, buf, strlen(buf));
    sprintf(buf, "Host: %s\r\n", hostname);
    Rio_writen(clientfd, buf, strlen(buf));
    Rio_writen(clientfd, user_agent_hdr, strlen(user_agent_hdr));
    Rio_writen(clientfd, accept_hdr, strlen(accept_hdr));
    Rio_writen(clientfd, accept_encoding_hdr, strlen(accept_encoding_hdr));
    Rio_writen(clientfd, connection_hdr, strlen(connection_hdr));
    Rio_writen(clientfd, proxy_connection_hdr, strlen(proxy_connection_hdr));
    Rio_writen(clientfd, "\r\n", strlen("\r\n"));
    return clientfd;
}

/* init cache blocks */
void cache_init(){
    int i;
    caches = (struct cache_block *)(malloc(cache_cnt*sizeof(struct cache_block)));
    for (i=0;i<cache_cnt;i++){
        caches[i].timestamp = 0;    
        caches[i].url[0] = '\0';
        caches[i].response[0] = '\0';
        caches[i].readcnt = 0;
        Sem_init(&caches[i].mutex, 0, 1);
        Sem_init(&caches[i].w, 0, 1);
    }
}

/*
 * find a hit in cache
 * if miss, return 0,
 * if hit, fill response with cached data, and return 1
 */
int cache_read(char *url, char *response, int timestamp){
    int i;
    for (i = 0; i<cache_cnt; i++){
        while (caches[i].write_request){
            /* not add new read, let write first */
        }
        
        /* lock to add readcnt*/
        P(&(caches[i].mutex));
        caches[i].readcnt++;
        if (caches[i].readcnt == 1){
            P(&(caches[i].w));
        }
        V(&(caches[i].mutex));
        
        /* hit */
        if (strcmp(url, caches[i].url) == 0){
            /* copy http response */
            strcpy(response, caches[i].response);
            /* update last timestamp, and readcnt--, return */
            P(&(caches[i].mutex));
            caches[i].readcnt--;
            if (timestamp>caches[i].timestamp){
                caches[i].timestamp= timestamp;
            }
            if (caches[i].readcnt == 0){
                V(&(caches[i].w));
            }
            V(&(caches[i].mutex));
            return 1;
        }
        
        /* lock to readcnt-- */
        P(&(caches[i].mutex));
        caches[i].readcnt--;
        if (caches[i].readcnt == 0){
            V(&(caches[i].w));
        }
        V(&(caches[i].mutex));
    }
    return 0;
    
}

/*
 * add new http response to cache
 * using LRU evict policy
 */
void cache_write(char *response, char *url, int timestamp){
    int i;
    int evict;
    int min_timestamp = 0x7fffffff;
    
    /* find the victim, who has smallest timestamp 
     * not strict LRU, because we didn't lock, timestamp may be changed.
     */
    for (i=0;i<cache_cnt;i++){
        if (caches[i].timestamp < min_timestamp){
            min_timestamp = caches[i].timestamp;
            evict = i;
        }
    }
    caches[evict].write_request = 1;
    
    /* write in new http response */
    P(&(caches[evict].w));
    strcpy(caches[evict].response, response);
    strcpy(caches[evict].url, url);
    caches[evict].timestamp = timestamp;
    caches[evict].write_request = 0;
    V(&(caches[evict].w));
}
