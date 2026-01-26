#pragma once

#include "Meo3_Type.h"
#include <string>

class MeoRegistrationClient {
public:
    MeoRegistrationClient();

    // For this model, gateway host/port are only needed for broadcast if you want unicast
    void setGateway(const char* host, uint16_t port);
    void setLogger(MeoLogFunction logger);

    // Perform registration if no credentials exist.
    // 1) broadcast IP/MAC/features
    // 2) listen on TCP 8091 for gateway response
    bool registerIfNeeded(const MeoDeviceInfo& devInfo,
                          const MeoFeatureRegistry& features,
                          std::string& deviceIdOut,
                          std::string& transmitKeyOut);

private:
    std::string    _gatewayHost;
    uint16_t       _port;
    MeoLogFunction _logger;

    bool _sendBroadcast(const MeoDeviceInfo& devInfo,
                        const MeoFeatureRegistry& features);
    bool _waitForRegistrationResponse(std::string& responseJson);
    bool _parseRegistrationResponse(const std::string& json,
                                    std::string& deviceIdOut,
                                    std::string& transmitKeyOut);
};