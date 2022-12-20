/*
** server.c - a stream socket server demo
*/
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <poll.h>
#include <sys/mman.h>
#include <signal.h>
using namespace std;
#define MAXDATASIZE 100000
#define DEFAULT_PORT "1360" // the port users will be connecting to
#define BACKLOG 10 // how many pending connections queue will hold
#define TIMEOUT 10000
   // Start off with room for 5 connections
    // (We'll realloc as necessary)
    int* fd_count = (int*) mmap(NULL, sizeof (int) , PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    int fd_size = 5;
    struct pollfd *pfds =(struct pollfd *) mmap(NULL, sizeof *pfds * fd_size , PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
// get sockaddr, IPv4 or IPv6:
/**
 * @brief Get the in addr object
 * 
 * @param sa sockaddress
 * @return void* 
 */

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
string getFileName(string filePath){
   
    stringstream ssin(filePath);
    int i=0;

    string segment;
    string last="";

    while(std::getline(ssin, segment, '/'))
    {
        last=segment;
    }
    return last;
}
/**
 * @brief  Add a new file descriptor to the set
 * 
 * @param pfds set of polls
 * @param newfd new file descriptor
 * @param fd_count number of polls
 * @param fd_size file discreptor size
 */
void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
{
    // If we don't have room, add more space in the pfds array
    int old_fd = *fd_size;
    if (*fd_count == *fd_size) {
        *fd_size *= 2; // Double it

        *pfds =(struct pollfd *) mremap(*pfds, sizeof(**pfds) * (old_fd),sizeof(**pfds) * (*fd_size),MREMAP_MAYMOVE);
    }

    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN; // Check ready-to-read

    (*fd_count)++;
}

/**
 * @brief Remove an index from the set
 * 
 * @param pfds  set of polls
 * @param i index of the poll
 * @param fd_count number of polls
 */

void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
    // Copy the one from the end over this one
    pfds[i] = pfds[*fd_count-1];

    (*fd_count)--;
}

/**
 * @brief construct a HTTP header
 * 
 * @param status if 1 then return ok 200 header if 0 return not found 404
 * @param dataLength length of the data put it as zero for post requests
 * @return string of the http request
 */
string construct_header(int status,int dataLength = 0){
    if (status == 1 && dataLength > 0){
    return "HTTP/1.1 200 OK\r\n"+to_string(dataLength)+"\r\n";
    }
    else if(status == 1){
        return "HTTP/1.1 200 OK\r\n";
    }
    else{
        return "HTTP/1.1 404 NOT FOUND\r\n";
    }
}

vector<string> simple_tokenizer(string s,string delim, int limit){
    vector<string> seglist;
    size_t pos = 0; 
    string token1;
    int i = 0;
   while (( pos = s.find (delim)) != std::string::npos && i<limit)  
    {  
        token1 = s.substr(0, pos); // store the substring   
        seglist.push_back(token1);
        // cout << token1 << endl;  
        s.erase(0, pos + delim.length());  /* erase() function store the current positon and move to next token. */ 
        i++;  
        }  
        //cout << given_str << endl; // it print last token of the string.  
    seglist.push_back(s);
    return seglist;
}  


 int get_head(char* s,char* parts[],int *len){
    int k=0;
    int z=3;
    int j=0;
    char* head=s;
    while(k<z){
	    while(*s!='\n'){
		    s++;
            j++;
	    }
        char* part = s+1;
	    *s='\0';
	    parts[k]=head;
        s=part;
        head=part;
        j++;
        k++;
        if (k==1){
            string head(parts[0]);
            vector<string>arr =simple_tokenizer(head," ",1);
            if(*arr.begin() == "POST"){
                z=4;
            }
        }   
    }	
    while(*s!='\n'){
		s++;
        j++;
	}
    s++;
    j++;
    parts[4]=s;
    *len=j;
	return z;
}
/**
 * @brief send all bytes
 * 
 * @param s file descriptor
 * @param buf buffer holding the data
 * @param len length of the data
 * @return int 
 */
int sendall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}

/**
 * @brief handles get requests
 * 
 * @param path path of the file from get request
 * @param read int to read length of the returned message into
 * @return char* the constructed response for http get
 */
char* handle_get(string path, int * read){

     ifstream ifs(path, ios::binary|ios::ate);
     if(!ifs.is_open()){
        string header = construct_header(0);
        char headerbuff[header.length()];
        char* response = strcpy(headerbuff,header.c_str());
        *read = header.length();
        return response;
     }
    ifstream::pos_type pos = ifs.tellg();
    int length = pos;

    char *pChars = new char[length];
    ifs.seekg(0, ios::beg);
    ifs.read(pChars, length);

    ifs.close();
    string header =construct_header(1,length);
    *read = length + header.length();
    char headerbuff[*read];
    strcpy(headerbuff,header.c_str());
    char* response = strcat(headerbuff,pChars);
    //printf("%s", response);

    return response;
}

