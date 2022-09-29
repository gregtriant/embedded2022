#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <iostream>
#include <signal.h>
#include <libwebsockets.h>
#include <ArduinoJson.hpp>
#include <boost/filesystem.hpp> 
namespace fs = boost::filesystem;


#define KGRN "\033[0;32;32m"
#define KCYN "\033[0;36m"
#define KRED "\033[0;32;31m"
#define KYEL "\033[1;33m"
#define KBLU "\033[0;32;34m"
#define KCYN_L "\033[1;36m"
#define KBRN "\033[0;33m"
#define RESET "\033[0m"

#define EXAMPLE_RX_BUFFER_BYTES (20*1024) 

static int destroy_flag = 0;
static int connection_flag = 0;
static int writeable_flag = 0;
static int DO_WORK = 1;

typedef struct symb {
    std::string symbol;
    long total_times;
    long prev_total_1_min;
    long current_1_min;
    long prev_total_15_min;
    long current_15_min;
} Symbol;

typedef struct fd {
    float time_recieved;
    float time_sent;
    float price;
    float volume;
} Filedata;

std::vector<Symbol> all_symbols;

unsigned int msleep(unsigned int tms) {
  return usleep(tms * 1000);
}

void print_symbol(Symbol s) {
    std::cout << s.symbol << "(" << s.total_times << ", " << s.prev_total_1_min << ", " << s.current_1_min << ", " << s.prev_total_15_min << ", " << s.current_15_min << ")" << std::endl;
}

void writeToFile(std::string filename, Filedata fd) {
    FILE *fptr;
    fptr = fopen(filename.c_str(), "ab");
    fwrite(&fd, sizeof(fd), 1, fptr);
    fclose(fptr);
}

unsigned long long getTimeNow() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long long millisecondsSinceEpoch = (unsigned long long)(tv.tv_sec) * 1000 + (unsigned long long)(tv.tv_usec) / 1000;
    return millisecondsSinceEpoch;
}

// for threads
void *do_every_min(void *vargp) {
    while(DO_WORK) {
        std::cout << "\n--- " << getTimeNow() << " Short time passed, working... ---" << std::endl;
        for(int i=0; i<all_symbols.size(); i++) {
            all_symbols[i].current_1_min = all_symbols[i].total_times - all_symbols[i].prev_total_1_min;
            print_symbol(all_symbols[i]);

            all_symbols[i].prev_total_1_min = all_symbols[i].total_times;
        }   
        msleep(4* 1000);
    }
    return NULL;
}

void *do_every_15_min(void *vargp) {
    while(DO_WORK) {
        std::cout << "\n------- " << getTimeNow() << " LONG time passed -------" << std::endl;
        for(int i=0; i<all_symbols.size(); i++) {
            all_symbols[i].current_15_min = all_symbols[i].total_times - all_symbols[i].prev_total_15_min;
            all_symbols[i].prev_total_15_min = all_symbols[i].total_times;
        }   
        msleep(10* 1000);
    }
    return NULL;
}


void clientRecievedData(char* in)
{
    ArduinoJson::DynamicJsonDocument doc(EXAMPLE_RX_BUFFER_BYTES);
    ArduinoJson::deserializeJson(doc, in);
    const char* type = doc["type"];
    if (strcmp(type, "trade") == 0) {
        ArduinoJson::JsonArray array = doc["data"].as<ArduinoJson::JsonArray>();
        
        for(ArduinoJson::JsonObject nested : array) {
            float price = nested["p"];
            float vol = nested["v"];
            float timestamp = nested["t"];
            const char *symbol = nested["s"];
            std::string symbolString(symbol);
            std::replace(symbolString.begin(), symbolString.end(), ':', '_');
            
            bool found = false;
            for(int i=0; i<all_symbols.size(); i++) {
                
                if(all_symbols[i].symbol == symbolString) {
                    found = true;
                    all_symbols[i].total_times++;
                }
            }
            if (found == false) { // new symbol found
                printf("new symbol found: %s\n", symbol);
                Symbol s;
                s.symbol = symbolString;
                s.total_times = 1;
                s.prev_total_1_min = 0;
                s.prev_total_15_min = 0;
                s.current_1_min = 0;
                s.current_15_min = 0;
                all_symbols.push_back(s);
            }

            // write data to file
            float ts_now = (float)getTimeNow();
            Filedata fd;
            fd.time_recieved = ts_now;
            fd.time_sent = timestamp;
            fd.price = price;
            fd.volume = vol;

            
        }
    } else {
        char output[200];
        ArduinoJson::serializeJson(doc, output, 200);
        printf("No Trade: %s\n", output);
    }
    
}

