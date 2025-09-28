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
#include "../include/command_processor.h"
#include "../include/input_handling.h"

// Forward declarations
static void handle_client(void *arg);
static void* monitor_connections(void *arg);
static bool json_to_text_command(cJSON *json, char *text_command, size_t max_len);

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
    
    // Initialize session state
    conn->session_db = NULL;
    conn->session_input_buf = newInputBuffer();
    
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
    
    // Free session state
    if (conn->session_input_buf) free_input_buffer(conn->session_input_buf);
    if (conn->session_db) db_close_database(conn->session_db);
    
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
    

    
    while (conn->connected) {
        // Read length prefix (4 bytes)
        uint32_t message_length;
        ssize_t bytes_read = recv(conn->socket_fd, &message_length, 4, MSG_WAITALL);
        
        if (bytes_read != 4) {
            if (bytes_read <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // No data available
                    usleep(10000);  // 10ms
                    continue;
                }
                // Connection closed or error
                break;
            }
        }
        
        // Convert from network byte order
        message_length = ntohl(message_length);
        
        // Validate message length
        if (message_length > MAX_BUFFER_SIZE - 1) {
            const char *error = json_create_error_response("Message too large");
            connection_send_response(conn, error);
            free((void*)error);
            break;
        }
        
        // Read the JSON message data
        pthread_mutex_lock(&conn->lock);
        bytes_read = recv(conn->socket_fd, conn->buffer, message_length, MSG_WAITALL);
        
        if (bytes_read != (ssize_t)message_length) {
            pthread_mutex_unlock(&conn->lock);
            break;
        }
        
        // Null terminate the received data
        conn->buffer[bytes_read] = '\0';
        conn->buffer_length = bytes_read;
        
        // Update last activity time
        conn->last_activity = time(NULL);
        pthread_mutex_unlock(&conn->lock);
        
        // Process JSON command
        if (!connection_process_command(handler_arg, server->db, server->txn_manager)) {
            // Error processing command
            const char *error = json_create_error_response("Error processing command");
            connection_send_response(conn, error);
            free((void*)error);
        }
    }
    
    free(handler_arg);
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
        // Send length prefix (4 bytes, network byte order)
        uint32_t response_len = strlen(response);
        uint32_t net_len = htonl(response_len);
        send(conn->socket_fd, &net_len, 4, 0);
        
        // Send the actual response
        send(conn->socket_fd, response, response_len, 0);
    }
    pthread_mutex_unlock(&conn->lock);
}