char* handle_post(string path,int sockfd ,char* body,int lenofbody,int length){
    string name = getFileName(path);
    fstream newfile;
    int len=length;
    int numbytes=0;
    char recivebuf[MAXDATASIZE];
    newfile.open(name,ios::out); //open a file to perform read operation using file object
    if (newfile.is_open()){   //checking whether the file is open
        newfile.write(body,lenofbody);
        while( len >0){
                numbytes=recv(sockfd, recivebuf, MAXDATASIZE, 0);
                len=len-numbytes;
                newfile.write((char *)recivebuf,numbytes);
        }   
    newfile.close(); //close the file object.
    char * response;
    return strcpy(response,construct_header(1).c_str());
   }
   else{
    printf("file not found");
    char * response;
    return strcpy(response,construct_header(0).c_str());
   }
}
/**
 * @brief recieve from client
 * 
 * @param new_fd file descriptor to recieve on
 * @param HTTP_req buffer to recieve in
 * @param size buffer size
 * @return string of the message recieved
 */
string recieve(int new_fd,char * HTTP_req, int size){
    int numbbytes;
    if(numbbytes = recv(new_fd, HTTP_req, size, 0) == -1){
        perror("recv header");
        exit(1);
    }
    //printf ("Recieved : %s", HTTP_req);
    string message(HTTP_req);
    return message;
}

/**
 * @brief send response back to client
 * 
 * @param new_fd file descriptor
 * @param responsebuffer buffer holding response
 * @param n length of the data
 */
void send_response(int new_fd,char * responsebuffer, int * n){
    if (sendall(new_fd, responsebuffer, n) == -1){
            perror("send");
            exit(1);
        }
    printf("sent\n");
}

/**
 * @brief reap dead processes
 * 
 */
void sigchld_handler(int signal_number)
{
    (*fd_count)--;
}


int run_server(int argc,char* argv[]){

    signal(SIGCHLD,sigchld_handler);
    int main_listener, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage client_info; // client's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char remoteIP[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

 
 if ((rv = getaddrinfo(NULL, DEFAULT_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
  // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((main_listener = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(main_listener, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(main_listener, p->ai_addr, p->ai_addrlen) == -1) {
            close(main_listener);
            perror("server: bind");
            continue;
        }

        break;
    }

     freeaddrinfo(servinfo); // all done with this structure
    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(main_listener, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
    /**got the listener**/

    // Add the listener to set
    pfds[0].fd = main_listener;
    pfds[0].events = POLLIN; // Report ready to read on incoming connection

    *fd_count = 0; // For the listener

    while(1) { // main accept() loop

                sin_size = sizeof client_info;
                new_fd = accept(main_listener,(struct sockaddr *)&client_info,&sin_size);

                if (new_fd == -1) {
                    perror("accept");
                    continue;
                }

                printf("pollserver: new connection from %s on ""socket %d\n",
                inet_ntop(client_info.ss_family,get_in_addr((struct sockaddr*)&client_info),remoteIP, INET6_ADDRSTRLEN),new_fd);

                int pid = fork();
                if(pid == 0){
                    (*fd_count)  ++;
                    printf("%d fd count from child \n",*fd_count);
                    close(main_listener);
                    struct pollfd ufds[1];
                    ufds[0].fd = new_fd;
                    ufds[0].events = POLLIN;
                    while(1){
                        int eve=poll(ufds,1,TIMEOUT/(*fd_count));
                        if (eve!=-1 && eve!=0){
                        char HTTP_req[MAXDATASIZE];
                        char* newBody;
                        int numbbytes;
                        if((numbbytes = recv(new_fd , HTTP_req, MAXDATASIZE, 0)) == -1){
                            perror("recv header");
                            break;
                        }
                        printf ("Recieved : %s", HTTP_req);

                        // We got some good data from a client
                        char* arguments[5];
                        int rest=0;
                        int arguments_len=get_head(HTTP_req,arguments,&rest);

                        if(arguments_len == 3){

                            string header(arguments[0]);
                            vector<string>arr = simple_tokenizer(header, " ",3);
                            int len = 0;
                            newBody = handle_get(*(arr.begin()+1), &len);
                            send_response(new_fd, newBody,&len);
                        }
                        else if(arguments_len==4){
                            string header(arguments[0]);
                            vector<string>arr = simple_tokenizer(header, " ",3);
                            string contentlength(arguments[3]);
                            vector<string>arr2 = simple_tokenizer(contentlength,":",2);

                            int len = stoi(*(arr2.begin()+1));
                            newBody = handle_post(*(arr.begin()+1),new_fd,arguments[4],rest,len-rest);
                            int l = 20;
                            send_response(new_fd, newBody,&l);
                        }
                        else{
                            if(send(new_fd, "error parsing HTTP protocol", 1000,0) == -1){
                                 perror("send");
                                    break;
                            }    
                        }
                    }
                    else if(eve==0){
                        cout << "time out" << endl;
                        close(new_fd);
                        kill(getpid(), SIGINT);
                    }else{
                        perror("poll"); // error occurred in poll()
                    }
                  }
                }
                close(new_fd);
    }
    return 0;
}

int main(int argc,char *argv[])
{
    run_server(argc,argv);
    return 0;
}