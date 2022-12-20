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
    cout << dataLength << endl;
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

string handle_post(string path, string body){
   // string name = getFileName(path);
    fstream newfile;
    //cout << body << "\n";
    newfile.open(path,ios::out); //open a file to perform read operation using file object
    if (newfile.is_open()){   //checking whether the file is open
            newfile.write(body.c_str(),body.length());
            //cout << tp << "\n"; //print the data of the string
        
     newfile.close(); //close the file object.
    return construct_header(1);
   }
   else{
    printf("file not found");
    return construct_header(0);

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
void thread_func(struct pollfd *pfds, int i, int* fd_count){
           
}
/**
 * @brief reap dead processes
 * 
 */
void sigchld_handler()
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}


int run_server(int argc,char* argv[]){
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

    *fd_count = 1; // For the listener

    // sa.sa_handler = sigchld_handler; // reap all dead processes
    // sigemptyset(&sa.sa_mask);
    // sa.sa_flags = SA_RESTART;
    // if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    //     perror("sigaction");
    //     exit(1);
    // }


    while(1) { // main accept() loop
        int poll_count = poll(pfds, *fd_count, 2500);
                                printf("fd_count from parent%d \n",*fd_count);

         for(int i = 0; i < *fd_count; i++) {
            if (pfds[i].revents & POLLIN) { // We got one!!
              if (pfds[i].fd == main_listener) {
                    // If listener is ready to read, handle new connection

                    sin_size = sizeof client_info;
                    new_fd = accept(main_listener,
                        (struct sockaddr *)&client_info,
                        &sin_size);

                    if (new_fd == -1) {
                        perror("accept");
                    } else {
                        add_to_pfds(&pfds, new_fd, fd_count, &fd_size);

                        printf("pollserver: new connection from %s on "
                            "socket %d\n",
                            inet_ntop(client_info.ss_family,
                                get_in_addr((struct sockaddr*)&client_info),
                                remoteIP, INET6_ADDRSTRLEN),
                            new_fd);
                    }
                
            }else{
                int pid = fork();
                if(pid == 0){
                    char HTTP_req[MAXDATASIZE];
                    char* newBody;
                    int client_fd = pfds[i].fd;
                    int numbbytes;
                    if(numbbytes = recv(client_fd, HTTP_req, MAXDATASIZE, 0) == -1){
                    perror("recv header");
                    close(client_fd);
                    exit(0);
                }
                //printf ("Recieved : %s", HTTP_req);
                string str_req(HTTP_req);
                    //cout << str_req <<"\n";
                // We got some good data from a client
                vector<string>arr = simple_tokenizer(str_req, "\r\n",5);
                            // cout << *arr.begin() << endl;
                vector<string> header = simple_tokenizer(*arr.begin()," ",3);
                            // cout << *header.begin() << endl;
                if(*header.begin() == "GET"){
                    int len = 0;
                    newBody = handle_get(*(header.begin()+1), &len);
                    send_response(client_fd, newBody,&len);
                }
                else{
                 if(send(client_fd, "error parsing HTTP protocol", 1000,0) == -1){
                     perror("send");
                        close(client_fd);
                        exit(0);
                 }    
                }
                close(client_fd);
                del_from_pfds(pfds, i, fd_count);
                                    printf("fd_count from child%d",* fd_count);
                exit(1);
            }

                close(pfds[i].fd);
            }
            }
         }
    }
        // if (!fork()) { // this is the child process
            // else if(*header.begin() == "POST"){
                // cout << *(header.begin()+1) << endl;
                // cout << *(arr.begin()+4) << endl;
                // int length = stoi(*(arr.begin()+3));
                // cout << length << endl;
                // string s = "";
                // if(length > MAXDATASIZE){
                //     for(int i = MAXDATASIZE; i < length; i+=MAXDATASIZE){
                //         string s = s+ recieve(new_fd, HTTP_req, MAXDATASIZE);
                //         //cout << i << endl;
                //     }
                // }
                // string k = *(arr.begin()+5) + s;
                // newBody = handle_post(*(header.begin()+1),k);
                
                // if(i){
                //     newHeader = construct_header(1);
                // }
                // newBody = "";
            // }
            // else{
            //     newBody = "";
            //     printf("error HTTP protocol not found");
            // }            
            // }
     
    // }
    return 0;
}

int main(int argc,char *argv[])
{
    run_server(argc,argv);
    return 0;
}