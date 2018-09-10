#include <CommlayerAPI.h>
#include <SafeQueue.h>
#include <ServiceMessage.h>

#include <unistd.h>  //usleep
#include <iostream>
#include <sstream>  // std::stringstream
//#include "Logger.h"

static char red[] = "\033[1;31m";
static char green[] = "\033[1;32m";
static char magenta[] = "\033[1;35m";
static char nocolor[] = "\033[0m";

static std::ostream& operator<<(std::ostream& os, const CLRequestToken& request)
{
    os << "REQ[" << request.uuid().urn() << " " << request.servicePath() << " " << request.expirationDate() << "]";
    return os;
}

static std::ostream& operator<<(std::ostream& os, const ByteArray& bin)
{
    char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < bin.size(); i++)
        os << hex[bin[i] / 16] << hex[bin[i] % 16] << ':';
    return os;
}

static std::ostream& operator<<(std::ostream& os, const ResponseStatusCode s)
{
    switch (s.value())
    {
        case ResponseStatusCode::OK:
            os << "OK";
            break;
        case ResponseStatusCode::BAD_REQUEST:
            os << "BAD_REQUEST";
            break;
        case ResponseStatusCode::BUSY:
            os << "BUSY";
            break;
        case ResponseStatusCode::NOT_FOUND:
            os << "NOT_FOUND";
            break;
        case ResponseStatusCode::METHOD_NOT_ALLOWED:
            os << "METHOD_NOT_ALLOWED";
            break;
        case ResponseStatusCode::INTERNAL_ERROR:
            os << "INTERNAL_ERROR";
            break;
        default:
            os << "no StatusCode";
    };
    return os;
}

static std::ostream& operator<<(std::ostream& os, const GatewayMessage& message)
{
    switch (message.type().value())
    {
        case GatewayMessageType::EVENT:
        {
            const GatewayMessage& e = message;
            os << "Event\n["
               << "\n  id: " << e.id().urn() << "\n  payload: " << e.payload() << "\n  servicePath: " << e.servicePath() << "\n]\n";
        }
        break;
        case GatewayMessageType::REQUEST:
        {
            const GatewayMessage& r = message;
            os << "Request["
               << "\n  id: " << r.id().urn() << "\n  payload: " << r.payload() << "\n  servicePath: " << r.servicePath()
               << "\n  requestToken:" << r.requestToken() << "\n]\n";
        }
        break;
        case GatewayMessageType::RESPONSE:
        {
            const GatewayMessage& r = message;
            os << "Response["
               << "\n  id: " << r.id().urn() << "\n  payload: " << r.payload() << "\n  servicePath: " << r.servicePath()
               << "\n  statusCode: " << r.statusCode() << "\n]\n";
        }
        break;
        default:
            os << "Empty[GatewayMessage]";
    };
    return os;
}

class Tupel
{
  public:
    CommlayerAPI& bridge;
    bool& end;
    inline Tupel(CommlayerAPI& bridge, bool& end) : bridge(bridge), end(end), _mutex(new pthread_mutex_t) { pthread_mutex_init(_mutex, 0); }
    inline void lock() { pthread_mutex_lock(_mutex); }
    inline void unlock() { pthread_mutex_unlock(_mutex); }
    ~Tupel() { pthread_mutex_destroy(_mutex); }
    inline SafeQueue<CLRequestToken>& request() { return _request; }
  private:
    pthread_mutex_t* _mutex;
    SafeQueue<CLRequestToken> _request;
};

static void* func(void* data)
{
    Tupel* tupel = static_cast<Tupel*>(data);
    tupel->lock();
    std::cout << red << "start second thread\n" << magenta << std::flush;
    tupel->unlock();
    bool& end = tupel->end;
    while (!end)
    {
        if (tupel->bridge.receiveMessageSize())
        {
            tupel->lock();
            const GatewayMessage gatewayMessage = tupel->bridge.dequeueReceiveMessage();
            std::cout << red << "reveived Message: " << gatewayMessage << std::endl << magenta << std::flush;

            if (gatewayMessage.type().isARequest())
                tupel->request().push_front(gatewayMessage.requestToken());
            tupel->unlock();
        }
        tupel->unlock();
        usleep(50 * 1000);
    }
    tupel->lock();
    std::cout << red << "end second thread\n" << magenta << std::flush;
    tupel->unlock();
    pthread_exit(NULL);
}

