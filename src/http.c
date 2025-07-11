#include "http.h"
#include "websocket.h"


#define HASHMAP_SEARCH
#ifdef HASHMAP_SEARCH
    #define STB_DS_IMPLEMENTATION
    #include "stb_ds.h"
#endif

// #define LINEAR_DYNAMIC_ARRAY_SEARCH

static http_routes_t* http_routes = NULL;
static new_schemas_routes_t* new_schemas_routes = NULL;
static http_routes_hashmap_t* http_routes_hmap = NULL;
static new_schemas_hashmap_t* new_schemas_hmap = NULL;

char http_response[HTTP_RESPONSE_BUFFER_SIZE];
char payload_temp_buff[PAYLOAD_TEMP_BUFFER_SIZE];
const char* new_schema_http_header_field = {"Upgrade: "};

char resp302[KB(1) / 2];
const char* captive_site_routes[] = {"GET /generate_204","GET /hotspot-detect.html","GET /connecttest.txt","GET /redirect"};
bool dns_captive_site_response(struct tcp_pcb *tpcb,struct pbuf *p,char* payload){
    bool in_captive_site_routes = false;
    for (int i = 0; i < count_of(captive_site_routes); i++){
        in_captive_site_routes = in_captive_site_routes || strstr(payload,captive_site_routes[i]);
    }
    if(in_captive_site_routes){
        int r = snprintf(resp302, sizeof(resp302),DNS_CAPTIVE_RESPONSE_HEADER,dnss_captive_site);
        tcp_write(tpcb, resp302, r, TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);
    }
    return in_captive_site_routes;
}


void add_http_route(const char* route_path, route_response_handler_t route_response_handler){
    #ifdef LINEAR_DYNAMIC_ARRAY_SEARCH
        if(http_routes == NULL){
            http_routes = (http_routes_t*)calloc(1,sizeof(http_routes_t));
        } 
        
        if (http_routes->count >= http_routes->capacity) {                                         
            if (http_routes->capacity == 0) http_routes->capacity = 16;                           
            else http_routes->capacity *= 2;                                                
            http_routes->items = realloc(http_routes->items, http_routes->capacity*sizeof(*http_routes->items)); 
        }                                           
        http_routes->items[http_routes->count++] = (http_route_t){route_path,strlen(route_path),route_response_handler};
    #endif
    #ifdef HASHMAP_SEARCH
        http_route_item_t item = (http_route_item_t){strlen(route_path),route_response_handler};
        shput(http_routes_hmap,route_path,item);
    #endif
};

// NEW SCHEMAS ROUTES SHOULD INHERIT THE CONNECTION FROM THE HTTP CALLBACK HANDLER TROUGH "Upgrade" FIELD
// ONCE THE HANDLER IS CALLED THE HTTP CALLBACK SHOULD BE REMOVED FROM THE TCP_RECV
void add_new_schema_route(const char* new_schema, new_schema_handler_t new_schema_handler){
    if(new_schemas_routes == NULL){
        new_schemas_routes = (new_schemas_routes_t*)calloc(1,sizeof(new_schemas_routes_t));
    } 
    
    if (new_schemas_routes->count >= new_schemas_routes->capacity) {                                         
        if (new_schemas_routes->capacity == 0) new_schemas_routes->capacity = 4;                           
        else new_schemas_routes->capacity *= 2;                                                
        new_schemas_routes->items = realloc(new_schemas_routes->items, new_schemas_routes->capacity*sizeof(*new_schemas_routes->items)); 
    }
    
    size_t len = strlen(new_schema)+sizeof(new_schema_http_header_field)+1;
    char* new_schema_with_upgrade = (char*)calloc(1,len);
    snprintf(new_schema_with_upgrade, len, "%s%s", new_schema_http_header_field, new_schema);

    new_schemas_routes->items[new_schemas_routes->count++] = (new_schema_route_t){new_schema_with_upgrade,new_schema_handler};
};

static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        if (tpcb->state != CLOSED && tpcb->state != TIME_WAIT) tcp_close(tpcb);
        return ERR_OK;
    }
    tcp_recved(tpcb, p->tot_len);

    int len = p->tot_len < sizeof(payload_temp_buff) - 1
                  ? p->tot_len
                  : sizeof(payload_temp_buff) - 1;
    
    memcpy(payload_temp_buff, p->payload, len);
    payload_temp_buff[len] = '\0';
    
    
    for (size_t i = 0; i < new_schemas_routes->count; ++i){
        if(strstr(payload_temp_buff,new_schemas_routes->items[i].new_schema)){
            return new_schemas_routes->items[i].new_schema_handler(payload_temp_buff,tpcb,p);
        }
    }
    
    char *line_end = strstr(payload_temp_buff, "\r\n");
    if (!line_end) line_end = payload_temp_buff + len;
    *line_end = '\0';
    
    
    char method[8], path[256];
    char query_parameters[512] = {0};
    char *query_params_point = strstr(payload_temp_buff, "?");

    if(query_params_point){
        *query_params_point = ' ';
        if (sscanf(payload_temp_buff, "%7s %255s %511s", method, path, query_parameters) != 3) {
            goto send_404;
        }
    } else{
        if (sscanf(payload_temp_buff, "%7s %255s", method, path) != 2) {
            goto send_404;
        }
    }
    
    if (dns_captive_site_response(tpcb, p, payload_temp_buff)) {
        pbuf_free(p);
        tcp_close(tpcb);
        return ERR_OK;
    }

    if (strcmp(method, "GET") == 0) {
        

            #ifdef LINEAR_DYNAMIC_ARRAY_SEARCH

                for (size_t i = 0; i < http_routes->count; ++i) {
                    if (strcmp(path, http_routes->items[i].route_path) == 0) {
                        http_routes->items[i]
                            .route_response_handler(http_response, sizeof(http_response));
                        tcp_write(tpcb, http_response, strlen(http_response),
                                TCP_WRITE_FLAG_COPY);
                        tcp_output(tpcb);
                        pbuf_free(p);
                        tcp_close(tpcb);
                        return ERR_OK;
                    }
                }
            
            #endif

            #ifdef HASHMAP_SEARCH

                http_route_item_t found = shget(http_routes_hmap,path);
                if (found.route_response_handler){
                    found.route_response_handler(query_parameters,http_response, sizeof(http_response));
                    tcp_write(tpcb, http_response, strlen(http_response),TCP_WRITE_FLAG_COPY);
                    tcp_output(tpcb);
                    pbuf_free(p);
                    tcp_close(tpcb);
                    return ERR_OK;
                }

            #endif

    }

send_404:
    {
        const char *resp404 =
            "HTTP/1.1 404 Not Found\r\n"
            "Connection: close\r\n"
            "Content-Length: 0\r\n"
            "\r\n";
        tcp_write(tpcb, resp404, strlen(resp404), TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);
        pbuf_free(p);
        tcp_close(tpcb);
    }

    return ERR_OK;
}

static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_callback);
    return ERR_OK;
}

void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        printf("Erro ao criar PCB\n");
        return;
    }
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }
    
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP rodando na porta 80...\n");
}