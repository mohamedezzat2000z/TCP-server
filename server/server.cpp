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
#define MAXDATASIZE 1000000 // maximum data size of the server's buffer
#define DEFAULT_PORT "1360" // the port users will be connecting to
#define TIMEOUT 10000// server base time out for each process
    // allocate number of currently spawned processes mapped to all processes to determine timeout heuristic
    int* fd_count = (int*) mmap(NULL, sizeof (int) , PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    //pollfd data structure used to store fd for polling   
    struct pollfd *pfds =(struct pollfd *) mmap(NULL, sizeof *pfds * fd_size , PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
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
/**
 * @brief Get the File Name object
 * 
 * @param filePath path to extract file name from
 * @return string file name
 */
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
 * @brief construct a HTTP header
 * 
 * @param status if 1 then return ok 200 header if 0 return not found 404
 * @param dataLength length of the data put it as zero for post requests
 * @return string of the http request
 */
string construct_header(int status,int dataLength = 0){
    if (status == 1 && dataLength > 0){
    return "HTTP/1.1 200 OK\r\n" +string("content-length:")+to_string(dataLength)+"\r\n";
    }
    else if(status == 1){
        return "HTTP/1.1 200 OK\r\n";
    }
    else{
        return "HTTP/1.1 404 NOT FOUND\r\n";
    }
}
/**
 * @brief tokenize a string into a vector of strings
 * 
 * @param s string to be tokenized
 * @param delim string delimiter to tokenize around
 * @param limit tokenization limit (how many slices)
 * @return vector<string> vector of the sliced string
 */
vector<string> simple_tokenizer(string s,string delim, int limit){
    vector<string> seglist;
    size_t pos = 0; 
    string token1;
    int i = 0;
   while (( pos = s.find (delim)) != std::string::npos && i<limit)  
    {  
        token1 = s.substr(0, pos); // store the substring   
        seglist.push_back(token1);
       
        s.erase(0, pos + delim.length());  /* erase() function store the current positon and move to next token. */ 
        i++;  
        }  

    seglist.push_back(s);
    return seglist;
}  

/**
 * @brief send all bytes
 * 
 * @param s file descriptor
 * @param buf buffer holding the data
 * @param len length of the data
 * @return int -1 on failure and 0 on success
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
    char * response = (char *) malloc(sizeof(char)*(*read));
    char * h = response;
    int i = 0;
    while(headerbuff[i] != '\0'){
        *h = headerbuff[i];
        i++;
        h++;
    }
    char * b = pChars;
    while (i < (*read))
    {
        *h = *b;
        i++;
        b++; 
        h++;
    }

    return response;
}
/**
 * @brief function to handle incoming post requests from client
 * 
 * @param path path of the file to be written
 * @param sockfd socket file descriptor
 * @param length length of the file
 */
void handle_post(string path,int sockfd,int length){
    cout << path <<endl;
    string file=getFileName(path);
    fstream newfile;
    int len=length;
    int numbytes=0;
    char recivebuf[MAXDATASIZE];
    newfile.open(file,ios::out); //open a file to perform read operation using file object
    if (newfile.is_open()){//checking whether the file is open
        int ren=18;
        char response[ren];
        strcpy(response,construct_header(1).c_str());
        char * r = strcat(response, "");
        send_response(sockfd, r,&ren);
        while( len >0){
               numbytes=recv(sockfd, recivebuf, MAXDATASIZE, 0);
                len=len-numbytes;
                newfile.write((char *)recivebuf,numbytes);
        }  
 
    newfile.close(); //close the file object.
   }
   else{
    printf("file not found");
    int ren=25;
    char response[ren];
    strcpy(response,construct_header(0).c_str());
    char * r = strcat(response, "");
    send_response(sockfd, r,&ren);
   }
}

/**
 * @brief reap dead processes
 * 
 */
void sigchld_handler(int signal_number)
{
    (*fd_count)--;
    cout << "dead client"<<endl;
}

/**
 * @brief main function to run the server
 * 
 * @param argc argument count
 * @param argv argument array
 * @return int 0
 */
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
    char * port = DEFAULT_PORT;//socket port
    if(argc == 2){
        port = argv[1];
    }
 if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
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
                    close(main_listener);
                    struct pollfd ufds[1];
                    ufds[0].fd = new_fd;
                    ufds[0].events = POLLIN;
                    int numbbytes; 
                    while(1){
                        int eve=poll(ufds,1,TIMEOUT/(*fd_count));
                        if (eve!=-1 && eve!=0){
                        char HTTP_req[MAXDATASIZE];
                        char* newBody; 
                        if((numbbytes = recv(new_fd , HTTP_req, MAXDATASIZE, 0)) == -1){
                            perror("recv header");
                            break;
                        }

                        HTTP_req[numbbytes] = '\0';
                        string header(HTTP_req);
                        vector<string>arr = simple_tokenizer(header,"\r\n",4);
                     
                        vector<string>arr2 = simple_tokenizer(*(arr.begin()), " ",3);
                        if((*(arr2.begin())) == "GET"){
                            int len = 0;
                            newBody = handle_get(*(arr2.begin()+1), &len);
                            send_response(new_fd, newBody,&len);
                        }
                        else if((*(arr2.begin()) )== "POST"){
                        
                            vector<string>arr3 = simple_tokenizer((*(arr.begin()+3) ),":",2);
                            int len = stoi(*(arr3.begin()+1));
                            handle_post(*(arr2.begin()+1),new_fd,len);
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