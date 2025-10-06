#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tirpc/rpc/rpc.h>
#include "log_service.h"

/* 
 * Client function to send logs to RPC server
 * This function should be called from the main server
 */
void send_log(const char *username, const char *operation, const char *timestamp) {
    CLIENT *cl;
    int *result;
    char *rpc_server;
    log_data data;
    
    /* Get RPC server address from environment variable */
    rpc_server = getenv("LOG_RPC_IP");
    if (rpc_server == NULL) {
        fprintf(stderr, "Error: Environment variable LOG_RPC_IP not defined\n");
        return;
    }
    
    /* Initialize data structure */
    data.username = (char *)username;
    data.operation = (char *)operation;
    data.timestamp = (char *)timestamp;
    
    /* Connect to RPC server */
    cl = clnt_create(rpc_server, LOG_PROG, LOG_VERS, "tcp");
    if (cl == NULL) {
        clnt_pcreateerror(rpc_server);
        return;
    }
    
    /* Call remote procedure */
    result = log_operation_1(&data, cl);
    if (result == NULL) {
        clnt_perror(cl, "Error in RPC call");
    }
    
    /* Free resources */
    clnt_destroy(cl);
}
