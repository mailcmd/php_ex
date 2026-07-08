#include <sapi/embed/php_embed.h>
#include <zend.h>

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#define PHP_ERROR 0
#define PHP_OK 1

////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE

//// HEADER 
typedef struct {
    char **headers;
    int count;
    int capacity;
} header_list_t;

static header_list_t g_headers = {0};

static void add_header(const char *header) {
    if (g_headers.count + 1 >= g_headers.capacity) {
        g_headers.capacity = g_headers.capacity ? g_headers.capacity * 2 : 8;
        g_headers.headers = realloc(g_headers.headers, g_headers.capacity * sizeof(char*));
    }
    g_headers.headers[g_headers.count] = strdup(header);
    g_headers.count++;
}

static void php_clear_headers() {
    for (int i = 0; i < g_headers.count; i++) {
        free(g_headers.headers[i]);
    }
    g_headers.count = 0;
}


static int header_handler(sapi_header_struct *sapi_header,
                             sapi_header_op_enum op,
                             sapi_headers_struct *sapi_headers) {
    if (op == SAPI_HEADER_ADD || op == SAPI_HEADER_REPLACE) {
        add_header(sapi_header->header);
    }
    return 0; // Success
}


//// OUTPUT 
static char *g_output = NULL;
static size_t g_output_len = 0;
static size_t g_output_cap = 0;

static int output_handler(const char *str, unsigned int str_length) {
    if (!g_output) {
        g_output_cap = 1024;
        g_output = malloc(g_output_cap);
    }
    
    if (g_output_len + str_length + 1 > g_output_cap) {
        g_output_cap = (g_output_len + str_length + 1) * 2;
        g_output = realloc(g_output, g_output_cap);
    }
    
    memcpy(g_output + g_output_len, str, str_length);
    g_output_len += str_length;
    g_output[g_output_len] = '\0';
    
    return str_length;
}

static void php_clear_output() {
    free(g_output);
}


////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC API

int php_define_global(const char *varname, const char *key, const char *value) {
  char *define_code;
  asprintf(&define_code, "%s['%s'] = '%s'", varname, key, value);
  if (define_code) {
    zval dummy;
    ZVAL_NULL(&dummy);
    zend_eval_string(define_code, &dummy, "global_definitions");
    zval_ptr_dtor(&dummy);
    free(define_code);
    return PHP_OK;
  } else {
    return PHP_ERROR;
  }
}

int php_init(int argc, char **argv) {
    php_embed_module.ub_write = output_handler; 
    php_embed_module.header_handler = header_handler;
    if (php_embed_init(argc, argv) == FAILURE) {
        return PHP_ERROR;
    } else {
        return PHP_OK;
    }
}

void php_end() {
    php_embed_shutdown();
}

void php_create_context() {
    zend_activate();
}

void php_destroy_context() {
    zend_deactivate();
}

int php_define_get(const char *key, const char *value) {
  return php_define_global("$_GET", key, value) & php_define_global("$_REQUEST", key, value);
}

int php_define_cookie(const char *key, const char *value) {
  return php_define_global("$_COOKIE", key, value);
}

int php_define_post(const char *key, const char *value) {
  return php_define_global("$_POST", key, value) & php_define_global("$_REQUEST", key, value);
}

int php_run_script(const char *filename) {
    g_output = NULL;
    g_output_len = 0;
    g_output_cap = 0;
        
    zend_file_handle file_handle;

    // reset header_sent flag
    SG(headers_sent) = 0;
    PG(display_errors) = 1;

    memset(&file_handle, 0, sizeof(zend_file_handle));
    file_handle.type = ZEND_HANDLE_FILENAME;
    file_handle.filename = (char *)filename;
    file_handle.handle.fp = 0;
    file_handle.opened_path = NULL;
    file_handle.free_filename = 0;

    // Execute the script
    int result = php_execute_script(&file_handle);

    return result;
}

