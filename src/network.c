#define _GNU_SOURCE
#include "../include/network.h"
#include "../vendor/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>      // For errno constants
#include <sys/time.h>   // For timeval
#include <sys/types.h>  // For fd_set
#include <sys/select.h> // For select>

// Forward declarations
static void handle_client(void *arg);
static void* monitor_connections(void *arg);

// Create a new database server
DatabaseServer* server_create(uint16_t port, Database *db, TransactionManager *txn_manager) {
    DatabaseServer *server = malloc(sizeof(DatabaseServer));
    if (!server) {
        perror("Failed to allocate server");
        return NULL;
    }
    
    server->port = port;
    server->running = false;
    server->db = db;
    server->txn_manager = txn_manager;
    server->connection_count = 0;
    
    // Create thread pool with 8 worker threads and queue size of MAX_CONNECTIONS
    server->thread_pool = thread_pool_create(8, MAX_CONNECTIONS);
    if (!server->thread_pool) {
        perror("Failed to create thread pool");
        free(server);
        return NULL;
    }
    
    // Initialize active connections array
    server->active_connections = malloc(sizeof(ClientConnection*) * MAX_CONNECTIONS);
    if (!server->active_connections) {
        perror("Failed to allocate connections array");
        thread_pool_destroy(server->thread_pool);
        free(server);
        return NULL;
    }
    
    // Initialize synchronization primitives
    if (pthread_mutex_init(&server->connections_lock, NULL) != 0) {
        perror("Failed to initialize connections mutex");
        free(server->active_connections);
        thread_pool_destroy(server->thread_pool);
        free(server);
        return NULL;
    }
    
    return server;
}

// Start the database server
bool server_start(DatabaseServer *server) {
    if (!server) {
        return false;
    }
    
    // Create socket
    server->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_fd < 0) {
        perror("Failed to create socket");
        return false;
    }
    
    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Failed to set socket options");
        close(server->server_fd);
        return false;
    }
    
    // Prepare server address
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(server->port);
    
    // Bind socket to address
    if (bind(server->server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Failed to bind socket");
        close(server->server_fd);
        return false;
    }
    
    // Listen for connections
    if (listen(server->server_fd, 10) < 0) {
        perror("Failed to listen");
        close(server->server_fd);
        return false;
    }
    
    // Ignore SIGPIPE to prevent crashes when writing to disconnected sockets
    signal(SIGPIPE, SIG_IGN);
    
    // Start monitor thread for connection timeouts
    if (pthread_create(&server->monitor_thread, NULL, monitor_connections, server) != 0) {
        perror("Failed to create monitor thread");
        close(server->server_fd);
        return false;
    }
    
    server->running = true;
    printf("Database server started on port %d\n", server->port);
    
    // Accept connections in a loop
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
    int client_socket;
    
    while (server->running) {
        client_socket = accept(server->server_fd, (struct sockaddr*)&client_addr, &addrlen);
        if (client_socket < 0) {
            if (server->running) {
                perror("Failed to accept connection");
            }
            continue;
        }
        
        // Create client connection
        ClientConnection *conn = connection_create(client_socket, client_addr);
        if (!conn) {
            close(client_socket);
            continue;
        }

        // Add client handling task to thread pool
        ClientHandlerArg *handler_arg = malloc(sizeof(ClientHandlerArg));
        if (!handler_arg) {
            fprintf(stderr, "Failed to allocate handler argument\n");
            connection_close(conn);
            
            pthread_mutex_lock(&server->connections_lock);
            // Remove the connection from active connections
            for (int i = 0; i < server->connection_count; i++) {
                if (server->active_connections[i] == conn) {
                    server->connection_count--;
                    if (i < server->connection_count) {
                        server->active_connections[i] = 
                            server->active_connections[server->connection_count];
                    }
                    break;
                }
            }
            pthread_mutex_unlock(&server->connections_lock);
            continue;
        }

        handler_arg->server = server;
        handler_arg->connection = conn;

        
        // Add to active connections
        pthread_mutex_lock(&server->connections_lock);
        if (server->connection_count < MAX_CONNECTIONS) {
            server->active_connections[server->connection_count++] = conn;
            pthread_mutex_unlock(&server->connections_lock);
            
            // Start auto transaction for this connection
            conn->transaction_id = txn_begin(server->txn_manager);
            
            // Add client handling task to thread pool
            if (!thread_pool_add_task(server->thread_pool, handle_client, handler_arg)) {
                fprintf(stderr, "Failed to add client to thread pool\n");
                connection_close(conn);
                
                pthread_mutex_lock(&server->connections_lock);
                for (int i = 0; i < server->connection_count; i++) {
                    if (server->active_connections[i] == conn) {
                        // Remove from active connections
                        server->connection_count--;
                        if (i < server->connection_count) {
                            server->active_connections[i] = 
                                server->active_connections[server->connection_count];
                        }
                        break;
                    }
                }
                pthread_mutex_unlock(&server->connections_lock);
            }
        } else {
            pthread_mutex_unlock(&server->connections_lock);
            // Too many connections
            const char *msg = json_create_error_response("Server at maximum capacity");
            send(client_socket, msg, strlen(msg), 0);
            free((void*)msg);
            close(client_socket);
            free(conn);
        }
    }
    
    return true;
}

