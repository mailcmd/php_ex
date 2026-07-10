/***

    TODO LIST:
    =========

    - [x] PHP run timeout
    - [x] PHP return header
    - [x] Fork every request

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <libgen.h>

#include "ei.h"

#include "php.h"

#define FUNCTION_SUCCESS 0
#define FUNCTION_FAIL 1

#define MASTER_NODE "php_master@127.0.0.1"

#define EI_LOG_INFO(format, ...) printf("%s: " format "\n", "INFO", ##__VA_ARGS__)
#define EI_LOG_WARN(format, ...) fprintf(stderr, "%s: " format "\n", "WARNING", ##__VA_ARGS__)
#define EI_LOG_ERROR(format, ...) fprintf(stderr, "%s: " format "\n", "ERROR", ##__VA_ARGS__)

typedef struct {
    unsigned int id;
    char *caller_name;
    erlang_pid caller_pid;
    char *node_name;
    char *short_node_name;
} request_data_t;

////////////////////////////////////////////////////////////////////////////////////////////////
// HELPERS
void usage(const char* cmd) {
  fprintf(stderr, "\nUsage: \n");
  fprintf(stderr, "  %s <node_name> <secretcookie>\n\n", cmd);
}

void error_response(int conn_fd, request_data_t request_data, const char* msg) {
    ei_x_buff out_buf;

    EI_LOG_ERROR("[%u] %s", request_data.id, msg);

    ei_x_new(&out_buf);
    ei_x_encode_version(&out_buf);
    ei_x_encode_tuple_header(&out_buf, 4);
    ei_x_encode_atom(&out_buf, request_data.node_name);
    ei_x_encode_long(&out_buf, request_data.id);
    ei_x_encode_atom(&out_buf, "error");
    ei_x_encode_binary(&out_buf, msg, strlen(msg));

    // Send payload back directly to the caller's PID
    ei_send(conn_fd, &request_data.caller_pid, out_buf.buff, out_buf.index);
    ei_x_free(&out_buf);
}

void php_define_globals(const char *define_type,
                        int list_len,
                        ei_x_buff in_buf,
                        int *index,
                        int req_id) {
    for (int i = 0; i < list_len; i++) {
        int len;
        int type;
        char key[MAXATOMLEN];
        char value[MAXATOMLEN];

        ei_get_type(in_buf.buff, index, &type, &len);
        if (type != ERL_SMALL_TUPLE_EXT) {
            EI_LOG_WARN(
                        "[%u] %s param pair %d: Invalid pair, expect a small tuple (%d)",
                        req_id,
                        define_type,
                        i,
                        type
                        );
            continue;
        }
        if (ei_decode_tuple_header(in_buf.buff, index, &len) != 0 || len != 2) {
            EI_LOG_WARN(
                        "[%u] %s param pair %d: Invalid pair, expect a 2-tuple (%d)",
                        req_id,
                        define_type,
                        i,
                        type
                        );
            continue;
        }

        // extract the key
        ei_get_type(in_buf.buff, index, &type, &len);
        if (type == ERL_ATOM_EXT) {
            ei_decode_atom(in_buf.buff, index, key);
        } else if (type == ERL_BINARY_EXT) {
            ei_decode_binary(in_buf.buff, index, key, &len);
            key[len] = '\0';
        } else {
            EI_LOG_WARN(
                        "[%u] %s param %d: Invalid key type, expect a binary or an atom (%d)",
                        req_id,
                        define_type,
                        i,
                        type
                        );
            ei_skip_term(in_buf.buff, index);
            ei_skip_term(in_buf.buff, index);
            continue;
        }

        // extract the value
        ei_get_type(in_buf.buff, index, &type, &len);
        if (type != ERL_BINARY_EXT) {
            EI_LOG_WARN(
                        "%s param %d: Invalid value type, expect a binary (%d)",
                        define_type,
                        i,
                        type
                        );
            ei_skip_term(in_buf.buff, index);
            continue;
        }
        ei_decode_binary(in_buf.buff, index, value, &len);
        value[len] = '\0';
        int result;
        if (strcmp(define_type, "GET") == 0) {
            result = php_define_get(key, value);
        } else if (strcmp(define_type, "POST") == 0) {
            result = php_define_post(key, value);
        } else if (strcmp(define_type, "COOKIE") == 0) {
            result = php_define_cookie(key, value);
        }
        if (result != PHP_OK) {
            EI_LOG_ERROR("[%u] Failed defining $_%s ->'%s' = '%s'",
                         req_id,
                         define_type,
                         key,
                         value);
        } else {
            EI_LOG_INFO("[%u] Appending to $_%s -> '%s' => '%s'",
                        req_id,
                        define_type,
                        key,
                        value);
        }
    }
    ei_skip_term(in_buf.buff, index);
}

void php_add_headers(int list_len,
                     ei_x_buff in_buf,
                     int *index,
                     int req_id) {
    for (int i = 0; i < list_len; i++) {
        int len;
        int type;
        char key[MAXATOMLEN];
        char value[MAXATOMLEN];

        ei_get_type(in_buf.buff, index, &type, &len);
        if (type != ERL_SMALL_TUPLE_EXT) {
            EI_LOG_WARN(
                        "[%u] header pair %d: Invalid pair, expect a small tuple (%d)",
                        req_id,
                        i,
                        type
                        );
            continue;
        }
        if (ei_decode_tuple_header(in_buf.buff, index, &len) != 0 || len != 2) {
            EI_LOG_WARN(
                        "[%u] header pair %d: Invalid pair, expect a 2-tuple (%d)",
                        req_id,
                        i,
                        type
                        );
            continue;
        }

        // extract the key
        ei_get_type(in_buf.buff, index, &type, &len);
        if (type == ERL_ATOM_EXT) {
            ei_decode_atom(in_buf.buff, index, key);
        } else if (type == ERL_BINARY_EXT) {
            ei_decode_binary(in_buf.buff, index, key, &len);
            key[len] = '\0';
        } else {
            EI_LOG_WARN(
                        "[%u] header pair %d: Invalid key type, expect a binary or an atom (%d)",
                        req_id,
                        i,
                        type
                        );
            ei_skip_term(in_buf.buff, index);
            ei_skip_term(in_buf.buff, index);
            continue;
        }

        // extract the value
        ei_get_type(in_buf.buff, index, &type, &len);
        if (type != ERL_BINARY_EXT) {
            EI_LOG_WARN(
                        "[%u] header pair %d: Invalid value type, expect a binary (%d)",
                        req_id,
                        i,
                        type
                        );
            ei_skip_term(in_buf.buff, index);
            continue;
        }
        ei_decode_binary(in_buf.buff, index, value, &len);
        value[len] = '\0';
        int result;
        result = php_define_header(key, value);
        if (result != PHP_OK) {
            EI_LOG_ERROR("[%u] Failed defining header ->'%s' = '%s'",
                         req_id,
                         key,
                         value);
        } else {
            EI_LOG_INFO("[%u] Appending to header -> '%s' => '%s'",
                        req_id,
                        key,
                        value);
        }
    }
    ei_skip_term(in_buf.buff, index);
}

////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTIONS

// RETURN_OK {:<node_name>, request_id, body, headers}
ei_x_buff* run(ei_x_buff in_buf, int index, request_data_t request_data) {
    int arity;
    int type;
    int script_path_len;
    char *script_path;
    ei_x_buff *out_buf;

    // Decode run params (4th element, a n-tuple)
    if (ei_decode_tuple_header(in_buf.buff, &index, &arity) != 0) {
        return NULL;
    }

    // First tuple value MUST BE script_path
    ei_get_type(in_buf.buff, &index, &type, &script_path_len);
    if (type != ERL_BINARY_EXT) {
        return NULL;
    }

    // OPEN PHP CONTEXT
    php_create_context();

    script_path = malloc(script_path_len + 1);
    ei_decode_binary(in_buf.buff, &index, script_path, &script_path_len);
    script_path[script_path_len] = '\0';
    EI_LOG_INFO("[%s] [%u] Received 'run' request with param '%s'",
                request_data.short_node_name,
                request_data.id,
                script_path);

    char *dir = strdup(script_path);
    dir = dirname(dir);
    if (chdir(dir) != 0) {
        perror("chdir");
        free(dir);
        return NULL;
    }
    free(dir);

    // The remain tuple values MUST BE lists
    int list_len;
    // GETs values
    if (arity > 1) {
        ei_get_type(in_buf.buff, &index, &type, &list_len);
        if (type == ERL_LIST_EXT) {
            ei_decode_list_header(in_buf.buff, &index, &list_len);
            php_define_globals("GET", list_len, in_buf, &index, request_data.id);
        } else if (type == ERL_NIL_EXT){
            EI_LOG_INFO("[%s] [%u] No GET params",
                        request_data.short_node_name,
                        request_data.id);
            if (arity > 2) ei_skip_term(in_buf.buff, &index);
        } else {
            EI_LOG_WARN("[%s] [%u] GET params: Invalid type, expect a list (%d)",
                        request_data.short_node_name,
                        request_data.id,
                        type
                        );
            ei_skip_term(in_buf.buff, &index);
        }
    }
    // POSTs values
    if (arity > 2) {
        ei_get_type(in_buf.buff, &index, &type, &list_len);
        if (type == ERL_LIST_EXT) {
            ei_decode_list_header(in_buf.buff, &index, &list_len);
            php_define_globals("POST", list_len, in_buf, &index, request_data.id);
        } else if (type == ERL_NIL_EXT) {
            EI_LOG_INFO("[%s] [%u] No POST params",
                        request_data.short_node_name,
                        request_data.id);
            if (arity > 3) ei_skip_term(in_buf.buff, &index);
        } else {
            EI_LOG_WARN("[%s] [%u] POST params: Invalid type, expect a list (%d)",
                        request_data.short_node_name,
                        request_data.id,
                        type
                        );
            ei_skip_term(in_buf.buff, &index);
        }
    }
    // COOKIEs values
    if (arity > 3) {
        ei_get_type(in_buf.buff, &index, &type, &list_len);
        if (type == ERL_LIST_EXT) {
            ei_decode_list_header(in_buf.buff, &index, &list_len);
            php_define_globals("COOKIE", list_len, in_buf, &index, request_data.id);
        } else if (type == ERL_NIL_EXT){
            EI_LOG_INFO("[%s] [%u] No COOKIEs",
                        request_data.short_node_name,
                        request_data.id);
            ei_skip_term(in_buf.buff, &index);
        } else {
            EI_LOG_WARN("[%s] [%u] COOKIE params: Invalid type, expect a list (%d)",
                        request_data.short_node_name,
                        request_data.id,
                        type
                        );
            ei_skip_term(in_buf.buff, &index);
        }
    }
    // HEADERS values
    if (arity > 3) {
        ei_get_type(in_buf.buff, &index, &type, &list_len);
        if (type == ERL_LIST_EXT) {
            ei_decode_list_header(in_buf.buff, &index, &list_len);
            php_add_headers(list_len, in_buf, &index, request_data.id);
        } else if (type == ERL_NIL_EXT){
            EI_LOG_INFO("[%s] [%u] No HEADERs",
                        request_data.short_node_name,
                        request_data.id);
            ei_skip_term(in_buf.buff, &index);
        } else {
            EI_LOG_WARN("[%s] [%u] HEADERS: Invalid type, expect a list (%d)",
                        request_data.short_node_name,
                        request_data.id,
                        type
                        );
            ei_skip_term(in_buf.buff, &index);
        }
    }

    EI_LOG_INFO("[%s] [%u] Running script '%s'",
                request_data.short_node_name,
                request_data.id,
                script_path);

    int result = php_run_script(script_path);

    if (result == PHP_OK) {
        EI_LOG_INFO("[%s] [%u] Running ok!",
                    request_data.short_node_name,
                    request_data.id,
                    script_path);

        // close session if the script do not 
        php_session_close();
            
        // Build the reply payload tuple
        out_buf = malloc(sizeof(ei_x_buff));
        ei_x_new(out_buf);
        ei_x_encode_version(out_buf);
        ei_x_encode_tuple_header(out_buf, 4);
        ei_x_encode_atom(out_buf, request_data.node_name);
        ei_x_encode_long(out_buf, request_data.id);
        ei_x_encode_binary(out_buf, g_output, g_output_len);

        ei_x_encode_list_header(out_buf, g_headers.count);
        for (int i = 0; i < g_headers.count; i++) {
            ei_x_encode_binary(out_buf, g_headers.headers[i], strlen(g_headers.headers[i]));
        }
        ei_x_encode_empty_list(out_buf);

        php_clear_output();
        /* php_clear_error(); */
        php_clear_headers();

        free(script_path);

        // CLOSE PHP CONTEXT
        php_destroy_context();

        return out_buf;
    } else {
        EI_LOG_ERROR("[%s] [%u] Failed running '%s' (%d)",
                     request_data.short_node_name,
                     request_data.id,
                     script_path,
                     result);

        php_clear_headers();
        free(script_path);

        // CLOSE PHP CONTEXT
        php_destroy_context();
        return NULL;
    }
}


