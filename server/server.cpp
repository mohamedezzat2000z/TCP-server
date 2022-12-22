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
#define MAXDATASIZE 10000 // maximum data size of the server's buffer
#define DEFAULT_PORT "1360" // the port users will be connecting to
#define TIMEOUT 10000// server base time out for each process
#define BACKLOG 10 // the maxuiam size of the connected users
    // allocate number of currently spawned processes mapped to all processes to determine timeout heuristic
    int* fd_count = (int*) mmap(NULL, sizeof (int) , PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    int fd_size = 5;
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
string construct_header(int status,int dataLength = -1){
    if (status == 1 && dataLength >= 0){
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
}

/**
 * @brief handles get requests
 * 
 * @param path path of the file from get request
 * @param read int to read length of the returned message into
 * @return char* the constructed response for http get
 */
char* handle_get(string path, int * read){
    
     ifstream ifs(path, ios::binary|ios::ate);//open the file 
     if(!ifs.is_open()){//if it does not exits we send back NOT FOUND
        string header = construct_header(0);//construct an NOT FOUND header
        char headerbuff[header.length()];
        cout << "File Not found!\n" <<endl;
        cout << header << endl;
        char* response = strcpy(headerbuff,header.c_str());
        *read = header.length();
        return response;//return the response
     }
     //if the file is open
    ifstream::pos_type pos = ifs.tellg();
    int length = pos;
    //new array to read data into
    char *pChars = new char[length];
    ifs.seekg(0, ios::beg);
    ifs.read(pChars, length);//read data

    ifs.close();//close file
    string header =construct_header(1,length);//construct an OK header
    cout << "File found!"<< endl;
    cout << header << endl;
    *read = length + header.length();//increment the raw data's length
    char headerbuff[*read];
    strcpy(headerbuff,header.c_str());
    char * response = (char *) malloc(sizeof(char)*(*read));//allocate an new buffer to recieve the whole message
    char * h = response;//pointer to start appending header data
    int i = 0;
    while(headerbuff[i] != '\0'){//append header data
        *h = headerbuff[i];
        i++;
        h++;
    }
    char * b = pChars;//pointer to start appending data itself
    while (i < (*read))
    {//append data
        *h = *b;
        i++;
        b++; 
        h++;
    }
    return response;//return the response back
}
/**
 * @brief function to handle incoming post requests from client
 * 
 * @param path path of the file to be written
 * @param sockfd socket file descriptor
 * @param length length of the file
 */
void handle_post(string path,int sockfd,int length){

    string file=getFileName(path);
    fstream newfile;
    int len=length;
    int numbytes=0;
    char recivebuf[MAXDATASIZE];//reciever buffer
    newfile.open(file,ios::out); //open a file to perform read operation using file object
    if (newfile.is_open()){//checking whether the file is open
        int ren=18;
        char response[ren];
        strcpy(response,construct_header(1).c_str());//construct a header to send back OK to start recieveing the file
        char * r = strcat(response, "");
        send_response(sockfd, r,&ren);//send the OK message
        while( len >0){//recieve back the file contents and write it
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

    char * port;//socket port
    if(argc ==1){//if there is no given port
        string p = DEFAULT_PORT;
        port = new char[p.length()+1];
        strcpy(port, p.c_str());
        printf(" connecting on port%s \n", port);    
        cout << port << endl;      
    }
    else if(argc == 2){//if there is a given port
        port=argv[1];
    }
    //validate port number
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
     //if binding failed
    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }
    // start the listener
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
                //client info
                sin_size = sizeof client_info;
                //accept the new requests
                new_fd = accept(main_listener,(struct sockaddr *)&client_info,&sin_size);
                //if accepting failed
                if (new_fd == -1) {
                    perror("accept");
                    continue;
                }

                printf("pollserver: new connection from %s on ""socket %d\n",
                inet_ntop(client_info.ss_family,get_in_addr((struct sockaddr*)&client_info),remoteIP, INET6_ADDRSTRLEN),new_fd);
                //for a child process to handle the request
                int pid = fork();
                if(pid == 0){//if it is the child process
                    (*fd_count)  ++;//increment number of processes working
                    close(main_listener);//close listener file descriptor as the child does not need it
                    struct pollfd ufds[1];//make a polling ds
                    ufds[0].fd = new_fd;//add the given file descriptor
                    ufds[0].events = POLLIN;//poll it
                    int numbbytes; //integer to store number of bytes
                    while(1){//polling loop
                        int eve=poll(ufds,1,TIMEOUT/(*fd_count));//poll on the socket
                        if (eve!=-1 && eve!=0){// if an event occured
                        char HTTP_req[MAXDATASIZE];
                        char* newBody; 
                        if((numbbytes = recv(new_fd , HTTP_req, MAXDATASIZE, 0)) == -1){//recieve the message on the socket
                            perror("recv header");
                            break;
                        }

                        HTTP_req[numbbytes] = '\0';
                        string header(HTTP_req);//get the string to print
                        cout << header << endl;
                        vector<string>arr = simple_tokenizer(header,"\r\n",4);//parse header for connection data
                     
                        vector<string>arr2 = simple_tokenizer(*(arr.begin()), " ",3);//parse the first line to extract the command
                        if((*(arr2.begin())) == "GET"){//if the command is GET
                            int len = 0;
                            newBody = handle_get(*(arr2.begin()+1), &len);//send to the handle get function
                            send_response(new_fd, newBody,&len);//formulate response and send it back
                            if(len > 24)
                                free(newBody);//free the allocated resources
                        }
                        else if((*(arr2.begin()) )== "POST"){//if the command is POST
                        
                            vector<string>arr3 = simple_tokenizer((*(arr.begin()+3) ),":",2);//parse the last line to get the content's length
                            int len = stoi(*(arr3.begin()+1));//convert the length string to integer
                            handle_post(*(arr2.begin()+1),new_fd,len);// send to handle post function
                        }
                        else{
                            if(send(new_fd, "error parsing HTTP protocol", 1000,0) == -1){//if the header was sent wrong
                                 perror("send");
                                    break;
                            }    
                        }
                    }
                    else if(eve==0){//if the time expired we timeout
                        cout << "time out" << endl;
                        close(new_fd);
                        kill(getpid(), SIGINT);
                    }else{
                        perror("poll"); // error occurred in poll()
                    }
                  }
                }
                close(new_fd);//parent closes the file descriptor to continue listening for new requests
    }
    return 0;
}

int main(int argc,char *argv[])
{
    run_server(argc,argv);
    return 0;
}