// Stop the database server
void server_stop(DatabaseServer *server) {
    if (!server || !server->running) {
        return;
    }
    
    server->running = false;
    
    // Close server socket to stop accept()
    close(server->server_fd);
    
    // Wait for monitor thread to finish
    pthread_join(server->monitor_thread, NULL);
    
    // Close all client connections
    pthread_mutex_lock(&server->connections_lock);
    for (int i = 0; i < server->connection_count; i++) {
        connection_close(server->active_connections[i]);
    }
    server->connection_count = 0;
    pthread_mutex_unlock(&server->connections_lock);
    
    // Destroy thread pool
    thread_pool_destroy(server->thread_pool);
    
    // Clean up resources
    pthread_mutex_destroy(&server->connections_lock);
    free(server->active_connections);
    
    printf("Database server stopped\n");
}

// Create a new client connection
ClientConnection* connection_create(int socket_fd, struct sockaddr_in address) {
    ClientConnection *conn = malloc(sizeof(ClientConnection));
    if (!conn) {
        perror("Failed to allocate connection");
        return NULL;
    }
    
    conn->socket_fd = socket_fd;
    conn->address = address;
    conn->buffer_length = 0;
    conn->connected = true;
    conn->authenticated = false;  // Will be used with ACL
    conn->transaction_id = 0;     // Will be set when added to thread pool
    conn->last_activity = time(NULL);
    
    // Initialize mutex
    if (pthread_mutex_init(&conn->lock, NULL) != 0) {
        perror("Failed to initialize connection mutex");
        free(conn);
        return NULL;
    }
    
    // Set default database
    strcpy(conn->current_database, "");
    
    printf("Client connected: %s:%d\n", 
           inet_ntoa(address.sin_addr), 
           ntohs(address.sin_port));
    
    return conn;
}

// Close a client connection
void connection_close(ClientConnection *conn) {
    if (!conn) {
        return;
    }
    
    pthread_mutex_lock(&conn->lock);
    if (conn->connected) {
        conn->connected = false;
        close(conn->socket_fd);
        printf("Client disconnected: %s:%d\n", 
               inet_ntoa(conn->address.sin_addr), 
               ntohs(conn->address.sin_port));
    }
    pthread_mutex_unlock(&conn->lock);
    
    pthread_mutex_destroy(&conn->lock);
    free(conn);
}

