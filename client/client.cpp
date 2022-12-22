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

/**
 * @brief Get the in addr object , IPv4 or IPv6:
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
 * @brief get the file name to read it
 * 
 * @param filePath the path to the file
 * @return string the file name 
 */
string getFileName(string filePath){
   
    stringstream ssin(filePath); // open string stram
    int i=0;
    string segment;
    string last="";
    while(std::getline(ssin, segment, '/'))  // break at teh '\' sign to get the file name
    {
        last=segment;
    }
    return last;
}

/**
 * @brief get the file name to read it
 * 
 * @param filePath the path to the file
 * @return string the file name 
 */
int get_head(char* s,char* parts[]){
	char* head=s; // hold the first line in the request
	int j=0;
	while(*s!='\n'){ // move untill you find the new line client
		s++;
		j++;
	}	
	char* len = s+1; // hold the second in the header
	*s='\0'; // add delimtar to break the header
	j++;
	parts[0]=head;// hold the first line in the request
	char* k =len;
	while(*k!='\r'){
		k++;
		j++;
	}
	j++;
	parts[2]=k+2; // hold the rest of the line in the request
	*s='\0'; // add delimtar to break the header
	parts[1]=len; // hold the second line in the request
	j++;
	
	return j;
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
        // cout << token1 << endl;  
        s.erase(0, pos + delim.length());  /* erase() function store the current positon and move to next token. */ 
        i++;  
        }  
        //cout << given_str << endl; // it print last token of the string.  
    seglist.push_back(s);
    return seglist;
}  


/**
 * @brief sent the string of the header
 * 
 * @param socket the number of the socket
 * @param sent the sented header
 * @return number -1 if fail or 1 if not
 */
int sendString(int socket,string  sent)
{
    int n = sent.length()+1;
    char sendbuffer[n];
    strcpy(sendbuffer, sent.c_str());
    return sendall(socket,sendbuffer,&n);
}

/**
 * @brief break the readed command from command file and parser it
 * 
 * @param s the command
 * @param arr to hold the arguments
 * @return number of the arguments
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
 * @brief recive the post reponsed
 * 
 * @param socket the number of the socket
 */
int recivePOST(int sockfd){
        int numbytes=0;  // number of reived bytes
        char recivebuf[MAXDATASIZE]; // the recived buffer
       
        if ( (numbytes=recv(sockfd, recivebuf, MAXDATASIZE-1, 0))  == -1) {
            // this part is in case of error
            perror("recv");
            exit(1);
        }
        recivebuf[numbytes] = '\0';
        vector<string>arr = simple_tokenizer(recivebuf," ",3);
        if ( (*(arr.begin()+1) )== "200" )
            return 1;

        return -1;
}

/**
 * @brief recive the get reponsed
 * 
 * @param socket the number of the socket
 * @param filepath the path to write the file
 */
void reciveGET(int sockfd,string filepath){
        int numbytes=0; // number of reived bytes
        char recivebuf[MAXDATASIZE];// the recived buffer
    
        if ((numbytes=recv(sockfd, recivebuf, MAXDATASIZE, 0)) == -1) {
            // this part is in case of error
            perror("recv");
            exit(1);
        }
	
	    char* parts[3];
     	int j=get_head(recivebuf,parts);
        string arr[4];
	    string s(parts[0]);
        simple_tokenizer(s,arr);
        if(arr[1]=="200"){ // in case the request is ok
            string contLength(parts[1]);
            vector<string>arr = simple_tokenizer(parts[1],":",2);
            int length=stoi( *(arr.begin()+1) ); // length of the sended buffer
            string file=getFileName(filepath); // get the file path 
            ofstream out(file); // opern the file
  	        if(! out) //to handle the file doesnt open 
  	        {  
    		    cout<<"Cannot open output file\n";
                exit(-1);
  	        }
            if(length > 0){
                out.write((char *)parts[2],MAXDATASIZE-j); // write the rest of the file bart
                length=length-(MAXDATASIZE-j);// the length of the 
                while( length>0){ // if the file does not recive it all
                    numbytes=recv(sockfd, recivebuf, MAXDATASIZE, 0);// the numebr of the recived byte + fill the buffer with the recive data
                    length=length-numbytes;// decrease the length of the length
                    out.write((char *)recivebuf,numbytes);// write the recived buffer
                }
            }

            out.close(); // close the file
            

        }
}

