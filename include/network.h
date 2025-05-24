#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>
#include <stdint.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include "../include/database.h"
#include "../include/transaction.h"
#include "thread_pool.h"

// Configuration constants
#define DEFAULT_PORT 8080
#define MAX_CONNECTIONS 100
#define CONNECTION_TIMEOUT_SECONDS 60
#define MAX_BUFFER_SIZE 4096


// Client connection structure
typedef struct ClientConnection {
    int socket_fd;
    struct sockaddr_in address;
    
    // Buffer for receiving data
    char buffer[MAX_BUFFER_SIZE];
    size_t buffer_length;
    
    // Session state
    char current_database[256];
    uint32_t transaction_id;
    time_t last_activity;
    bool connected;
    
    // For ACL (future implementation)
    bool authenticated;
    char username[64];
    
    // Synchronization
    pthread_mutex_t lock;
} ClientConnection;

// Server structure
typedef struct {
    int server_fd;
    uint16_t port;
    bool running;
    
    // Thread pool for handling client connections
    ThreadPool *thread_pool;
    
    // Database reference
    Database *db;
    TransactionManager *txn_manager;
    
    // Connection monitoring
    pthread_t monitor_thread;
    ClientConnection **active_connections;
    int connection_count;
    pthread_mutex_t connections_lock;
} DatabaseServer;

typedef struct {
    DatabaseServer *server;
    ClientConnection *connection;
} ClientHandlerArg;

// Server functions
DatabaseServer* server_create(uint16_t port, Database *db, TransactionManager *txn_manager);
bool server_start(DatabaseServer *server);
void server_stop(DatabaseServer *server);

// Connection handling
ClientConnection* connection_create(int socket_fd, struct sockaddr_in address);
void connection_close(ClientConnection *conn);
bool connection_process_command(ClientConnection *conn, Database *db, TransactionManager *txn_manager);
void connection_send_response(ClientConnection *conn, const char *response);

// JSON handling
char* json_create_error_response(const char *message);
char* json_create_success_response(const char *data);
bool json_parse_command(const char *json_str, void *statement);

#endif // NETWORK_H