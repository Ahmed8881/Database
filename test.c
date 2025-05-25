#include "include/network.h"
#include "include/thread_pool.h"
#include "include/json_formatter.h"
#include "include/database.h"
#include "include/transaction.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

// Global server pointer for signal handling
DatabaseServer *g_server = NULL;

// Signal handler for clean shutdown
void handle_signal(int sig) {
    printf("\nReceived signal %d, shutting down server...\n", sig);
    if (g_server) {
        server_stop(g_server);
    }
    exit(0);
}

int main() {
    printf("Database Server\n");
    printf("---------------\n");
    
    // Initialize a database and transaction manager
    Database db;
    memset(&db, 0, sizeof(Database));
    strcpy(db.name, "test_db");
    
    TransactionManager txn_manager;
    memset(&txn_manager, 0, sizeof(TransactionManager));
    
    // Create the server on port 8080
    printf("Creating database server on port 8080...\n");
    DatabaseServer *server = server_create(8080, &db, &txn_manager);
    
    if (!server) {
        printf("Failed to create server!\n");
        return 1;
    }
    
    // Store server pointer for signal handler
    g_server = server;
    
    // Set up signal handling for Ctrl+C
    signal(SIGINT, handle_signal);
    
    // Start the actual server
    printf("Starting server...\n");
    if (!server_start(server)) {
        printf("Failed to start server!\n");
        return 1;
    }
    
    printf("Server started successfully!\n");
    printf("Listening on port 8080\n");
    printf("Press Ctrl+C to stop the server\n");
    
    // The server is now running in its own threads
    // Main thread can wait here
    while (server->running) {
        sleep(1);
    }
    
    printf("Server stopped\n");
    return 0;
}
