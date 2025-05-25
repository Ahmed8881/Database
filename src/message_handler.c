#include "../include/protocol.h"
#include "../include/network.h"
#include "../include/command_processor.h"
#include "../include/json_formatter.h"
#include <string.h>
#include <stdlib.h>

// Static function declarations
static bool handle_login_request(ClientConnection* conn, const char* username, const char* password);
static bool handle_execute_request(ClientConnection* conn, const char* sql);
static bool handle_meta_request(ClientConnection* conn, const char* command);
static bool handle_status_requests(ClientConnection* conn, ResponseType type);

bool validate_request_header(const RequestHeader* header) {
    if (header->version != 1) {
        return false;
    }
    if (header->length == 0) {
        return false;
    }
    return true;
}

bool validate_response_header(const ResponseHeader* header) {
    if (header->version != 1) {
        return false;
    }
    if (header->request_id == 0) {
        return false;
    }
    return true;
}

bool process_client_message(ClientConnection* conn, const char* message, size_t length) {
    // Parse request header
    RequestHeader header;
    if (length < sizeof(RequestHeader)) {
        return false;
    }
    
    memcpy(&header, message, sizeof(RequestHeader));
    
    if (!validate_request_header(&header)) {
        return false;
    }

    // Process based on request type
    switch (header.type) {
        case REQUEST_LOGIN: {
            AuthRequest auth_req;
            if (length != sizeof(RequestHeader) + sizeof(AuthRequest)) {
                return false;
            }
            memcpy(&auth_req, message + sizeof(RequestHeader), sizeof(AuthRequest));
            return handle_login_request(conn, auth_req.username, auth_req.password);
        }
        case REQUEST_EXECUTE: {
            ExecuteRequest exec_req;
            if (length != sizeof(RequestHeader) + sizeof(ExecuteRequest)) {
                return false;
            }
            memcpy(&exec_req, message + sizeof(RequestHeader), sizeof(ExecuteRequest));
            return handle_execute_request(conn, exec_req.sql);
        }
        case REQUEST_META: {
            MetaRequest meta_req;
            if (length != sizeof(RequestHeader) + sizeof(MetaRequest)) {
                return false;
            }
            memcpy(&meta_req, message + sizeof(RequestHeader), sizeof(MetaRequest));
            return handle_meta_request(conn, meta_req.command);
        }
        case REQUEST_AUTH_STATUS:
        case REQUEST_DATABASE_STATUS:
        case REQUEST_TRANSACTION_STATUS: {
            return handle_status_requests(conn, header.type);
        }
        case REQUEST_LOGOUT:
            conn->authenticated = false;
            return true;
        case REQUEST_PING:
            return true;
        default:
            return false;
    }
}

static bool handle_login_request(ClientConnection* conn, const char* username, const char* password) {
    // Authenticate user
    bool auth_result = auth_verify_user(conn->db->user_manager, username, password);
    
    // Create response
    ResponseHeader resp_header = {
        .version = 1,
        .type = RESPONSE_AUTH_STATUS,
        .length = sizeof(AuthStatusResponse),
        .request_id = conn->last_request_id,
        .error_code = auth_result ? ERROR_SUCCESS : ERROR_AUTH_FAILED
    };
    
    AuthStatusResponse status = {
        .authenticated = auth_result,
        .username = auth_result ? username : ""
    };
    
    // Send response
    char buffer[MAX_BUFFER_SIZE];
    memcpy(buffer, &resp_header, sizeof(resp_header));
    memcpy(buffer + sizeof(resp_header), &status, sizeof(status));
    
    connection_send_response(conn, buffer, sizeof(resp_header) + sizeof(status));
    
    return true;
}

