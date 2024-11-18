#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define USERNAME_LEN 8
#define SUBJECT_LEN 100
#define MESSAGE_LEN 500
#define BUFFER_SIZE 1024

int validateUsername(const char* username){
    if(strlen(username) > 9){
        printf("Username too long.\n");
        return 0;
    }
    
    for(size_t i = 0; i < strlen(username); i++){
        if(!((username[i] >= 48 && username[i] <= 57) || (username[i] >= 65 && username[i] <= 90) || (username[i] >= 97 && username[i] <= 122) || username[i] == '\0')){
            printf("Only upper and lowercase letters allowed as well as numbers.\n");
            return 0;
        }
    }
    return 1;
}

void rmnewLine(char* string){
    for(size_t i = 0; i < strlen(string); i++){
        if(string[i] == '\n'){
            string[i] = '\0';
            break;
        }
    }
}

void enterMessage(char* message){
    for(int i = 0; i < MESSAGE_LEN; i++){
        message[i] = getchar();
        if(message[i-1] == '.' && message[i] == '\n'){
            message[i] = '\0';
            return;
        }
    }
}

void sendMessage(char* sender, int sender_len, int client_socket){
    char message[MESSAGE_LEN + 2] = {0};
    char subject[SUBJECT_LEN + 2] = {0};  
    char receiver[USERNAME_LEN + 2] = {0};

    printf("\nInsert Receiver username: ");
    fgets(receiver, sizeof(receiver), stdin);
    rmnewLine(receiver);
    if(!validateUsername(receiver)) exit(EXIT_FAILURE);
    int receiver_len = strlen(receiver);
    
    printf("Insert Message subject: ");
    fgets(subject, sizeof(subject), stdin);
    rmnewLine(subject);
    int subject_len = strlen(subject);

    printf("Insert Message: ");
    enterMessage(message);
    int message_len = strlen(message);
    
    int buffer_size = 5 + sender_len + 1 + receiver_len + 1 + subject_len + 1 + message_len + 1 + 1; // +1 for all \n and + 1 for '.'
    char sendProtocol[buffer_size];
    sprintf(sendProtocol, "SEND\n%s\n%s\n%s\n%s\n.", sender, receiver, subject, message);
    send(client_socket, sendProtocol, strlen(sendProtocol), 0);
}

void listMessages(char* sender, int sender_len, int client_socket){
    char listProtocol[5 + sender_len + 1];
    sprintf(listProtocol, "LIST\n%s\n", sender);
    send(client_socket, listProtocol, strlen(listProtocol), 0);
}

void readMessage(char* sender, int sender_len, int client_socket){
    printf("Enter Message-Number: ");
    int msg_num; scanf("%d", &msg_num); getchar();
    
    int msg_num_len = snprintf(NULL, 0, "%d", msg_num);
    int buffer_size = 5 + sender_len + 1 + msg_num_len + 1;
    
    char* readProtocol = (char*)malloc(buffer_size);
    sprintf(readProtocol, "READ\n%s\n%d\n",sender, msg_num);
    
    send(client_socket, readProtocol, strlen(readProtocol), 0);
    free(readProtocol);
}

void removeMessage(char* sender, int sender_len, int client_socket){
    printf("Enter Message-Number: ");
    int msg_num; scanf("%d", &msg_num); getchar();

    int msg_num_len = snprintf(NULL, 0, "%d", msg_num);
    int buffer_size = 4 + sender_len + 1 + msg_num_len + 1;

    char* removeProtocol = (char*)malloc(buffer_size);
    sprintf(removeProtocol, "DEL\n%s\n%d\n",sender, msg_num);

    send(client_socket, removeProtocol, strlen(removeProtocol), 0);
    free(removeProtocol);
}

int main(int argc, char* argv[]){
    if(argc != 3){
        fprintf(stderr, "Usage: %s <ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    char username[USERNAME_LEN + 2] = {0}; // 8 for username +1 for \n and +1 for \0 (so that \n will not be in input buffer)
    const char* server_ip = argv[1];
    int port = atoi(argv[2]);

    printf("Enter your username: "); 
    fgets(username, sizeof(username), stdin);
    rmnewLine(username);
    if(!validateUsername(username)) exit(EXIT_FAILURE);
    int username_len = strlen(username);

    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(client_socket < 0){
        perror("Failed to create a socket.");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if(inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0){
        perror("Failed to convert IP-adress.");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    if(connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("Failed to connect to server.");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    while(1){
        char response[BUFFER_SIZE] = {0};
        printf("Available Options:\n0. QUIT: logout the client\n1. SEND: client sends a message to the server\n2. LIST: lists all received messages of a specific user from his inbox\n3. READ: display a specific message of a specific user\n4. DEL:  removes a specific message\nEnter command: ");
        int input = 187; scanf("%d", &input); getchar();
        if(input == 0){
            close(client_socket);
            return 0;
        }else if(input == 1) sendMessage(username, username_len, client_socket);
        else if(input == 2) listMessages(username, username_len, client_socket);
        else if(input == 3) readMessage(username, username_len, client_socket);
        else if(input == 4) removeMessage(username, username_len, client_socket);
        else{
            printf("Only options 0-4 available.\n");
            continue;
        }

        ssize_t bytes_received = recv(client_socket, response, BUFFER_SIZE - 1, 0);
        if(bytes_received > 0){
            response[bytes_received] = '\0';
            printf("\nServer response:\n%s\n", response);
        }
    }
}