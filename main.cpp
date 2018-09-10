#include <CommlayerAPI.h>

int main(int argc, char* argv[])
{
    std::vector<std::string> paths;
    ServiceSpecificationValues values("name", "testService", "first and last", paths, "plaintext", "This is only a test service. It has no purpose.");
    CommlayerAPI bridge(values);
    return 0;
}