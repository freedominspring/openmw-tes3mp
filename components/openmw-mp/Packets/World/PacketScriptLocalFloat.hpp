#ifndef OPENMW_PACKETSCRIPTLOCALFLOAT_HPP
#define OPENMW_PACKETSCRIPTLOCALFLOAT_HPP

#include <components/openmw-mp/Packets/World/WorldPacket.hpp>

namespace mwmp
{
    class PacketScriptLocalFloat : public WorldPacket
    {
    public:
        PacketScriptLocalFloat(RakNet::RakPeerInterface *peer);

        virtual void Object(WorldObject &worldObject, bool send);
    };
}

#endif //OPENMW_PACKETSCRIPTLOCALFLOAT_HPP
