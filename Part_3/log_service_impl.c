/*
 * RPC logging service implementation
 * Contains only the service function implementation
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log_service.h"

/* 
 * Implementation of the log_operation RPC procedure
 */
int *log_operation_1_svc(log_data *data, struct svc_req *rqstp) {
    static int result = 1;
    
    /* Print operation information */
    printf("%s %s %s\n", data->username, data->operation, data->timestamp);
    fflush(stdout);
    
    return &result;
}
