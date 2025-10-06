#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define MAX_USERS 100    // Initial capacity of users array
#define MAX_STRING 256   // Maximum string size
#define INITIAL_FILES 10    // Initial capacity of files array per user

// Structure to store published file information
typedef struct {
    char filename[MAX_STRING];
    char description[MAX_STRING];
} File;

// Structure to store user information
typedef struct {
    char username[MAX_STRING];
    int connected;
    char ip[MAX_STRING];
    int port;
    File *files;        // File pointer to allow resizing
    int num_files;      // Current number of files
    int max_files;      // Capacity of files array
} User;

// Global variables
int server_socket;
User *users = NULL;
int num_users = 0;
int max_users = 0;
pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function prototypes
void *handle_client(void *socket_desc);
int find_user(char *username);
void add_user(char *username);
void remove_user(char *username);
void connect_user(int user_index, char *ip, int port);
void disconnect_user(int user_index);
int add_file(int user_index, char *filename, char *description);
int remove_file(int user_index, char *filename);
void handle_sigint(int sig);
void free_user_resources(int user_index);

int main(int argc, char *argv[]) {
    int port = 0;
    int client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t thread_id;
    int *new_sock;
    char ip_str[INET_ADDRSTRLEN];

    // Handle SIGINT signal (Ctrl+C)
    signal(SIGINT, handle_sigint);

    // Parse command line arguments
    if (argc != 3 || strcmp(argv[1], "-p") != 0) {
        printf("Usage: %s -p <port>\n", argv[0]);
        exit(1);
    }
    port = atoi(argv[2]);
    if (port <= 0) {
        printf("Error: Invalid port\n");
        exit(1);
    }

    // Initialize users array
    max_users = MAX_USERS;
    users = (User *)calloc(max_users, sizeof(User));
    if (!users) {
        perror("Error allocating memory for users");
        exit(1);
    }

    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error creating socket");
        exit(1);
    }

    // Allow port reuse
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Error in setsockopt");
        exit(1);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error in bind");
        exit(1);
    }

    // Listen for connections
    if (listen(server_socket, 10) < 0) {
        perror("Error in listen");
        exit(1);
    }

    // Get local IP
    char local_ip[MAX_STRING];
    struct sockaddr_in local_addr;
    socklen_t local_len = sizeof(local_addr);
    getsockname(server_socket, (struct sockaddr *)&local_addr, &local_len);
    inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, MAX_STRING);

    printf("s > init server %s:%d\n", local_ip, port);
    printf("s > \n");

    // Main server loop
    while (1) {
        // Accept client connection
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Error in accept");
            continue;
        }

        // Get client IP
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));

        // Create new socket for the thread
        new_sock = malloc(sizeof(int));
        *new_sock = client_socket;

        // Create new thread to handle connection
        if (pthread_create(&thread_id, NULL, handle_client, (void *)new_sock) < 0) {
            perror("Error creating thread");
            free(new_sock);
            close(client_socket);
            continue;
        }
        
        // Detach thread so it can clean up automatically
        pthread_detach(thread_id);
    }

    return 0;
}

// Function to handle SIGINT signal (Ctrl+C)
void handle_sigint(int sig) {
    printf("\nClosing server...\n");
    
    // Free resources for each user
    for (int i = 0; i < num_users; i++) {
        free_user_resources(i);
    }
    
    close(server_socket);
    free(users);
    exit(0);
}

// Function to free resources for a specific user
void free_user_resources(int user_index) {
    if (users[user_index].files != NULL) {
        free(users[user_index].files);
        users[user_index].files = NULL;
    }
}

// Function to search for a user by name
int find_user(char *username) {

    for (int i = 0; i < num_users; i++) {
        if (strcmp(users[i].username, username) == 0) {
            return i;
        }
    }
    
    return -1;
}

// Function to add a new user
void add_user(char *username) {
    
    // Check if we need to resize the array
    if (num_users >= max_users) {
        max_users *= 2;
        User *temp = realloc(users, max_users * sizeof(User));
        if (!temp) {
            perror("Error resizing users array");
            
            return;
        }
        users = temp;
    }

    // Initialize new user
    strncpy(users[num_users].username, username, MAX_STRING - 1);
    users[num_users].username[MAX_STRING - 1] = '\0';
    users[num_users].connected = 0;
    users[num_users].num_files = 0;
    
    // Initialize dynamic files array
    users[num_users].max_files = INITIAL_FILES;
    users[num_users].files = (File *)calloc(users[num_users].max_files, sizeof(File));
    if (!users[num_users].files) {
        perror("Error allocating memory for files");
        return;
    }
    
    num_users++;
}