// Client connection handler (runs in worker thread)
static void handle_client(void *arg) {
    ClientHandlerArg *handler_arg = (ClientHandlerArg*)arg;
    ClientConnection *conn = handler_arg->connection;
    DatabaseServer *server = handler_arg->server;

    // set socket to non-blocking mode
    int flags = fcntl(conn->socket_fd, F_GETFL, 0);
    if (flags == -1) {
        perror("Failed to get socket flags");
        connection_close(conn);
        free(handler_arg);
        return;
    }
    if (fcntl(conn->socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("Failed to set socket to non-blocking mode");
        connection_close(conn);
        free(handler_arg);
        return;
    }
    

    
    // Send welcome message
    const char *welcome = json_create_success_response("Connected to Database Server");
    connection_send_response(conn, welcome);
    free((void*)welcome);
    
    while (conn->connected) {
        // Read client request
        pthread_mutex_lock(&conn->lock);
        conn->buffer_length = 0;
        ssize_t bytes_read = recv(conn->socket_fd, 
                                 conn->buffer, 
                                 MAX_BUFFER_SIZE - 1, 
                                 0);
        
        if (bytes_read <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available, don't treat as error
                pthread_mutex_unlock(&conn->lock);
                // Sleep briefly to avoid CPU spinning
                usleep(10000);  // 10ms
                continue;
            }
            // Actual error
            pthread_mutex_unlock(&conn->lock);
            break;
        }
        
        // Null terminate the received data
        conn->buffer[bytes_read] = '\0';
        conn->buffer_length = bytes_read;
        
        // Update last activity time
        conn->last_activity = time(NULL);
        pthread_mutex_unlock(&conn->lock);
        
        // Process command (JSON parsing and execution)
        if (!connection_process_command(handler_arg, server->db, server->txn_manager)) {
            // Error processing command
            const char *error = json_create_error_response("Error processing command");
            connection_send_response(conn, error);
            free((void*)error);
        }
    free(handler_arg);
    }
    
    // Clean up when client disconnects
    pthread_mutex_lock(&server->connections_lock);
        
    // Roll back any active transaction
    if (conn->transaction_id != 0) {
        txn_rollback(server->txn_manager, conn->transaction_id);
        conn->transaction_id = 0;
    }
    
    // Remove from active connections
    for (int i = 0; i < server->connection_count; i++) {
        if (server->active_connections[i] == conn) {
            server->connection_count--;
            if (i < server->connection_count) {
                server->active_connections[i] = 
                    server->active_connections[server->connection_count];
            }
            break;
        }
    }
    pthread_mutex_unlock(&server->connections_lock);
    
    // Close and free the connection
    connection_close(conn);
}

// Monitor thread to handle connection timeouts
static void* monitor_connections(void *arg) {
    DatabaseServer *server = (DatabaseServer*)arg;
    
    while (server->running) {
        // Sleep for a while before checking connections
        sleep(5);
        
        time_t current_time = time(NULL);
        
        pthread_mutex_lock(&server->connections_lock);
        
        // Check each connection for timeout
        for (int i = 0; i < server->connection_count; i++) {
            ClientConnection *conn = server->active_connections[i];
            
            pthread_mutex_lock(&conn->lock);
            if (conn->connected && 
                (current_time - conn->last_activity) > CONNECTION_TIMEOUT_SECONDS) {
                
                printf("Connection timed out: %s:%d\n", 
                       inet_ntoa(conn->address.sin_addr), 
                       ntohs(conn->address.sin_port));
                
                // Roll back transaction if active
                if (conn->transaction_id != 0) {
                    txn_rollback(server->txn_manager, conn->transaction_id);
                    conn->transaction_id = 0;
                }
                
                // Close connection
                conn->connected = false;
                close(conn->socket_fd);
                
                // Mark for removal
                server->active_connections[i] = NULL;
            }
            pthread_mutex_unlock(&conn->lock);
        }
        
        // Remove NULL connections
        int j = 0;
        for (int i = 0; i < server->connection_count; i++) {
            if (server->active_connections[i] != NULL) {
                server->active_connections[j++] = server->active_connections[i];
            }
        }
        server->connection_count = j;
        
        pthread_mutex_unlock(&server->connections_lock);
    }
    
    return NULL;
}

// Send a response to the client
void connection_send_response(ClientConnection *conn, const char *response) {
    if (!conn || !conn->connected || !response) {
        return;
    }
    
    pthread_mutex_lock(&conn->lock);
    if (conn->connected) {
        send(conn->socket_fd, response, strlen(response), 0);
        // Send a newline for message framing
        send(conn->socket_fd, "\n", 1, 0);
    }
    pthread_mutex_unlock(&conn->lock);
}

