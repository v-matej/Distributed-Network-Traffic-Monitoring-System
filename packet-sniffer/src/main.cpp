#include <pcap.h>
#include <iostream>

int main() {
    char errbuf[PCAP_ERRBUF_SIZE];

    pcap_if_t *alldevs;
    if (pcap_findalldevs(&alldevs, errbuf) == -1) {
        std::cerr << "Error finding devices: " << errbuf << std::endl;
        return 1;
    }

    std::cout << "Available network interfaces:\n";

    for (pcap_if_t *d = alldevs; d != nullptr; d = d->next) {
        std::cout << " - " << d->name << std::endl;
    }

    pcap_freealldevs(alldevs);

    return 0;
}