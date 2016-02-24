#include "infinispan/hotrod/exceptions.h"
#include <hotrod/impl/transport/tcp/InetSocketAddress.h>
#include "hotrod/impl/protocol/Codec20.h"
#include "hotrod/impl/protocol/HotRodConstants.h"
#include "hotrod/impl/protocol/HeaderParams.h"
#include "hotrod/impl/transport/Transport.h"
#include "hotrod/impl/transport/TransportFactory.h"
#include "hotrod/sys/Log.h"
#include "hotrod/sys/Mutex.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <set>
#include <map>
#include <vector>

namespace infinispan {
namespace hotrod {

using transport::Transport;
using transport::InetSocketAddress;
using transport::TransportFactory;
using infinispan::hotrod::sys::Mutex;
using infinispan::hotrod::sys::ScopedLock;
using infinispan::hotrod::sys::ScopedUnlock;

namespace protocol {

extern long msgId;
extern Mutex lock;

long Codec20::getMessageId() {
    ScopedLock<Mutex> l(lock);
    long msgIdToBeReturned = msgId;
    ScopedUnlock<Mutex> ul(lock);
    return msgIdToBeReturned;
}

HeaderParams& Codec20::writeHeader(
    Transport& transport, HeaderParams& params) const
{
    return writeHeader(transport, params, HotRodConstants::VERSION_20);
}

HeaderParams& Codec20::writeHeader(
    Transport& transport, HeaderParams& params, uint8_t version) const
{
    transport.writeByte(HotRodConstants::REQUEST_MAGIC);
    ScopedLock<Mutex> l(lock);
    transport.writeVLong(params.setMessageId(++msgId).messageId);
    ScopedUnlock<Mutex> ul(lock);
    transport.writeByte(version);
    transport.writeByte(params.opCode);
    transport.writeArray(params.cacheName);
    transport.writeVInt(params.flags);
    transport.writeByte(params.clientIntel);
    transport.writeVInt(params.topologyId.get());
    return params;
}

uint8_t Codec20::readHeader(
    Transport& transport, HeaderParams& params) const
{
    uint8_t magic = transport.readByte();
    if (magic != HotRodConstants::RESPONSE_MAGIC) {
        std::ostringstream message;
        message << std::hex << std::setfill('0');
        message << "Invalid magic number. Expected 0x" << std::setw(2) << static_cast<unsigned>(HotRodConstants::RESPONSE_MAGIC) << " and received 0x" << std::setw(2) << static_cast<unsigned>(magic);
        throw InvalidResponseException(message.str());
    }

    uint64_t receivedMessageId = transport.readVLong();
    // TODO: java comment, to be checked
    // If received id is 0, it could be that a failure was noted before the
    // message id was detected, so don't consider it to a message id error
    if (receivedMessageId != params.messageId && receivedMessageId != 0) {
        std::ostringstream message;
        message << "Invalid message id. Expected " <<
            params.messageId << " and received " << receivedMessageId;
        throw InvalidResponseException(message.str());
    }

    uint8_t receivedOpCode = transport.readByte();

    // Read both the status and new topology (if present),
    // before deciding how to react to error situations.
    uint8_t status = transport.readByte();
    readNewTopologyIfPresent(transport, params);

    // Now that all headers values have been read, check the error responses.
    // This avoids situations where an exceptional return ends up with
    // the socket containing data from previous request responses.
    if (receivedOpCode != params.opRespCode) {
      if (receivedOpCode == HotRodConstants::ERROR_RESPONSE) {
        checkForErrorsInResponseStatus(transport, params, status);
        }
        std::ostringstream message;
        message << "Invalid response operation. Expected " << std::hex <<
            (int) params.opRespCode << " and received " << std::hex << (int) receivedOpCode;
        throw InvalidResponseException(message.str());
    }

    return status;
}

void Codec20::readNewTopologyIfPresent(
    Transport& transport, HeaderParams& params) const
{
    uint8_t topologyChangeByte = transport.readByte();
    if (topologyChangeByte == 1)
        readNewTopologyAndHash(transport, params.topologyId, params.cacheName);
}

void Codec20::readNewTopologyAndHash(Transport& transport, IntWrapper& topologyId, const std::vector<char>& /*cacheName*/) const{
    // Just consume the header's byte
	// Do not evaluate new topology right now
	uint32_t newTopologyId = transport.readVInt();
    topologyId.set(newTopologyId); //update topologyId reference
//    uint32_t numKeyOwners = transport.readVInt();

//    uint8_t hashFunctionVersion = transport.readByte();
//    uint32_t hashSpace = transport.readVInt();
    uint32_t clusterSize = transport.readVInt();

//    std::vector<InetSocketAddress> addresses(clusterSize);
    for (uint32_t i = 0; i < clusterSize; i++) {
       /*std::string host*/transport.readString();
       /*int16_t port = */transport.readUnsignedShort();
//       addresses[i] = InetSocketAddress(host, port);
    }

    uint8_t hashFunctionVersion = transport.readByte();
    uint32_t numSegments = transport.readVInt();

//    std::vector<std::vector<InetSocketAddress>> segmentOwners(numSegments);

    if (hashFunctionVersion > 0) {
       for (uint32_t i = 0; i < numSegments; i++) {
          uint8_t numOwners = transport.readByte();
          for (uint8_t j = 0; j < numOwners; j++) {
             /*uint32_t memberIndex = */transport.readVInt();
//             segmentOwners[i][j] = addresses[memberIndex];
          }
       }
    }
    // TODO: topology evaluation

//    TransportFactory tf = transport.getTransportFactory();
//    int currentTopology = tf.getTopologyId(cacheName);
//    std::map<InetSocketAddress, std::set<int32_t> > m = computeNewHashes(
//            transport, newTopologyId, numKeyOwners, hashFunctionVersion, hashSpace,
//            clusterSize);
//
//    std::vector<InetSocketAddress> socketAddresses;
//    for (std::map<InetSocketAddress, std::set<int32_t> >::iterator it = m.begin(); it != m.end(); ++it) {
//        socketAddresses.push_back(it->first);
//    }
//    transport.getTransportFactory().updateServers(socketAddresses);
//
//    if (hashFunctionVersion == 0) {
//        TRACE("No hash function present.");
//        transport.getTransportFactory().clearHashFunction(cacheName);
//    } else {
//        TRACE("Updating Hash version: %u owners: %d hash_space: %u cluster_size: %u",
//                hashFunctionVersion,
//                numKeyOwners,
//                hashSpace,
//                clusterSize);
//        transport.getTransportFactory().updateHashFunction(m, numKeyOwners,
//                hashFunctionVersion, hashSpace, cacheName);
//    }
}

std::map<InetSocketAddress, std::set<int32_t> > Codec20::computeNewHashes(
        Transport& transport, uint32_t /*newTopologyId*/, int16_t /*numKeyOwners*/,
        uint8_t /*hashFunctionVersion*/, uint32_t /*hashSpace*/, uint32_t clusterSize) const {

    std::map<InetSocketAddress, std::set<int32_t> > map;
    for (uint32_t i = 0; i < clusterSize; i++) {
        std::string host = transport.readString();
        int16_t port = transport.readUnsignedShort();
        int32_t hashCode = transport.read4ByteInt();
        InetSocketAddress address(host, port);

        std::map<InetSocketAddress, std::set<int32_t> >::iterator it =
                map.find(address);
        if (it == map.end()) {
            std::set<int32_t> hashes;
            hashes.insert(hashCode);
            map.insert(
                    std::pair<InetSocketAddress, std::set<int32_t> >(address,
                            hashes));
        } else {
            it->second.insert(hashCode);
        }
    }
    return map;
}

void Codec20::checkForErrorsInResponseStatus(Transport& transport, HeaderParams& params, uint8_t status) const {
    try {
        switch (status) {
        case HotRodConstants::INVALID_MAGIC_OR_MESSAGE_ID_STATUS:
        case HotRodConstants::REQUEST_PARSING_ERROR_STATUS:
        case HotRodConstants::UNKNOWN_COMMAND_STATUS:
        case HotRodConstants::SERVER_ERROR_STATUS:
        case HotRodConstants::COMMAND_TIMEOUT_STATUS:
        case HotRodConstants::UNKNOWN_VERSION_STATUS: {
            // If error, the body of the message just contains a message
            std::string msgFromServer = transport.readString();
            if (msgFromServer.find("SuspectException") != std::string::npos || msgFromServer.find("SuspectedException") != std::string::npos) {
                // Handle both Infinispan's and JGroups' suspicions
                // TODO: This will be better handled with its own status id in version 2 of protocol
                throw RemoteNodeSuspectException(msgFromServer, params.messageId, status);
            } else {
                throw HotRodClientException(msgFromServer); //, params.messageId, status);
            }
        }
        default: {
            throw InternalException("Unknown status: " + status);
        }
        }
    } catch (const Exception &) {
        // Some operations require invalidating the transport
        switch (status) {
        case HotRodConstants::INVALID_MAGIC_OR_MESSAGE_ID_STATUS:
        case HotRodConstants::REQUEST_PARSING_ERROR_STATUS:
        case HotRodConstants::UNKNOWN_COMMAND_STATUS:
        case HotRodConstants::UNKNOWN_VERSION_STATUS: {
            transport.invalidate();
        }
        }
        throw;
    }
}
std::vector<char> Codec20::returnPossiblePrevValue(transport::Transport& t, uint8_t status, uint32_t /*flags*/) const{
	std::vector<char> result;
	if (HotRodConstants::hasPrevious(status)) {
		TRACEBYTES("return value = ", result);
		result = t.readArray();
	} else {
		TRACE("No return value");
	}
return result;
}

void Codec20::writeExpirationParams(transport::Transport& t,uint64_t lifespan, uint64_t maxIdle) const {
	uint32_t lInt= (uint32_t) lifespan;
	uint32_t mInt= (uint32_t) maxIdle;
	if ( lInt != lifespan)
		throw HotRodClientException("lifespan exceed 32bit integer");
	if ( mInt != maxIdle)
		throw HotRodClientException("maxIdle exceed 32bit integer");
	t.writeVInt(lInt);
    t.writeVInt(mInt);
}

}}} // namespace infinispan::hotrod::protocol