// Process a JSON command from the client
bool connection_process_command(ClientHandlerArg *handlerArgs, Database *db, TransactionManager *txn_manager) {
    (void)db; // Mark as used
    (void)txn_manager; // Mark as used
    
    ClientConnection *conn = handlerArgs->connection;
    
    // Parse the JSON command
    cJSON *json = cJSON_Parse(conn->buffer);
    if (!json) {
        const char *error = json_create_error_response("Invalid JSON");
        connection_send_response(conn, error);
        free((void*)error);
        return false;
    }
    
    // Extract command type
    cJSON *command_item = cJSON_GetObjectItem(json, "command");
    if (!command_item || !cJSON_IsString(command_item)) {
        const char *error = json_create_error_response("Missing or invalid command field");
        connection_send_response(conn, error);
        free((void*)error);
        cJSON_Delete(json);
        return false;
    }
    
    const char *command = cJSON_GetStringValue(command_item);
    char text_command[MAX_BUFFER_SIZE] = {0};
    
    // Convert JSON command to text format for processing
    if (json_to_text_command(json, text_command, sizeof(text_command))) {
        // Use per-connection session state
        if (!conn->session_input_buf) {
            conn->session_input_buf = newInputBuffer();
        }

        // Use a response buffer
        char response_buf[MAX_BUFFER_SIZE] = {0};
        
        // Process the text command
        process_command_for_server(text_command, strlen(text_command), &conn->session_db, 
                                 conn->session_input_buf, response_buf, sizeof(response_buf));
        
        // Convert response back to JSON and send
        cJSON *response_json = cJSON_CreateObject();
        if (strlen(response_buf) > 0) {
            // Clean up the response text by removing trailing newlines and control chars
            char *clean_response = response_buf;
            size_t len = strlen(clean_response);
            while (len > 0 && (clean_response[len-1] == '\n' || clean_response[len-1] == '\r' || 
                             clean_response[len-1] == '\t')) {
                clean_response[--len] = '\0';
            }
            
            cJSON_AddBoolToObject(response_json, "success", true);
            cJSON_AddStringToObject(response_json, "message", clean_response);
        } else {
            cJSON_AddBoolToObject(response_json, "success", true);
            cJSON_AddStringToObject(response_json, "message", "Command executed");
        }
        
        char *json_string = cJSON_Print(response_json);
        connection_send_response(conn, json_string);
        free(json_string);
        cJSON_Delete(response_json);
    } else {
        const char *error = json_create_error_response("Unsupported command");
        connection_send_response(conn, error);
        free((void*)error);
        cJSON_Delete(json);
        return false;
    }
    
    cJSON_Delete(json);
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

// Convert JSON command to text format for processing by the existing CLI handler
bool json_to_text_command(cJSON *json, char *text_command, size_t max_len) {
    cJSON *command_item = cJSON_GetObjectItem(json, "command");
    if (!command_item || !cJSON_IsString(command_item)) {
        return false;
    }
    
    const char *command = cJSON_GetStringValue(command_item);
    
    if (strcmp(command, "select") == 0) {
        // Handle SELECT command
        cJSON *table_item = cJSON_GetObjectItem(json, "table");
        cJSON *columns_item = cJSON_GetObjectItem(json, "columns");
        cJSON *where_item = cJSON_GetObjectItem(json, "where");
        
        if (!table_item || !cJSON_IsString(table_item)) {
            return false;
        }
        
        // Start building SELECT statement
        snprintf(text_command, max_len, "SELECT ");
        
        if (columns_item && cJSON_IsArray(columns_item)) {
            int array_size = cJSON_GetArraySize(columns_item);
            for (int i = 0; i < array_size; i++) {
                cJSON *col = cJSON_GetArrayItem(columns_item, i);
                if (cJSON_IsString(col)) {
                    const char *col_name = cJSON_GetStringValue(col);
                    if (i > 0) {
                        strncat(text_command, ", ", max_len - strlen(text_command) - 1);
                    }
                    strncat(text_command, col_name, max_len - strlen(text_command) - 1);
                }
            }
        } else {
            strncat(text_command, "*", max_len - strlen(text_command) - 1);
        }
        
        strncat(text_command, " FROM ", max_len - strlen(text_command) - 1);
        strncat(text_command, cJSON_GetStringValue(table_item), max_len - strlen(text_command) - 1);
        
        if (where_item && cJSON_IsObject(where_item)) {
            cJSON *column = cJSON_GetObjectItem(where_item, "column");
            cJSON *operator = cJSON_GetObjectItem(where_item, "operator");
            cJSON *value = cJSON_GetObjectItem(where_item, "value");
            
            if (column && operator && value && 
                cJSON_IsString(column) && cJSON_IsString(operator)) {
                
                strncat(text_command, " WHERE ", max_len - strlen(text_command) - 1);
                strncat(text_command, cJSON_GetStringValue(column), max_len - strlen(text_command) - 1);
                strncat(text_command, " ", max_len - strlen(text_command) - 1);
                strncat(text_command, cJSON_GetStringValue(operator), max_len - strlen(text_command) - 1);
                strncat(text_command, " ", max_len - strlen(text_command) - 1);
                
                if (cJSON_IsString(value)) {
                    strncat(text_command, "\"", max_len - strlen(text_command) - 1);
                    strncat(text_command, cJSON_GetStringValue(value), max_len - strlen(text_command) - 1);
                    strncat(text_command, "\"", max_len - strlen(text_command) - 1);
                } else if (cJSON_IsNumber(value)) {
                    char num_str[32];
                    snprintf(num_str, sizeof(num_str), "%g", cJSON_GetNumberValue(value));
                    strncat(text_command, num_str, max_len - strlen(text_command) - 1);
                }
            }
        }
        
        return true;
    }
    else if (strcmp(command, "insert") == 0) {
        // Handle INSERT command
        cJSON *table_item = cJSON_GetObjectItem(json, "table");
        cJSON *values_item = cJSON_GetObjectItem(json, "values");
        
        if (!table_item || !cJSON_IsString(table_item) || 
            !values_item || !cJSON_IsArray(values_item)) {
            return false;
        }
        
        snprintf(text_command, max_len, "INSERT INTO %s VALUES (", 
                cJSON_GetStringValue(table_item));
        
        int array_size = cJSON_GetArraySize(values_item);
        for (int i = 0; i < array_size; i++) {
            cJSON *value = cJSON_GetArrayItem(values_item, i);
            if (i > 0) {
                strncat(text_command, ", ", max_len - strlen(text_command) - 1);
            }
            
            if (cJSON_IsString(value)) {
                strncat(text_command, "\"", max_len - strlen(text_command) - 1);
                strncat(text_command, cJSON_GetStringValue(value), max_len - strlen(text_command) - 1);
                strncat(text_command, "\"", max_len - strlen(text_command) - 1);
            } else if (cJSON_IsNumber(value)) {
                char num_str[32];
                snprintf(num_str, sizeof(num_str), "%g", cJSON_GetNumberValue(value));
                strncat(text_command, num_str, max_len - strlen(text_command) - 1);
            }
        }
        strncat(text_command, ")", max_len - strlen(text_command) - 1);
        
        return true;
    }
    else if (strcmp(command, "begin") == 0) {
        snprintf(text_command, max_len, ".txn begin");
        return true;
    }
    else if (strcmp(command, "commit") == 0) {
        snprintf(text_command, max_len, ".txn commit");
        return true;
    }
    else if (strcmp(command, "rollback") == 0) {
        snprintf(text_command, max_len, ".txn rollback");
        return true;
    }
    else if (strcmp(command, "create_database") == 0) {
        cJSON *database_item = cJSON_GetObjectItem(json, "database");
        if (!database_item || !cJSON_IsString(database_item)) {
            return false;
        }
        snprintf(text_command, max_len, "CREATE DATABASE %s", 
                cJSON_GetStringValue(database_item));
        return true;
    }
    else if (strcmp(command, "use_database") == 0) {
        cJSON *database_item = cJSON_GetObjectItem(json, "database");
        if (!database_item || !cJSON_IsString(database_item)) {
            return false;
        }
        snprintf(text_command, max_len, "USE DATABASE %s", 
                cJSON_GetStringValue(database_item));
        return true;
    }
    else if (strcmp(command, "create_table") == 0) {
        cJSON *table_item = cJSON_GetObjectItem(json, "table");
        cJSON *columns_item = cJSON_GetObjectItem(json, "columns");
        
        if (!table_item || !cJSON_IsString(table_item) || 
            !columns_item || !cJSON_IsArray(columns_item)) {
            return false;
        }
        
        snprintf(text_command, max_len, "CREATE TABLE %s (", 
                cJSON_GetStringValue(table_item));
        
        int array_size = cJSON_GetArraySize(columns_item);
        for (int i = 0; i < array_size; i++) {
            cJSON *col = cJSON_GetArrayItem(columns_item, i);
            if (!cJSON_IsObject(col)) continue;
            
            cJSON *name = cJSON_GetObjectItem(col, "name");
            cJSON *type = cJSON_GetObjectItem(col, "type");
            cJSON *size = cJSON_GetObjectItem(col, "size");
            
            if (!name || !cJSON_IsString(name) || !type || !cJSON_IsString(type)) {
                continue;
            }
            
            if (i > 0) {
                strncat(text_command, ", ", max_len - strlen(text_command) - 1);
            }
            
            strncat(text_command, cJSON_GetStringValue(name), max_len - strlen(text_command) - 1);
            strncat(text_command, " ", max_len - strlen(text_command) - 1);
            strncat(text_command, cJSON_GetStringValue(type), max_len - strlen(text_command) - 1);
            
            if (size && cJSON_IsNumber(size)) {
                char size_str[32];
                snprintf(size_str, sizeof(size_str), "(%g)", cJSON_GetNumberValue(size));
                strncat(text_command, size_str, max_len - strlen(text_command) - 1);
            }
        }
        strncat(text_command, ")", max_len - strlen(text_command) - 1);
        
        return true;
    }
    else if (strcmp(command, "login") == 0) {
        // Handle LOGIN command
        cJSON *username_item = cJSON_GetObjectItem(json, "username");
        cJSON *password_item = cJSON_GetObjectItem(json, "password");
        
        if (!username_item || !cJSON_IsString(username_item) ||
            !password_item || !cJSON_IsString(password_item)) {
            return false;
        }
        
        snprintf(text_command, max_len, "LOGIN %s %s", 
                cJSON_GetStringValue(username_item),
                cJSON_GetStringValue(password_item));
        return true;
    }
    
    return false;
}