// Function to remove a user
void remove_user(char *username) {
    int index = find_user(username);
    
    // Free memory from files array
    free_user_resources(index);
    
    if (index != -1) {
        // Move last user to deleted position
        if (index < num_users - 1) {
            users[index] = users[num_users - 1];
        }
        num_users--;
    }
}

// Function to connect a user
void connect_user(int user_index, char *ip, int port) {
    users[user_index].connected = 1;
    strncpy(users[user_index].ip, ip, MAX_STRING - 1);
    users[user_index].ip[MAX_STRING - 1] = '\0';
    users[user_index].port = port;
}


// Function to disconnect a user
void disconnect_user(int user_index) {
    users[user_index].connected = 0;
}

// Function to add a file
int add_file(int user_index, char *filename, char *description) {
    
    int result = 0;
    
    // Check if file already exists
    for (int i = 0; i < users[user_index].num_files; i++) {
        if (strcmp(users[user_index].files[i].filename, filename) == 0) {
            result = -1;
            break;
        }
    }
    
    // Check if we need to resize files array
    if (users[user_index].num_files >= users[user_index].max_files) {
        users[user_index].max_files *= 2;
        File *temp = realloc(users[user_index].files, users[user_index].max_files * sizeof(File));
        if (!temp) {
            perror("Error resizing files array");
            return -2;  // Error resizing
        }
        users[user_index].files = temp;
    }
    
    // Add file
    strncpy(users[user_index].files[users[user_index].num_files].filename, filename, MAX_STRING - 1);
    users[user_index].files[users[user_index].num_files].filename[MAX_STRING - 1] = '\0';
    
    strncpy(users[user_index].files[users[user_index].num_files].description, description, MAX_STRING - 1);
    users[user_index].files[users[user_index].num_files].description[MAX_STRING - 1] = '\0';
    
    users[user_index].num_files++;
    
    return result;
}

// Function to remove a file
int remove_file(int user_index, char *filename) {
    
    int result = -1;
    
    // Search for file
    for (int i = 0; i < users[user_index].num_files; i++) {
        if (strcmp(users[user_index].files[i].filename, filename) == 0) {
            // Move last file to deleted position
            if (i < users[user_index].num_files - 1) {
                users[user_index].files[i] = users[user_index].files[users[user_index].num_files - 1];
            }
            users[user_index].num_files--;
            result = 0;
            break;
        }
    }
    
    return result;
}

// Function to read a string from a socket
int read_string(int socket, char *buffer, int max_length) {
    memset(buffer, 0, max_length);
    int total_read = 0;
    char c;
    
    while (total_read < max_length - 1) {
        int n = read(socket, &c, 1);
        if (n <= 0) {
            return -1; // Read error
        }
        
        if (c == '\0') {
            break;
        }
        
        buffer[total_read++] = c;
    }
    
    buffer[total_read] = '\0';
    return total_read;
}