static int ws_service_callback(
    struct lws *wsi,
    enum lws_callback_reasons reason, void *user,
    void *in, size_t len)
{
    switch (reason) {

        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            printf(KYEL "[Main Service] Connect with server success.\n" RESET);
            connection_flag = 1;
	        lws_callback_on_writable(wsi);
            break;

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            printf(KRED "[Main Service] Connect with server error: %s.\n" RESET, (const char*)in);
            destroy_flag = 1;
            connection_flag = 0;
            break;

        case LWS_CALLBACK_CLOSED:
            printf(KYEL "[Main Service] LWS_CALLBACK_CLOSED\n" RESET);
            destroy_flag = 1;
            connection_flag = 0;
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
            // printf(KCYN_L "[Main Service] Client recvived:" RESET);
            clientRecievedData((char *)in);
            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE:
            printf(KYEL "[Main Service] On writeable is called.\n" RESET);
            char* out = NULL;
            std::string symb_arr[9] = {"APPL\0", "AMZN\0", "MSFT\0", "KO\0", "IC MARKETS:1\0", "EURUSD\0", "BINANCE:BTCUSDT\0", "BINANCE:ETHUSDT\0", "BINANCE:XMRUSDT\0"};
            std::string str;
            
            for(int i = 0; i < 9; i++){
                str = "{\"type\":\"subscribe\",\"symbol\":\"" + symb_arr[i] + "\"}";
                const char *cstr = str.c_str();
                int len = str.length();
                std::cout << "str length: " << len << ", " << str << std::endl;
                
                out = (char *)malloc(sizeof(char)*(LWS_SEND_BUFFER_PRE_PADDING + len + LWS_SEND_BUFFER_POST_PADDING));
                memcpy(out + LWS_SEND_BUFFER_PRE_PADDING, cstr, len);
                lws_write(wsi, (unsigned char *)(out + LWS_SEND_BUFFER_PRE_PADDING), len, LWS_WRITE_TEXT);
            }

            free(out);
            writeable_flag = 1;
            break;
    }

    return 0;
}

static struct lws_protocols protocols[] =
{
	{
		"example-protocol",
		ws_service_callback,
		0,
		EXAMPLE_RX_BUFFER_BYTES,
	},
	{ NULL, NULL, 0, 0 } /* terminator */
};

void removeOldFiles() {
    fs::path p("./");
    if(fs::exists(p) && fs::is_directory(p))
    {
        fs::directory_iterator end;
        for(fs::directory_iterator it(p); it != end; ++it)
        {
            try
            {
                if(fs::is_regular_file(it->status()) && (it->path().extension().compare(".bin") == 0))
                {
                    fs::remove(it->path());
                }
            }
            catch(const std::exception &ex)
            {
                ex;
            }
        }
    }
}

int main(void)
{   
    removeOldFiles();
    struct lws_context *context = NULL;
    struct lws_context_creation_info info;
    struct lws *wsi = NULL;
    struct lws_protocols protocol;

    memset(&info, 0, sizeof(info));

    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    
    char inputURL[300] = "ws.finnhub.io/?token=cco3egaad3i3sjb7ns0gcco3egaad3i3sjb7ns10";
    const char *urlProtocol, *urlTempPath;
	char urlPath[300];
    context = lws_create_context(&info);
    printf(KRED "[Main] context created.\n" RESET);

    if (context == NULL) {
        printf(KRED "[Main] context is NULL.\n" RESET);
        return -1;
    }
    struct lws_client_connect_info clientConnectionInfo;
    memset(&clientConnectionInfo, 0, sizeof(clientConnectionInfo));
    clientConnectionInfo.context = context;
    if (lws_parse_uri(inputURL, &urlProtocol, &clientConnectionInfo.address,
	    &clientConnectionInfo.port, &urlTempPath))
    {
	    printf("Couldn't parse URL\n");
    }

    urlPath[0] = '/';
    strncpy(urlPath + 1, urlTempPath, sizeof(urlPath) - 2);
    urlPath[sizeof(urlPath)-1] = '\0';
    clientConnectionInfo.port = 443;
    clientConnectionInfo.path = urlPath;
    clientConnectionInfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
    
    clientConnectionInfo.host = clientConnectionInfo.address;
    clientConnectionInfo.origin = clientConnectionInfo.address;
    clientConnectionInfo.ietf_version_or_minus_one = -1;
    clientConnectionInfo.protocol = protocols[0].name;
    printf("Testing %s\n\n", clientConnectionInfo.address);
    printf("Connecticting to %s://%s:%d%s \n\n", urlProtocol, 
    clientConnectionInfo.address, clientConnectionInfo.port, urlPath);

    wsi = lws_client_connect_via_info(&clientConnectionInfo);
    if (wsi == NULL) {
        printf(KRED "[Main] wsi create error.\n" RESET);
        return -1;
    }

    printf(KGRN "[Main] wsi create success.\n" RESET);

    // start the every min thread
    pthread_t thread_work_every_min;
    pthread_create(&thread_work_every_min, NULL, do_every_min, NULL);

    // start the every 15 min thread
    pthread_t thread_work_every_15_min;
    pthread_create(&thread_work_every_15_min, NULL, do_every_15_min, NULL);

    while(!destroy_flag)
    {
        lws_service(context, 50);
    }

    lws_context_destroy(context);
    pthread_join(thread_work_every_min, NULL);

    return 0;
}