// MAIN
int main(int argc, char **argv) {
    /* int port = 0; // Port 0, chosen by OS */
    /* int listen_fd; */
    int conn_fd;
    /* struct sockaddr_in local_addr; */
    ei_cnode ec;
    ErlConnect connection;
    ei_x_buff in_buf, *out_buf;
    request_data_t request_data;

    if (argc < 2) {
      EI_LOG_ERROR("You must pass a node name");
      usage(argv[0]);
      return 1;
    }

    if (argc < 3) {
      EI_LOG_ERROR("You must pass the secretcookie");
      usage(argv[0]);
      return 1;
    }

    char *node_name = argv[1];
    char *secretcookie = argv[2];
    char *php_ini;

    if (argc == 4) {
        php_ini = argv[3];
    } else {
        php_ini = NULL;
    }
    
    char full_node_name[256];
    sprintf(full_node_name, "%s@127.0.0.1", node_name);

    // Init PHP
    php_init(php_ini);
    EI_LOG_INFO("[%s] PHP initialized", node_name);

    // Init ei node struct
    struct in_addr addr;
    inet_aton("127.0.0.1", &addr);
    int init = ei_connect_xinit(&ec,
                                "127.0.0.1",
                                node_name,                // node name (without @)
                                full_node_name,           // full node name
                                &addr,                    // struct in_addr (use INADDR_LOOPBACK)
                                secretcookie,             // cookie
                                0);                       // creator (0 for auto)

    if (init < 0) {
        EI_LOG_ERROR("[%s] Failed to initialize node", node_name);
        return 1;
    }
    EI_LOG_INFO("[%s] Node '%s' initialized", node_name, ec.thisalivename);

    // Connect to the master node
    EI_LOG_INFO("[%s] Connecting to php_master on %s", node_name, inet_ntoa(addr));
    conn_fd = ei_connect(&ec, MASTER_NODE);
    if (conn_fd < 0) {
        EI_LOG_ERROR("[%s] Failed connecting to PHP_MASTER (%s)", node_name, strerror(erl_errno));
        return 1;
    }
    EI_LOG_INFO("[%s] Connected to master node: %s", node_name, MASTER_NODE);

    signal(SIGCHLD, SIG_IGN);

    // Receive a message loop / handler
    ei_x_new(&in_buf);
    erlang_msg msg;

    while (1) {
        ei_x_free(&in_buf);
        ei_x_new(&in_buf);
        out_buf = NULL;

        int status = ei_xreceive_msg(conn_fd, &msg, &in_buf);
        if (status == ERL_TICK) {
            continue; // Keep-alive heartbeats from Erlang
        } else if (status == ERL_ERROR) {
            EI_LOG_ERROR("[%s] Connection closed or error occurred.", node_name);
            break;
        }

        // We specifically look for standard message passing (REG_SEND or SEND)
        if (msg.msgtype == ERL_REG_SEND || msg.msgtype == ERL_SEND) {
            EI_LOG_INFO("[%s] Message received from node '%s'", node_name, msg.toname);

            request_data.caller_name = msg.toname;
            request_data.node_name = ec.thisnodename;
            request_data.short_node_name = node_name;

            int index = 0;
            int version;
            int arity;
            char function_name[MAXATOMLEN];

            // Decode version header
            ei_decode_version(in_buf.buff, &index, &version);

            // Expecting a 4-tuple payload format: {CallerPid, RequestId, FuncName, Data}
            if (ei_decode_tuple_header(in_buf.buff, &index, &arity) == 0 && arity == 4) {
                // Decode Caller Pid (1st element)
                ei_decode_pid(in_buf.buff, &index, &request_data.caller_pid);
                EI_LOG_INFO("[%s] Caller ID: '%x'", node_name, request_data.caller_pid);

                // Decode Request Id (2nd element)
                ei_decode_long(in_buf.buff, &index, &request_data.id);
                EI_LOG_INFO("[%s] Request ID: '%u'", node_name, request_data.id);

                // Decode Function Name (3rd element)
                ei_decode_atom(in_buf.buff, &index, function_name);

                /// SWITCH OF FUNCTIONS
                ///// RUN
                if (strcmp(function_name, "run") == 0) {
                    // Fork the process
                    EI_LOG_INFO("[%s] Forking process...", node_name);
                    pid_t pid = fork();
                    if (pid == -1) {
                        EI_LOG_ERROR("[%s] [%u] There was a problem forking the process",
                                     node_name,
                                     request_data.id);
                        error_response(conn_fd, request_data, "Problems forking c-node");
                        continue;
                    } else if (pid == 0) {
                        // Child process
                        EI_LOG_INFO("[%s] [%u] Forked process, running script",
                                    node_name,
                                    request_data.id);
                        out_buf = run(in_buf, index, request_data);
                    } else {
                        // Parent process
                        continue;
                    }

                ///// EXIT
                } else if (strcmp(function_name, "exit") == 0) {
                    break;

                //// UNKNOWN
                } else {
                    error_response(conn_fd, request_data, "Unknown request");
                    continue;
                }

                /* PARENT PROCESS DOES NOT REACH THIS */
                // IF FUNCTION SUCCESS, SEND RESPONSE
                if (out_buf) {
                    ei_send(conn_fd, &request_data.caller_pid, out_buf->buff, out_buf->index);
                    ei_x_free(out_buf);
                    free(out_buf);
                    // IT IS THE FORKED PROCESS, SO I QUIT
                    close(conn_fd);
                    EI_LOG_INFO("[%s] [%u] Closing forked process", node_name, request_data.id);
                    exit(0);
                } else {
                    if (PG(last_error_message)) {
                        error_response(conn_fd, request_data, PG(last_error_message));
                    } else {
                        error_response(conn_fd, request_data,
                                       "Problems running script or malformed function params");
                    }
                    // IT IS THE FORKED PROCESS, SO I QUIT
                    close(conn_fd);
                    EI_LOG_INFO("[%s] [%u] Closing forked process", node_name, request_data.id);
                    exit(0);
                }
            } else {
                EI_LOG_ERROR("Malformed request");
            }
        }
    }

    php_end();
    close(conn_fd);
    return 0;
}
