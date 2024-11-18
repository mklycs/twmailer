#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <stdlib.h>     // ??? 
#include <signal.h>
#include <string.h>
#include <dirent.h>

#define USERNAME_LEN 8
#define SUBJECT_LEN 100
#define MESSAGE_LEN 500
#define BUFFER_SIZE 1024

int getSubstring(const char* string, char* substring, int start){
    for(size_t i = 0; i < strlen(string); i++){
        substring[i] = string[start];
        if(substring[i] == '\n'){
            substring[i] = '\0';
            return i+1;                         // +1 to skip \n
        }
        start++;
    }
    return 0; // ???
}

void extract_message(const char* string, char* message, int start){
    for(size_t i = 0; i < strlen(string); i++){
        message[i] = string[start];
        if(message[i] == '\n' && message[i-1] == '.'){
            message[i] = '\0';
            return;
        }
        start++;
    }
}

void handle_send(int client_socket, const char* buffer){
    char message[MESSAGE_LEN + 1] = {0}; 
    char subject[SUBJECT_LEN + 1] = {0}; 
    char sender[USERNAME_LEN + 1] = {0}; 
    char receiver[USERNAME_LEN + 1] = {0};  

    int start = 5 + getSubstring(buffer, sender, 5);                                                      // +5 to skip SEND\n
    start += getSubstring(buffer, receiver, start);
    start += getSubstring(buffer, subject, start);
    extract_message(buffer, message, start);

    char dirpath[20] = {0}; sprintf(dirpath, "mail-spool/%s", sender);

    struct stat stats;
    int result = stat(dirpath, &stats);
    if(!(result == 0 && S_ISDIR(stats.st_mode))) mkdir(dirpath, 0777);

    size_t msg_len = strlen(message); size_t sub_len = strlen(subject);
    size_t sender_len = strlen(sender); size_t receiver_len = strlen(receiver);

    char filepath[20 + sub_len]; sprintf(filepath, "%s/%s.txt", dirpath, subject);
    char content[6 + sender_len + 5 + receiver_len + 10 + sub_len + 10 + msg_len + 1]; 
    sprintf(content, "From: %s\nTo: %s\nSubject: %s\nMessage: %s\n", sender, receiver, subject, message);

    FILE *file = fopen(filepath, "w");
    fprintf(file, content);
    fclose(file);
    
    send(client_socket, "OK\n", strlen("OK\n"), 0);
}   

void handle_list(int client_socket, char* buffer){
    struct dirent *en;
    char sender[9] = {0}; int ii = 0;
    for(size_t i = 5; i < strlen(buffer); i++){                                // +5 to skip SEND\n
        if(buffer[i] == '\n') break;
        sender[ii] = buffer[i];
        ii++;
    }

    char userDir[21] = {0}; sprintf(userDir, "mail-spool/%s/", sender);
    char server_response[BUFFER_SIZE] = {0};
    DIR *dr = opendir(userDir);
    if(dr){
        for(int i = 1; (en = readdir(dr)) != NULL; i++){
            if(strcmp(en->d_name, ".") == 0 || strcmp(en->d_name, "..") == 0){
                i--;
                continue;
            }
            char temp[6 + 264];                                                // 264 to have enough space for en->d_name (i received compiler warning :P)
            sprintf(temp, "%d. %s\n", i, en->d_name);
            strcat(server_response, temp);                                     // appends temp at the end of the string

            if(strlen(server_response) >= BUFFER_SIZE - 1) break;              // to handle bufferoverflow ;D
        }
        closedir(dr);
    }
    send(client_socket, server_response, strlen(server_response), 0);
}

void handle_read(int client_socket, char * buffer){
    char server_response[BUFFER_SIZE] = {0};
    char filename[6 + SUBJECT_LEN] = {0}; 
    char sender[USERNAME_LEN + 1] = {0}; 
    char number[4] = {0};                                              // assumption that one user will not send more than 9999 messages
 
    int start = 5 + getSubstring(buffer, sender, 5);                   // + 5 to skip READ\n
    start += getSubstring(buffer, number, start);
 
    struct dirent * en;
    char userDir[20] = {0}; sprintf(userDir, "mail-spool/%s", sender);
    DIR *dr = opendir(userDir);

    int fileNumber = atoi(number);
    
    if(fileNumber){                                                                // check if number was inserted
        if(dr){                                                                    // find number of saved file
            for(int i = 1; (en = readdir(dr)) != NULL; i++){
                if(strcmp(en->d_name, ".") == 0 || strcmp(en->d_name, "..") == 0){
                    i--;
                    continue;
                }
                if(fileNumber == i){
                    strcpy(filename, en->d_name);
                    break;
                }
            }
            closedir(dr);
        }
    }   

    size_t filename_len = strlen(filename);
    char filePath[11 + USERNAME_LEN + 1 + filename_len]; sprintf(filePath, "mail-spool/%s/%s", sender, filename);
    char line[BUFFER_SIZE] = {0};
    FILE *file = fopen(filePath, "r");

    if(file){
        while(fgets(line, sizeof(line), file)) strncat(server_response, line, sizeof(server_response) - strlen(server_response) - 1);
        fclose(file);
    }
    
    send(client_socket, server_response, strlen(server_response), 0);
}