static bool handle_execute_request(ClientConnection* conn, const char* sql) {
    if (!conn->authenticated) {
        // Send error response
        ResponseHeader resp_header = {
            .version = 1,
            .type = RESPONSE_ERROR,
            .length = 0,
            .request_id = conn->last_request_id,
            .error_code = ERROR_PERMISSION_DENIED
        };
        connection_send_response(conn, (char*)&resp_header, sizeof(resp_header));
        return false;
    }

    // Process SQL command
    Statement stmt;
    if (!prepare_statement(sql, &stmt)) {
        ResponseHeader resp_header = {
            .version = 1,
            .type = RESPONSE_ERROR,
            .length = 0,
            .request_id = conn->last_request_id,
            .error_code = ERROR_SYNTAX_ERROR
        };
        connection_send_response(conn, (char*)&resp_header, sizeof(resp_header));
        return false;
    }

    // Execute statement
    switch (stmt.type) {
        case STATEMENT_SELECT: {
            // Execute query and get results
            DynamicRow** rows;
            int row_count = execute_select(&stmt, &rows);
            
            // Format as JSON
            char* json_result = create_json_result_string(rows, row_count, stmt.table_def,
                                                         stmt.columns_to_select, stmt.num_columns_to_select);
            
            // Create response
            ResultSetResponse result = {
                .row_count = row_count,
                .column_count = stmt.num_columns_to_select,
                .data = json_result
            };
            
            ResponseHeader resp_header = {
                .version = 1,
                .type = RESPONSE_RESULT_SET,
                .length = sizeof(ResultSetResponse) + strlen(json_result),
                .request_id = conn->last_request_id,
                .error_code = ERROR_SUCCESS
            };
            
            // Send response
            char buffer[MAX_BUFFER_SIZE];
            memcpy(buffer, &resp_header, sizeof(resp_header));
            memcpy(buffer + sizeof(resp_header), &result, sizeof(result));
            connection_send_response(conn, buffer, sizeof(resp_header) + sizeof(result));
            
            free(json_result);
            break;
        }
        case STATEMENT_INSERT:
        case STATEMENT_UPDATE:
        case STATEMENT_DELETE: {
            int affected_rows = execute_statement(&stmt);
            
            ResponseHeader resp_header = {
                .version = 1,
                .type = RESPONSE_OK,
                .length = 0,
                .request_id = conn->last_request_id,
                .error_code = ERROR_SUCCESS
            };
            
            connection_send_response(conn, (char*)&resp_header, sizeof(resp_header));
            break;
        }
        default:
            // Send error response
            ResponseHeader resp_header = {
                .version = 1,
                .type = RESPONSE_ERROR,
                .length = 0,
                .request_id = conn->last_request_id,
                .error_code = ERROR_INVALID_COMMAND
            };
            connection_send_response(conn, (char*)&resp_header, sizeof(resp_header));
            return false;
    }

    return true;
}

static bool handle_meta_request(ClientConnection* conn, const char* command) {
    if (!conn->authenticated) {
        // Send error response
        ResponseHeader resp_header = {
            .version = 1,
            .type = RESPONSE_ERROR,
            .length = 0,
            .request_id = conn->last_request_id,
            .error_code = ERROR_PERMISSION_DENIED
        };
        connection_send_response(conn, (char*)&resp_header, sizeof(resp_header));
        return false;
    }

    // Process meta command
    MetaCommandResult result = process_meta_command(command, conn->db);
    
    // Create response
    ResponseHeader resp_header = {
        .version = 1,
        .type = RESPONSE_OK,
        .length = 0,
        .request_id = conn->last_request_id,
        .error_code = result == META_COMMAND_SUCCESS ? ERROR_SUCCESS : ERROR_INVALID_COMMAND
    };
    
    connection_send_response(conn, (char*)&resp_header, sizeof(resp_header));
    return true;
}

static bool handle_status_requests(ClientConnection* conn, ResponseType type) {
    ResponseHeader resp_header = {
        .version = 1,
        .type = type,
        .length = 0,
        .request_id = conn->last_request_id,
        .error_code = ERROR_SUCCESS
    };
    
    char buffer[MAX_BUFFER_SIZE];
    memcpy(buffer, &resp_header, sizeof(resp_header));
    
    switch (type) {
        case RESPONSE_AUTH_STATUS: {
            AuthStatusResponse status = {
                .authenticated = conn->authenticated,
                .username = conn->authenticated ? conn->username : ""
            };
            memcpy(buffer + sizeof(resp_header), &status, sizeof(status));
            break;
        }
        case RESPONSE_DATABASE_STATUS: {
            DatabaseStatusResponse status = {
                .database_open = conn->db != NULL,
                .database_name = conn->db ? conn->db->name : ""
            };
            memcpy(buffer + sizeof(resp_header), &status, sizeof(status));
            break;
        }
        case RESPONSE_TRANSACTION_STATUS: {
            TransactionStatusResponse status = {
                .in_transaction = conn->transaction_id != 0,
                .transaction_id = conn->transaction_id
            };
            memcpy(buffer + sizeof(resp_header), &status, sizeof(status));
            break;
        }
    }
    
    connection_send_response(conn, buffer, sizeof(resp_header) + resp_header.length);
    return true;
}
