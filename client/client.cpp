/*
** client.c - a stream socket client demo
*/
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define DEFAULT_PORT "1360" // the port client will be connecting to
#define MAXDATASIZE 100 // max number of bytes we can get at once
using namespace std;


void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/**
* to send all parts
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

int get_head(char* s,char* parts[],bool get){
	char* head=s;
	int j=0;
	while(*s!='\n'){
		s++;
		j++;
	}	
	char* len = s+1;
	*s='\0';
	j++;
	parts[0]=head;
	char* k =len;
	if(get==true){
	  while(*k!='\r'){
		k++;
		j++;
	   }
	   j++;
	   parts[2]=k+2;
	   *s='\0';
	   parts[1]=len;
	   j++;
	}
	
	return j;
}

/**
* to send all  
*/
int sendString(int socket,string  sent)
{
    int n = sent.length();
    char sendbuffer[n];
    strcpy(sendbuffer, sent.c_str());
    return sendall(socket,sendbuffer,&n);
}

/**
* break the commmand
*/
int simple_tokenizer(string s,string arr[])
{
    stringstream ssin(s);
    int i=0;
    while (ssin.good() && i < 4){
        ssin >> arr[i];
        ++i;
    }
    return i;
}

/**
*recive all the incoming requests
*/
void recivePOST(int sockfd){
        int numbytes=0;
        char recivebuf[MAXDATASIZE];
    
        if ( (numbytes=recv(sockfd, recivebuf, MAXDATASIZE-1, 0))  == -1) {
            perror("recv");
            exit(1);
        }

        recivebuf[numbytes] = '\0';
        printf("Received: %s",recivebuf);
            
}


/**
* recieve ll the file data 
*/
void reciveGET(int sockfd,string filepath){
        int numbytes=0;
        char recivebuf[MAXDATASIZE];
    
        if ((numbytes=recv(sockfd, recivebuf, MAXDATASIZE, 0)) == -1) {
            perror("recv");
            exit(1);
        }
	
	    char* parts[3];
     	int j=get_head(recivebuf,parts,true);
        string arr[4];
	    string s(parts[0]);
        simple_tokenizer(s,arr);

        if(arr[1]=="200"){
            int length=stoi(parts[1]);
            string file=getFileName(filepath);
            ofstream out(file);
  	        if(! out)
  	        {  
    		    cout<<"Cannot open output file\n";
  	        }

            out.write((char *)parts[2],MAXDATASIZE-j);
            length=length-(MAXDATASIZE-j);
            while( length>0){
                numbytes=recv(sockfd, recivebuf, MAXDATASIZE, 0);
                length=length-MAXDATASIZE;
                if (length<=0){
		           
                    out.write((char *)recivebuf,MAXDATASIZE+length);
		        }
                else
                out.write((char *)recivebuf,sizeof(recivebuf));
                
            }
            out.close();

        }
}


/**
* file adding tho the request
*/
/*
char * add_body(string filePath,int *len,string header){

    string file= getFileName(filePath);
    ifstream ifs(file, ios::binary|ios::ate);
    ifstream::pos_type pos = ifs.tellg();
    int length = pos;
    char *pChars = new char[length];
    ifs.seekg(0, ios::beg);
    ifs.read(pChars, length);
    *len=length;
    ifs.close();
    return pChars;
}*/

/**
* this function constuct the http_message from a series of commands
*/
int construct_http_and_send(int socket ,string arr[]){
    string bulid;
    string type;
    if(arr[0] == "client_get"){
        type="GET";
    }else{
        type="POST";
    }
    string host="Host: "+arr[2]+"\r\n";
    string connect="Connection: keep-alive\r\n";
    bulid=type+" "+arr[1]+" "+"HTTP/1.1\r\n"+host+connect;
    int length=0;
    if (type=="POST"){
        string file= getFileName(arr[1]);
        ifstream ifs(file, ios::binary|ios::ate);
        ifstream::pos_type pos = ifs.tellg();
        length = pos;
        char *pChars = new char[length];
        ifs.seekg(0, ios::beg);
        ifs.read(pChars, length);
        ifs.close();


        bulid=bulid+"content-length:"+to_string(length)+"\r\n"+"\r\n";
    	length = length + bulid.length();
    	char headerbuff[length];
    	strcpy(headerbuff,bulid.c_str());
    	char* response = strcat(headerbuff,pChars);
        return sendall(socket,response,&length);


    }else{
	  bulid=bulid+"\r\n";
	}
	
    return sendString(socket,bulid);
}




/**
* the main function to handle all the requsets
*/
int main(int argc, char *argv[])
{

    int sockfd=-5, numbytes;  
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    ///////////////////////////////////////////////////////////
    fstream command;
    command.open("tpoint.txt",ios::in); //open a file to perform read operation using file objec
    if (command.is_open()){   //checking whether the file is open
    string tp;
    string arr[4];
	arr[0]="";
	arr[1]="";
	arr[2]="";
	arr[3]="";
	string host;
	string port;

    while(getline(command, tp)){ 

		cout << tp << "\n";
		int taken=simple_tokenizer(tp,arr);
		if(arr[2]!=host && arr[3]!=port){
        if(sockfd!=-5)
            close(sockfd);
		cout << "new connection" <<endl;
	    	// create the connection 
    	struct addrinfo *servinfo, *p;
    	int rv;
    	char s[INET6_ADDRSTRLEN];
		if (taken==4)
			port=arr[3];
		else
			port=DEFAULT_PORT;
		    host=arr[2];

		/* convert from strin to char array*/
		    int pl = port.length();
    		char prt[pl+1];
    		strcpy(prt, port.c_str());
		/* convert from strin to char array*/
		    int hl = host.length();
    		char hos[hl+1];
    		strcpy(hos, host.c_str());

    		if ((rv = getaddrinfo(hos, prt, &hints, &servinfo)) != 0) {
        		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        		return 1;
    		}
    		// loop through all the results and connect to the first we can
    		for(p = servinfo; p != NULL; p = p->ai_next) {
        		if ((sockfd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) {
            		perror("client: socket");
            		continue;
        		}
        		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            			close(sockfd);
            			perror("client: connect");
            			continue;
        		}
        			break;
    			}
    			if (p == NULL) {
        			fprintf(stderr, "client: failed to connect\n");
        			return 2;
   	    		}
    			inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),s, sizeof s);
    			printf("client: connecting to %s\n", s);
    			freeaddrinfo(servinfo); // all done with this structure
  		}
            //read data from file object and put it into string.
            // parse the array and make the http header

            		if( construct_http_and_send(sockfd,arr) == -1){
                		exit(1);
            		}

            		if(arr[0]=="client_get"){

                		reciveGET(sockfd,arr[1]);
            		}else{
               		 recivePOST(sockfd);

            		}
        }

     close(sockfd);

     command.close(); //close the file object.
   }
    return 0;
}