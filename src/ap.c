#include "ap.h"
#include "dhcpserver/dhcpserver.h"
#include "dnsserver/dnsserver.h"

char dnss_captive_site[64];

#if LWIP_MDNS_RESPONDER
    static void srv_txt(struct mdns_service *service, void *txt_userdata)
    {
        err_t res;
        LWIP_UNUSED_ARG(txt_userdata);

        res = mdns_resp_add_service_txtitem(service, "path=/", 6);
        LWIP_ERROR("mdns add service txt failed\n", (res == ERR_OK), return);
    }
#endif

static size_t get_mac_ascii(int idx, size_t chr_off, size_t chr_len, char *dest_in) {
    static const char hexchr[16] = "0123456789ABCDEF";
    uint8_t mac[6];
    char *dest = dest_in;
    assert(chr_off + chr_len <= (2 * sizeof(mac)));
    cyw43_hal_get_mac(idx, mac);
    for (; chr_len && (chr_off >> 1) < sizeof(mac); ++chr_off, --chr_len) {
        *dest++ = hexchr[mac[chr_off >> 1] >> (4 * (1 - (chr_off & 1))) & 0xf];
    }
    return dest - dest_in;
}


/// auth_mode : CYW43_AUTH_WPA2_AES_PSK or CYW43_AUTH_OPEN
int setup_connect_to_wifi(const char *ssid,
                          const char *password,
                          const char *site_name,
                          uint32_t auth_mode) {
    if (cyw43_arch_init()) {
        printf("Erro: falhou ao inicializar o módulo Wi-Fi.\n");
        return -1;
    }
    cyw43_arch_enable_sta_mode();
    
    char hostname[sizeof(CYW43_HOST_NAME) + 4];
    memcpy(&hostname[0], CYW43_HOST_NAME, sizeof(CYW43_HOST_NAME) - 1);
    get_mac_ascii(CYW43_HAL_MAC_WLAN0, 8, 4, &hostname[sizeof(CYW43_HOST_NAME) - 1]);
    hostname[sizeof(hostname) - 1] = '\0';
    netif_set_hostname(&cyw43_state.netif[CYW43_ITF_STA], hostname);

    while (cyw43_arch_wifi_connect_blocking(ssid, password, auth_mode) != 0) {
        printf("Tentando reconectar a '%s'...\n", ssid);
        sleep_ms(500);
    }
    struct netif *sta_if = &cyw43_state.netif[CYW43_ITF_STA];
    while (ip4_addr_isany_val(*netif_ip4_addr(sta_if))) {
        sleep_ms(100);
    }
    ip4_addr_t ip = *netif_ip4_addr(sta_if);
    printf("Wi-Fi conectado: SSID='%s', IP='%s'\n",
           ssid, ip4addr_ntoa(&ip));
    // Setup mdns
    #if LWIP_MDNS_RESPONDER
            cyw43_arch_lwip_begin();
            mdns_resp_init();
            printf("mdns host name %s.local\n", hostname);
        #if LWIP_VERSION_MAJOR >= 2 && LWIP_VERSION_MINOR >= 2
            mdns_resp_add_netif(&cyw43_state.netif[CYW43_ITF_STA], hostname);
            mdns_resp_add_service(&cyw43_state.netif[CYW43_ITF_STA], "pico_httpd", "_http", DNSSD_PROTO_TCP, 80, srv_txt, NULL);
        #else
            mdns_resp_add_netif(&cyw43_state.netif[CYW43_ITF_STA], hostname);
            mdns_resp_add_service(&cyw43_state.netif[CYW43_ITF_STA], "pico_httpd", "_http", DNSSD_PROTO_TCP, 80, srv_txt, NULL);
        #endif
            cyw43_arch_lwip_end();
    #endif
    return 0;
}

#define MAX_SCAN_RESULTS 32
typedef struct {
    cyw43_ev_scan_result_t results[MAX_SCAN_RESULTS];
    size_t count;
} flcc_env_t;
int _flcc_callback(void *env, const cyw43_ev_scan_result_t *result) {
    flcc_env_t *e = (flcc_env_t *)env;
    if (!result) return 0;
    for (size_t i = 0; i < e->count; i++) {
        if (memcmp(e->results[i].bssid, result->bssid, sizeof(result->bssid)) == 0) {
            return 0;
        }
    }
    if (e->count < MAX_SCAN_RESULTS) {
        e->results[e->count++] = *result;
        // DEBUG LOG
        // printf("NOVO: %.*s (canal %2d)\n", result->ssid_len, result->ssid, result->channel);
    }
    return 0;
}

int find_least_congested_channel(void) {
    cyw43_arch_enable_sta_mode();
    cyw43_wifi_scan_options_t options = {0};
    flcc_env_t env = { .count = 0 };
    if (cyw43_wifi_scan(&cyw43_state, &options, &env, _flcc_callback) != 0) {
        printf("Falha no scan de Wi-Fi\n");
        return 1;
    }    
    while(cyw43_wifi_scan_active(&cyw43_state)){
        printf(".");
        sleep_ms(1000);
    }
    int channel_counts[12] = {0};
    for (size_t i = 0; i < env.count; i++) {
        int ch = env.results[i].channel;
        if (ch >= 1 && ch <= 11) {
            channel_counts[ch]++;
        }
    }
    int best = 1;
    for (int ch = 2; ch <= 11; ch++) {
        if (channel_counts[ch] < channel_counts[best]) {
            best = ch;
        }
    }
    cyw43_arch_disable_sta_mode();
    return best;
}

int setup_access_point(const char *ssid, const char *password, const char *site_name) {
    if (cyw43_arch_init()) {
        printf("Erro ao inicializar Wi-Fi\n");
        return -1;
    }
    printf("Escaneando redes\n");
    int best_chan = find_least_congested_channel();
    printf("Melhor canal %d\n", best_chan);

    uint32_t auth = (password != NULL) ? CYW43_AUTH_WPA2_AES_PSK : CYW43_AUTH_OPEN;
    cyw43_arch_enable_ap_mode(ssid, password, auth);
    cyw43_wifi_ap_set_channel(&cyw43_state, best_chan);
    
    ip4_addr_t ip, mask, gw;
    IP4_ADDR(&ip,   192,168,4,1);
    IP4_ADDR(&mask, 255,255,255,0);
    IP4_ADDR(&gw,    0,  0,  0,0);
    netif_set_addr(&cyw43_state.netif[0], &ip, &mask, &gw);

    static dhcp_server_t dhcp;
    ip_addr_t dip, dnm;
    IP_ADDR4(&dip, 192,168,4,1);
    IP_ADDR4(&dnm, 255,255,255,0);
    dhcp_server_init(&dhcp, &dip, &dnm);

    static dns_server_t dns;
    if(site_name != NULL){
        strncpy(dnss_captive_site,site_name,sizeof(dnss_captive_site));
    } else {
        strncpy(dnss_captive_site,"picow.local",sizeof(dnss_captive_site));
    }
    dns_server_init(&dns, (ip_addr_t*)&ip, site_name);
    
    printf("AP pronto: SSID='%s', canal=%d, IP=192.168.4.1\n",ssid, best_chan);

    return 0;
}