// Main function to handle client connections
void *handle_client(void *socket_desc) {
    int sock = *(int*)socket_desc;
    free(socket_desc);
    
    char buffer[MAX_STRING];
    char operation[MAX_STRING];
    char datetime[MAX_STRING];  
    char username[MAX_STRING];
    char filename[MAX_STRING];
    char description[MAX_STRING];
    char port_str[MAX_STRING];
    char remote_username[MAX_STRING];
    int port = 0;
    unsigned char response = 0;
    
    // Read requested operation
    if (read_string(sock, operation, MAX_STRING) <= 0) {
        printf("s > Error reading operation\n");
        close(sock);
        return NULL;
    }

    // Read date and time (new functionality)
    if (read_string(sock, datetime, MAX_STRING) <= 0) {
        printf("s > Error reading date and time\n");
        close(sock);
        return NULL;
    }
    
    // Process operation
    if (strcmp(operation, "REGISTER") == 0) {
        // Read username
        if (read_string(sock, username, MAX_STRING) <= 0) {
            close(sock);
            return NULL;
        }
        
        printf("s > OPERATION REGISTER FROM %s - Timestamp: %s\n", username, datetime);
        
        // Check if user already exists
        pthread_mutex_lock(&users_mutex);
        int user_index = find_user(username);
        
        if (user_index != -1) {
            response = 1; // User already registered
        } else {
            add_user(username);
            response = 0; // Success
        }
        pthread_mutex_unlock(&users_mutex);
        
        // Send response
        write(sock, &response, 1);
    }
    else if (strcmp(operation, "UNREGISTER") == 0) {
        // Read username
        if (read_string(sock, username, MAX_STRING) <= 0) {
            close(sock);
            return NULL;
        }
        
        printf("s > OPERATION UNREGISTER FROM %s - Timestamp: %s\n", username, datetime);
        
        // Check if user exists
        pthread_mutex_lock(&users_mutex);
        int user_index = find_user(username);
        
        if (user_index == -1) {
            response = 1; // User does not exist
        } else {
            remove_user(username);
            response = 0; // Success
        }
        pthread_mutex_unlock(&users_mutex);
        
        // Send response
        write(sock, &response, 1);
    }
    else if (strcmp(operation, "CONNECT") == 0) {
        // Read username
        if (read_string(sock, username, MAX_STRING) <= 0) {
            close(sock);
            return NULL;
        }
        
        // Read port
        if (read_string(sock, port_str, MAX_STRING) <= 0) {
            close(sock);
            return NULL;
        }

        port = atoi(port_str);
        
        printf("s > OPERATION CONNECT FROM %s - Timestamp: %s\n", username, datetime);
        
        // Get client IP address
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        if (getpeername(sock, (struct sockaddr*)&addr, &addr_len) < 0) {
            perror("s > getpeername error");
            close(sock);
            return NULL;
        }
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
        
        // Check if user exists
        pthread_mutex_lock(&users_mutex);
        int user_index = find_user(username);
        
        if (user_index == -1) {
            response = 1; // User does not exist
        } else if (users[user_index].connected) {
            response = 2; // User already connected
        } else {
            connect_user(user_index, ip, port);
            response = 0; // Success
        }
        pthread_mutex_unlock(&users_mutex);        

        // Send response
        ssize_t bytes_written = write(sock, &response, 1);
        if (bytes_written <= 0) {
            perror("s > Error writing CONNECT response");
        }
    }
    else if (strcmp(operation, "DISCONNECT") == 0) {
        // Read username
        if (read_string(sock, username, MAX_STRING) <= 0) {
            close(sock);
            return NULL;
        }
        
        printf("s > OPERATION DISCONNECT FROM %s - Timestamp: %s\n", username, datetime);
        
        // Check if user exists and is connected
        pthread_mutex_lock(&users_mutex);
        int user_index = find_user(username);
        
        if (user_index == -1) {
            response = 1; // User does not exist
        } else if (!users[user_index].connected) {
            response = 2; // User not connected
        } else {
            disconnect_user(user_index);
            response = 0; // Success
        }
        pthread_mutex_unlock(&users_mutex);
        
        // Send response
        write(sock, &response, 1);
    }
    else if (strcmp(operation, "PUBLISH") == 0) {
        // Read username
        if (read_string(sock, username, MAX_STRING) <= 0) {
            response = 4;
            write(sock, &response, 1);
            close(sock);
            return NULL;
        }
        
        // Read filename
        if (read_string(sock, filename, MAX_STRING) <= 0) {
            response = 4;
            write(sock, &response, 1);
            close(sock);
            return NULL;
        }
        
        // Read description
        if (read_string(sock, description, MAX_STRING) <= 0) {
            response = 4;
            write(sock, &response, 1);
            close(sock);
            return NULL;
        }
        
        printf("s > OPERATION PUBLISH FROM %s - Timestamp: %s\n", username, datetime);
        
        if (strcmp(username, "__NONE__") == 0) {
            response = 1;
            write(sock, &response, 1);
            close(sock);
            return NULL;
        }
        pthread_mutex_lock(&users_mutex);
        int user_index = find_user(username);
        
        if (user_index == -1) {
            response = 1; // User does not exist
        } else if (!users[user_index].connected) {
            response = 2; // User not connected
        } else {
            int result = add_file(user_index, filename, description);
            if (result == -1) {
                response = 3; // File already published
            } else if (result == -2) {
                response = 4; // Error resizing
            } else {
                response = 0; // Success
            }
        }
        pthread_mutex_unlock(&users_mutex);
        
        // Send response
        write(sock, &response, 1);
    }
    else if (strcmp(operation, "DELETE") == 0) {
        // Read username
        if (read_string(sock, username, MAX_STRING) <= 0) {
            close(sock);
            return NULL;
        }
        
        // Read filename
        if (read_string(sock, filename, MAX_STRING) <= 0) {
            close(sock);
            return NULL;
        }
        
        printf("s > OPERATION DELETE FROM %s - Timestamp: %s\n", username, datetime);
        
        if (strcmp(username, "__NONE__") == 0) {
            response = 1;
            write(sock, &response, 1);
            close(sock);
            return NULL;
        }
        // Check if user exists and is connected
        pthread_mutex_lock(&users_mutex);
        int user_index = find_user(username);
        
        if (user_index == -1) {
            response = 1; // User does not exist
        } else if (!users[user_index].connected) {
            response = 2; // User not connected
        } else {
            int result = remove_file(user_index, filename);
            if (result == -1) {
                response = 3; // File not published
            } else {
                response = 0; // Success
            }
        }
        pthread_mutex_unlock(&users_mutex);
        
        // Send response
        write(sock, &response, 1);
    }
    else if (strcmp(operation, "LIST_USERS") == 0) {
        // Read username
        if (read_string(sock, username, MAX_STRING) <= 0) {
            close(sock);
            return NULL;
        }
        
        printf("s > OPERATION LIST_USERS FROM %s - Timestamp: %s\n", username, datetime);
        
        if (strcmp(username, "__NONE__") == 0) {
            response = 1;
            write(sock, &response, 1);
            close(sock);
            return NULL;
        }
        // Check if user exists and is connected
        pthread_mutex_lock(&users_mutex);
        int user_index = find_user(username);
        
        if (user_index == -1) {
            response = 1; // User does not exist
            write(sock, &response, 1);
        } else if (!users[user_index].connected) {
            response = 2; // User not connected
            write(sock, &response, 1);
        } else {
            response = 0; // Success
            write(sock, &response, 1);
            
            // Count connected users
            int connected_users = 0;
            for (int i = 0; i < num_users; i++) {
                if (users[i].connected) {
                    connected_users++;
                }
            }
            
            // Send number of connected users
            sprintf(buffer, "%d", connected_users);
            write(sock, buffer, strlen(buffer) + 1);
            
            // Send information for each connected user
            for (int i = 0; i < num_users; i++) {
                if (users[i].connected) {
                    // Send username
                    write(sock, users[i].username, strlen(users[i].username) + 1);
                    
                    // Send IP
                    write(sock, users[i].ip, strlen(users[i].ip) + 1);
                    
                    // Send port
                    sprintf(buffer, "%d", users[i].port);
                    write(sock, buffer, strlen(buffer) + 1);
                }
            }
        }
        pthread_mutex_unlock(&users_mutex);
    }
    else if (strcmp(operation, "LIST_CONTENT") == 0) {
        // Read username performing the operation
        if (read_string(sock, username, MAX_STRING) <= 0) {
            close(sock);
            return NULL;
        }
        
        // Read remote username
        if (read_string(sock, remote_username, MAX_STRING) <= 0) {
            close(sock);
            return NULL;
        }
        
        printf("s > OPERATION LIST_CONTENT FROM %s - Timestamp: %s\n", username, datetime);
        
        if (strcmp(username, "__NONE__") == 0) {
            response = 1;
            write(sock, &response, 1);
            close(sock);
            return NULL;
        }
        // Check if users exist and if local user is connected
        pthread_mutex_lock(&users_mutex);
        int user_index = find_user(username);
        int remote_user_index = find_user(remote_username);
        
        if (user_index == -1) {
            response = 1; // Local user does not exist
            write(sock, &response, 1);
        } else if (!users[user_index].connected) {
            response = 2; // Local user not connected
            write(sock, &response, 1);
        } else if (remote_user_index == -1) {
            response = 3; // Remote user does not exist
            write(sock, &response, 1);
        } else {
            response = 0; // Success
            write(sock, &response, 1);
            
            // Send number of files
            sprintf(buffer, "%d", users[remote_user_index].num_files);
            write(sock, buffer, strlen(buffer) + 1);
            
            // Send filenames
            for (int i = 0; i < users[remote_user_index].num_files; i++) {
                write(sock, users[remote_user_index].files[i].filename, 
                      strlen(users[remote_user_index].files[i].filename) + 1);
            }
        }
        pthread_mutex_unlock(&users_mutex);
    }
    else {
        printf("s > Unknown operation: %s\n", operation);
    }
    
    // Close socket
    close(sock);
    return NULL;
}
