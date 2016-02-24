#ifndef ISPN_HOTROD_PROTOCOL_HEADERPARAMS_H
#define ISPN_HOTROD_PROTOCOL_HEADERPARAMS_H

#include "infinispan/hotrod/Flag.h"
#include "hotrod/impl/IntWrapper.h"

#include <set>
#include <vector>
namespace infinispan {
namespace hotrod {
namespace protocol {

class HeaderParams
{
  public:
	HeaderParams(IntWrapper& topId);
    HeaderParams& setOpCode(uint8_t opCode);
    HeaderParams& setCacheName(const std::vector<char>& cacheName);
    HeaderParams& setFlags(uint32_t flags);
    HeaderParams& setClientIntel(uint8_t clientIntel);
    HeaderParams& setTxMarker(uint8_t txMarker);
    HeaderParams& setMessageId(uint64_t messageId);

  private:
    uint8_t toOpRespCode(uint8_t opCode);

    uint8_t opCode;
    uint8_t opRespCode;
    std::vector<char> cacheName;
    uint32_t flags;
    uint8_t clientIntel;
    uint8_t txMarker;
    IntWrapper& topologyId;
    uint64_t messageId;

    friend class Codec10;
    friend class Codec11;
    friend class Codec12;
    friend class Codec13;
    friend class Codec20;
    friend class Codec21;
    friend class Codec22;
    friend class Codec23;
    friend class Codec24;
};

}}} // namespace infinispan::hotrod::protocol

#endif // ISPN_HOTROD_PROTOCOL_HEADERPARAMS_H