static void readBase(ByteArray& payload, std::string& servicePath, uint32_t& ttl, int64_t& createdAt, MessageCompression& compression, bool& persist)
{
    std::cout << " payload: ";
    std::string sPayload;
    std::cin >> sPayload;
    if (sPayload.compare("_") == 0)
        sPayload = "";
    payload = sPayload;
    std::cout << " servicePath: ";
    std::cin >> servicePath;
    if (servicePath.compare("_") == 0)
        servicePath = "";
    std::cout << "  ttl: ";
    std::cin >> ttl;
    std::cout << "  createdAt(number or 'now'): ";
    {
        std::string tmp;
        std::cin >> tmp;
        if (tmp.compare("now") == 0)
            createdAt = time(0) * 1000;
        else
        {
            std::stringstream ss;
            ss << tmp;
            ss >> createdAt;
        }
    }
    uint32_t choice;
    std::cout << "  compression(0-2)[auto, compress, not]: ";
    std::cin >> choice;
    switch (choice)
    {
        case 1:
            compression = MessageCompression::willCompress();
            break;
        case 2:
            compression = MessageCompression::willNotCompress();
            break;
        default:
            compression = MessageCompression::willAutoCompress();
    };
    std::cout << "  persist(0,1): ";
    int p;
    std::cin >> p;
    persist = (p != 0);
}

static bool findFlag(int argc, char* argv[], const std::string flag)
{
    for (int i = 1; i < argc; i++)
        if (flag.compare(argv[i]) == 0)
            return true;
    return false;
}

static bool findFlagWithOption(int argc, char* argv[], const std::string flag, std::string& option)
{
    for (int i = 1; i < argc; i++)
        if (flag.compare(argv[i]) == 0)
        {
            if (i < argc - 1)
                option = argv[i + 1];
            return true;
        }
    return false;
}

int main(int argc, char* argv[])
{
    const bool systemBus = findFlag(argc, argv, "-system");
    std::string name = "tst";
    findFlagWithOption(argc, argv, "-name", name);
    std::cout << green << "Service name: \"" << name << "\"\n";
    if (systemBus)
        std::cout << green << "Use System Bus.\n";
    else
        std::cout << green << "Use Sesson Bus. To use System Bus use -system flag.";

    //Logger::enableStdoutLogging();
    //Logger::setLogLevel(LoggerLevel::DEBUG);

    bool end = false;
    std::cout << green << std::flush;

    std::vector<std::string> paths;
    paths.push_back("set");
    paths.push_back("get");
    paths.push_back("post");
    ServiceSpecificationValues values(name, "testService", "first and last", paths, "plaintext", "This is only a test service. It has no purpose.");
    CommlayerAPI bridge(values);
    Tupel tupel(bridge, end);
    pthread_t thread;
    const int rc = pthread_create(&thread, NULL, func, &tupel);
    (void)rc;
    std::cout << green << "begin CommlayerAPITest\n" << nocolor;
    std::string input;
    while (input.compare("end") != 0)
    {
        tupel.lock();
        std::cout << magenta << "write event, request, response or end\n";
        tupel.unlock();
        std::cin >> input;
        if (input.compare("event") == 0)
        {
            ByteArray payload;
            std::string servicePath;
            uint32_t ttl;
            int64_t createdAt;
            MessageCompression compression = MessageCompression::notCompress();
            bool persist;
            readBase(payload, servicePath, ttl, createdAt, compression, persist);
            bridge.sendEvent(payload, servicePath, ttl, compression, persist, nullptr);
        }
        else if (input.compare("request") == 0)
        {
            ByteArray payload;
            std::string servicePath;
            uint32_t ttl;
            int64_t createdAt;
            MessageCompression compression = MessageCompression::notCompress();
            bool persist;
            readBase(payload, servicePath, ttl, createdAt, compression, persist);
            bridge.sendRequest(payload, servicePath, ttl, compression, persist, nullptr);
        }
        else if (input.compare("response") == 0)
        {
            std::string servicePath;
            ByteArray payload;
            uint32_t ttl;
            int64_t createdAt;
            MessageCompression compression = MessageCompression::notCompress();
            bool persist;
            readBase(payload, servicePath, ttl, createdAt, compression, persist);
            uint8_t choice;
            std::cout << "  statusCode(0-5): ";
            std::cin >> choice;
            choice = static_cast<uint8_t>(choice - '0');
            if (choice > 5)
                choice = 0;
            ResponseStatusCode statusCode = ResponseStatusCode::createByValue(choice);
            if (tupel.request().size() == 0)
                std::cout << "No cl request token saved. Dont send the response.\n";
            else
            {
                CLRequestToken requestToken = tupel.request().pop_back();
                std::cout << "Take request Token: " << requestToken << "\n";
                bridge.sendResponse(requestToken, payload, servicePath, ttl, compression, persist, statusCode, nullptr);
            }
        }
    }
    end = true;
    pthread_join(thread, 0);
    std::cout << green << "end CommlayerAPITest\n" << nocolor;

    return 0;
}