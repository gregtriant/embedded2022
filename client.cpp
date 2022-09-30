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
#include <limits>
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
#define MINS 3

static int destroy_flag = 0;
static int connection_flag = 0;
static int writeable_flag = 0;
static int DO_WORK = 1;


typedef struct symb {
    std::string symbol;
    unsigned long total_times;
    unsigned long prev_total_1_min;
    unsigned long current_1_min;
    unsigned long last_15_mins[MINS];
} Symbol;

typedef struct fd {
    unsigned long long time_recieved;
    unsigned long long time_sent;
    float price;
    float volume;
} Filedata;

std::vector<Symbol> all_symbols;
int mins_passed = 0;

unsigned int msleep(unsigned int tms) {
  return usleep(tms * 1000);
}

void print_symbol(Symbol s) {
    int total_last_15_mins = 0;
    for (int i=0; i<MINS; i++) {
        if(s.last_15_mins[i] == std::numeric_limits<unsigned long>::max()) {
            total_last_15_mins = 0;
            break;
        }
        total_last_15_mins += s.last_15_mins[i];
    }
    std::string str = s.symbol + "(" + std::to_string(s.total_times)  + ", " + std::to_string(s.current_1_min) + ", " + std::to_string(total_last_15_mins) + ")\n";
    std::cout << str;
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

std::string getDateTimeNow()
{
  std::time_t now= std::time(0);
  std::tm* now_tm= std::gmtime(&now);
  char buf[42];
  std::strftime(buf, 42, "%Y-%m-%d %X", now_tm);
  return buf;
}

void candleWork(Symbol s) {
    // for last 1 min
    float initial_price = 0;
    float final_price = 0;
    float max_price = std::numeric_limits<float>::min();
    float min_price = std::numeric_limits<float>::max();
    float total_volume_last_min = 0;

    int lines_to_read = s.current_1_min;
    int lines_to_skip = s.total_times - s.current_1_min;
    int bytes_per_line = 2*sizeof(unsigned long long) + 2*sizeof(float); // 2 unsinged long longs and 2 floats per line
    FILE *file;
    std::string filename = "values_" + s.symbol + ".bin";
    file = fopen(filename.c_str(), "rb");
    if (!file)
    {
        fprintf(stderr, "Unable to open file %s", filename.c_str());
        return;
    }

    fseek(file, /* from the start */ lines_to_skip * bytes_per_line, SEEK_SET);
    unsigned char buffer[bytes_per_line];
    for (int i=0; i < lines_to_read; i++) {
        size_t result = fread(buffer, bytes_per_line, 1, file);
        unsigned long long t1;
        memcpy(&t1, &buffer, sizeof(unsigned long long));
        unsigned long long t2;
        memcpy(&t2, &buffer[0 + sizeof(unsigned long long)], sizeof(unsigned long long));
        float price;
        memcpy(&price, &buffer[0 + 2*sizeof(unsigned long long)], sizeof(float));
        float volume;
        memcpy(&volume, &buffer[0 + 2*sizeof(unsigned long long) + sizeof(float)], sizeof(float));

        // printf("%lld %lld %f %f\n", t1,t2,price,vol);

        total_volume_last_min += volume;
        if (i == 0)
            initial_price = price;
        
        if (i == lines_to_read - 1)
            final_price = price;

        if (price > max_price)
            max_price = price;
        
        if (price < min_price)
            min_price = price;
    }
    fclose(file);
    
    // save values to a file
    std::string candle_filename = "candle_" + s.symbol + ".txt";
    std::fstream file_out;

    file_out.open(candle_filename, std::ios_base::app);
    if (!file_out.is_open()) {
        std::cout << "failed to open " << candle_filename << '\n';
    } else {
        std::string data = std::to_string(getTimeNow()) + " " + std::to_string(initial_price) + " " + 
                            std::to_string(final_price) + " " + std::to_string(max_price) + " " + 
                            std::to_string(min_price) + " " + std::to_string(total_volume_last_min);
        file_out << data << std::endl;
    }
}

void meanWork(Symbol s) {
    int total_last_15_mins = 0;
    bool found_a_null = false;
    for (int i=0; i<MINS; i++) {
        if(s.last_15_mins[i] == std::numeric_limits<unsigned long>::max()) {
            total_last_15_mins = 0;
            found_a_null = true;
            break;
        }
        total_last_15_mins += s.last_15_mins[i];
    }
    if (found_a_null) { // if we found a null that means that the time needed has not passed yet
        std::string str = s.symbol + " <-- found an unset value in the last " + std::to_string(MINS) + " minutes\n";
        std::cout << str;
    } else {
        // do the work
        float mean_price = 0;
        float total_volume_last_15_min = 0;

        int lines_to_read = total_last_15_mins;
        int lines_to_skip = s.total_times - lines_to_read;
        int bytes_per_line = 2*sizeof(unsigned long long) + 2*sizeof(float); // 2 unsinged long longs and 2 floats per line
        FILE *file;
        std::string filename = "values_" + s.symbol + ".bin";
        file = fopen(filename.c_str(), "rb");
        if (!file)
        {
            fprintf(stderr, "Unable to open file %s", filename.c_str());
            return;
        }

        fseek(file, /* from the start */ lines_to_skip * bytes_per_line, SEEK_SET);
        unsigned char buffer[bytes_per_line];
        for (int i=0; i < lines_to_read; i++) {
            size_t result = fread(buffer, bytes_per_line, 1, file);
            unsigned long long t1;
            memcpy(&t1, &buffer, sizeof(unsigned long long));
            unsigned long long t2;
            memcpy(&t2, &buffer[0 + sizeof(unsigned long long)], sizeof(unsigned long long));
            float price;
            memcpy(&price, &buffer[0 + 2*sizeof(unsigned long long)], sizeof(float));
            float volume;
            memcpy(&volume, &buffer[0 + 2*sizeof(unsigned long long) + sizeof(float)], sizeof(float));

            // printf("%lld %lld %f %f\n", t1,t2,price,vol);

            mean_price += price;
            total_volume_last_15_min += volume;
        }
        if (lines_to_read != 0) {
            mean_price = mean_price/lines_to_read;
        } else {
            mean_price = 0;
        }
        fclose(file);
        
        // save values to a file
        std::string candle_filename = "mean_" + s.symbol + ".txt";
        std::fstream file_out;

        file_out.open(candle_filename, std::ios_base::app);
        if (!file_out.is_open()) {
            std::cout << "failed to open " << candle_filename << '\n';
        } else {
            std::string data = std::to_string(getTimeNow()) + " " + std::to_string(mean_price) + " " + std::to_string(total_last_15_mins);
            file_out << data << std::endl;
        }
    }
}

void *workToDo(void *symbol) {
    Symbol s = *(Symbol *) (symbol);
    print_symbol(s);
    
    candleWork(s);
    meanWork(s);
    return NULL;
}

// for threads
void *do_every_min(void *vargp) {
    int long_work = 3; // 3 times longer than short work
    while(DO_WORK) {
        msleep(4* 1000);
        mins_passed++;
        std::cout << KGRN << "\n--- " << getDateTimeNow() << " Short time passed "<< mins_passed <<" , working... --- " << RESET << std::endl;
        // if (mins_passed % long_work == 0) {
        //     std::cout << KCYN << "------- " << getDateTimeNow() << " LONG time passed -------" << RESET << std::endl;
        // }

        std::vector<pthread_t> my_threads;

        for (int i=0; i<all_symbols.size(); i++) {
            all_symbols[i].current_1_min = all_symbols[i].total_times - all_symbols[i].prev_total_1_min;
            all_symbols[i].prev_total_1_min = all_symbols[i].total_times;
            for (int j=1; j<MINS; j++) {
                all_symbols[i].last_15_mins[j-1] = all_symbols[i].last_15_mins[j]; // shift everything one place left.
            }
            all_symbols[i].last_15_mins[MINS-1] = all_symbols[i].current_1_min;

            pthread_t new_thread;
            pthread_create(&new_thread, NULL, workToDo, &all_symbols[i]);
            my_threads.push_back(new_thread);
        }

        for (int i=0; i<my_threads.size(); i++) {
            pthread_join(my_threads[i], NULL);
            // std::cout << "Thread: " << i << " Done!" << std::endl;
        }
        
    }
    return NULL;
}

// ----------------------------

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
            unsigned long long timestamp = nested["t"];
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
                s.current_1_min = 0;
                for(int i=0; i<MINS; i++) {
                    s.last_15_mins[i] = std::numeric_limits<unsigned long>::max();
                }
                all_symbols.push_back(s);
            }

            // write data to file
            unsigned long long ts_now = getTimeNow();
            Filedata fd;
            fd.time_recieved = ts_now;
            fd.time_sent = timestamp;
            fd.price = price;
            fd.volume = vol;
            // printf("Writing to file: %f, %f\n", fd.time_recieved, fd.time_sent);
            symbolString = "values_" + symbolString + ".bin";
            writeToFile(symbolString, fd);
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
            std::string symb_arr[11] = {"APPL\0", "AMZN\0", "MSFT\0", "KO\0", "TSLA\0", "NVDA\0", "IC MARKETS:1\0", "EURUSD\0", "BINANCE:BTCUSDT\0", "BINANCE:ETHUSDT\0", "BINANCE:XMRUSDT\0"};
            std::string str;
            
            for(int i = 0; i < 9; i++){
                str = "{\"type\":\"subscribe\",\"symbol\":\"" + symb_arr[i] + "\"}";
                const char *cstr = str.c_str();
                int len = str.length();
                std::cout << "Sending: " << len << ", " << str << std::endl;
                
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

// removing old .txt and .bin files
void removeOldFiles() {
    fs::path p("./");
    if(fs::exists(p) && fs::is_directory(p))
    {
        fs::directory_iterator end;
        for(fs::directory_iterator it(p); it != end; ++it)
        {
            try
            {
                if(fs::is_regular_file(it->status()) && (it->path().extension().compare(".bin") == 0) || (it->path().extension().compare(".txt") == 0))
                {   
                    std::cout << "Removing: " << it->path() << std::endl;
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

    removeOldFiles(); // .bin

    // start the every min thread
    pthread_t thread_work_every_min;
    pthread_create(&thread_work_every_min, NULL, do_every_min, NULL);

    while(!destroy_flag)
    {
        lws_service(context, 50);
    }

    lws_context_destroy(context);
    pthread_join(thread_work_every_min, NULL);

    return 0;
}
