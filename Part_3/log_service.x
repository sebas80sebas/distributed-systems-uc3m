/*
 * log_service.x - RPC interface definition for log service
 */

/* Structure to pass log data */
struct log_data {
    string username<100>;  /* Username */
    string operation<256>; /* Operation performed */
    string timestamp<30>;  /* Date and time */
};

/* Program, version, and procedure definition */
program LOG_PROG {
    version LOG_VERS {
        int log_operation(log_data) = 1;
    } = 1;
} = 0x20000001;