void handle_delete(int client_socket, char* buffer){
    char sender[USERNAME_LEN + 1] = {0};
    char msg_num[4] = {0};                                                     // assumption that one user will not send more than 9999 messages

    int start = 4 + getSubstring(buffer, sender, 4);                           // + 4 to skip DEL\n
    start += getSubstring(buffer, msg_num, start);
    int message_number = atoi(msg_num);

    DIR *dr; struct dirent *en;
    char userDir[20] = {0}; sprintf(userDir, "mail-spool/%s", sender);
    dr = opendir(userDir);
    if(dr){
        for(int i = 1; (en = readdir(dr)) != NULL; i++){
            if(strcmp(en->d_name, ".") == 0 || strcmp(en->d_name, "..") == 0){
                i--;
                continue;
            }
            if(i == message_number){
                char temp[21 + 256];                                           // 256 to have enough space for en->d_name (i received compiler warning :P)
                sprintf(temp, "%s/%s", userDir, en->d_name);
                remove(temp);
                send(client_socket, "OK\n", strlen("OK\n"), 0);
                closedir(dr);
                return;
            }
        }
        closedir(dr);
    }
    send(client_socket, "ERR\n", strlen("ERR\n"), 0);
}

void handle_client(int client_socket){
    char buffer[BUFFER_SIZE] = {0};
    while(1){
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);     // -1 because of '\0'
        if(bytes_received < 0){
            perror("Failed to receive message.");
            close(client_socket);
            return;
        }
        buffer[BUFFER_SIZE] = '\0';
        if(strncmp(buffer, "SEND", 4) == 0) handle_send(client_socket, buffer);
        else if(strncmp(buffer, "LIST", 4) == 0) handle_list(client_socket, buffer);
        else if(strncmp(buffer, "READ", 4) == 0) handle_read(client_socket, buffer);
        else if(strncmp(buffer, "DEL", 3) == 0) handle_delete(client_socket, buffer);
        else if(strncmp(buffer, "QUIT", 4) == 0){
            close(client_socket);
            return;
        }else send(client_socket, "ERR\n", 4, 0);
    }
}

int main(int argc, char* argv[]){
    if(argc != 2){
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct stat sb;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    const char* dir = "mail-spool";
    int server_socket, client_socket;
    int port = atoi(argv[1]);

    if(stat(dir, &sb) != 0)                          // checks if dir for all mails already exists
        mkdir(dir, 0777);

    memset(&server_addr, 0, sizeof(server_addr));    // initializes with 0
    server_addr.sin_family = AF_INET;                // initializes addfamily with AF_INET (IPv4 is being used)
    server_addr.sin_port = htons(port);              // initilizes the port on which the socket is listening (converts portnumber 6000 to big-endian, because networkprotocols work with while systems work with little-endian)
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // initilizes ip-address of the socket (ceonverts here as well to the correct byte order) INADDR_ANY means, that the socket should listen on every available networkinterface and not only on one specific ip-address

    server_socket = socket(AF_INET, SOCK_STREAM, 0); // socket() creates a socket (AF_INET is for IPv4 socket) (SOCK_STREAM- creates a TCP-socket) 
    if(server_socket < 0){
        perror("Failed to create a socket.");
        exit(EXIT_FAILURE);
    }

    if(bind(server_socket, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0){
        perror("Failed to bind socket to port and adress.");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if(listen(server_socket, 10) < 0){
        perror("Failed to listen for incoming connections.");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", port);

    while(1){
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
        if(client_socket < 0){
            perror("Failed to accept client.");
            continue;
        }else printf("Client accepted.\n"); 

        pid_t pid = fork();
        if(pid < 0){
            perror("Failed to fork.");
            close(client_socket);
            continue;
        } 

        if(pid == 0){
            close(server_socket);                                                                // childprozess doesnt need access to server-socket
            handle_client(client_socket);                                                        // handle client-request
            exit(0);                                                                             // terminate childprocess
        }else close(client_socket);                                                              // parentprocess doesnt need access to client-socket
    }

    close(server_socket);
}