/**
 * @brief parse and sent the file
 * 
 * @param socket the number of the socket
 * @param arr[] the sent parsed argument
 */
int construct_http_and_send(int socket ,string arr[]){
    string bulid; // string to hold the bulid
    string type; // the type GET| POST
    if(arr[0] == "client_get"){ // check of teh request is get
        type="GET";
    }else{
        type="POST";
    }
    string host="Host: "+arr[2]+"\r\n"; // append the host name
    string connect="Connection: keep-alive\r\n"; // type of connection
    bulid=type+" "+arr[1]+" "+"HTTP/1.1\r\n"+host+connect; //add the rest data filepath and type of http
    int length=0; // length of the file
    if (type=="POST"){

        ifstream ifs(arr[1], ios::binary|ios::ate);
        ifstream::pos_type pos = ifs.tellg();
        length = pos;
        char *pChars = new char[length];
        ifs.seekg(0, ios::beg);
        ifs.read(pChars, length);
        ifs.close();
        bulid=bulid+"content-length:"+to_string(length)+"\r\n"+"\r\n";
        int ok=-1;
        if (sendString(socket,bulid) != -1 ){ // send the header
              
                 ok=recivePOST(socket);
        }
        if(ok!=-1){ 
            return sendall(socket,pChars,&length);
        }
        return ok;

    }else{
	  bulid=bulid+"\r\n"; // check of teh request is get
	}
	
    return sendString(socket,bulid);
}




/**
* the main function to handle all the requsets
*/
int main(int argc, char *argv[])
{

    int sockfd=-5, numbytes;  
    struct addrinfo hints; // address info hint to set teh type of connection
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // family ipv4
    hints.ai_socktype = SOCK_STREAM; // socket stream for tcp connection

    if(argc<2){
        printf("no command file entred");
        exit(1);
    }
    fstream command;// intial the file 
    command.open(argv[1],ios::in); //open a file to perform read operation using file objec
    if (command.is_open()){ 
        //checking whether the file is open
    string tp;  // the readed command 
    string arr[4]; // string array to hold the 
    // initalize the array
	arr[0]="";
	arr[1]="";
	arr[2]="";
	arr[3]="";
	string host="";// the host that will be will be sent in connection pasred from commmand 
	string port="";// the host that port that will be sent in connection pasred from commmand 
 
    while(getline(command, tp)){  // read file in lines
    cout << tp << endl;
		int taken=simple_tokenizer(tp,arr);
  // parse the command and get the number of element sent in it
		if(arr[2]!=host || !(arr[3]==port || (arr[3].length()==0 && port=="1360"))  ){  // if the command wants a new host and port then start new connection
            if(sockfd!=-5) // if the inital socket
                close(sockfd); // close the last socket
		cout << "new connection" <<endl;
	    	// create the connection 
    	struct addrinfo *servinfo, *p;
    	int rv;
    	char s[INET6_ADDRSTRLEN];
		if (arr[3].length()>0){
			port=arr[3];
        }
		else{
            string temp(DEFAULT_PORT);
			port=temp;
        }
		host=arr[2];
		// convert from strin to char array
		    int pl = port.length();
    		char prt[pl+1];
    		strcpy(prt, port.c_str());
		// convert from strin to char array
		    int hl = host.length();
    		char hos[hl+1];
    		strcpy(hos, host.c_str());

    		if ((rv = getaddrinfo(hos, prt, &hints, &servinfo)) != 0) { // get the address information of the sended host
        		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        		return 1;
    		}
    		// loop through all the results and connect to the first we can
    		for(p = servinfo; p != NULL; p = p->ai_next) {
        		if ((sockfd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) {// if error occueres during making socket
            		perror("client: socket");
            		continue;
        		}
        		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) { // if error occueres during connection
            			close(sockfd);// close the socket
            			perror("client: connect");
            			continue;
        		}
        			break;
    			} 
    			if (p == NULL) { // in case no connection found
        			fprintf(stderr, "client: failed to connect\n");
        			return 0;
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
            		}
                    sleep(1);// not to overwhelm the server
        }

     close(sockfd); // close the socket

     command.close(); //close the file object.
   }
    return 0;
}