// Process a JSON command from the client
bool connection_process_command(ClientHandlerArg *handlerArgs, Database *db, TransactionManager *txn_manager) {
    // Parse the JSON command
    ClientConnection *conn = handlerArgs->connection;
    cJSON *root = cJSON_Parse(conn->buffer);
    if (!root) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "JSON parse error: %s", error_ptr);
            const char *response = json_create_error_response(error_msg);
            connection_send_response(conn, response);
            free((void*)response);
        }
        return false;
    }
    #ifdef DEBUG
    printf("DEBUG: Received command: %s\n", conn->buffer);
    #endif

    //     // Extract command type
    // cJSON *cmd = cJSON_GetObjectItem(root, "command");
    // if (!cmd || !cJSON_IsString(cmd)) {
    //     const char *response = json_create_error_response("Missing or invalid 'command' field");
    //     connection_send_response(conn, response);
    //     free((void*)response);
    //     cJSON_Delete(root);
    //     return false;
    // }
    
    // const char *command = cmd->valuestring;
    // bool success = true;
    
    // // Handle meta commands separately
    // if (strcmp(command, "meta") == 0) {
    //     cJSON_Delete(root);
    //     // Pass server pointer through conn->server or find another way to access it
    //     DatabaseServer *server = handlerArgs->server; // This won't work as written
    //     return json_parse_meta_command(conn->buffer, conn, server);
    // }

    cJSON_Delete(root);  // We don't need the root anymore as we'll parse directly
    
    // Parse the JSON into a Statement structure
    // Statement statement;
    // statement.db = db;  // Set the database reference
    
    // if (!json_parse_statement(conn->buffer, &statement)) {
    //     const char *response = json_create_error_response("Failed to parse command");
    //     connection_send_response(conn, response);
    //     free((void*)response);
    //     return false;
    // }
    
    // // Execute the statement
    // ExecuteResult result = execute_statement(&statement, db);
    
    // // Handle the result
    // char *response = NULL;
    // switch (result) {
    //     case EXECUTE_SUCCESS:
    //         response = json_create_success_response("Command executed successfully");
    //         break;
    //     case EXECUTE_DUPLICATE_KEY:
    //         response = json_create_error_response("Duplicate key error");
    //         break;
    //     case EXECUTE_TABLE_FULL:
    //         response = json_create_error_response("Table is full");
    //         break;
    //     case EXECUTE_TABLE_NOT_FOUND:
    //         response = json_create_error_response("Table not found");
    //         break;
    //     case EXECUTE_TABLE_OPEN_ERROR:
    //         response = json_create_error_response("Error opening table");
    //         break;
    //     case EXECUTE_INDEX_ERROR:
    //         response = json_create_error_response("Index error");
    //         break;
    //     default:
    //         response = json_create_error_response("Unknown error executing command");
    //         break;
    // }
    
    // Send response
    char *response = json_create_success_response("Received command");
    connection_send_response(conn, response);
    free(response);
    
    // // Clean up any allocated memory in the statement
    // if (statement.columns_to_select) {
    //     for (uint32_t i = 0; i < statement.num_columns_to_select; i++) {
    //         free(statement.columns_to_select[i]);
    //     }
    //     free(statement.columns_to_select);
    // }
    
    // if (statement.values) {
    //     for (uint32_t i = 0; i < statement.num_values; i++) {
    //         free(statement.values[i]);
    //     }
    //     free(statement.values);
    // }
    
    // return (result == EXECUTE_SUCCESS);

    return true;
}


// Parse a JSON command
bool json_parse_command(const char *json_str, void *output) {
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return false;
    }
    
    // Extract command type
    cJSON *command_type = cJSON_GetObjectItem(root, "command");
    if (!command_type || !cJSON_IsString(command_type)) {
        cJSON_Delete(root);
        return false;
    }
    
    // Process the command based on its type
    // TODO: Implement proper command processing
    
    cJSON_Delete(root);
    return true;
}