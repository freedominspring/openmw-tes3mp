#ifndef OPENMW_PACKETOBJECTANIMPLAY_HPP
#define OPENMW_PACKETOBJECTANIMPLAY_HPP

#include <components/openmw-mp/Packets/World/WorldPacket.hpp>

namespace mwmp
{
    class PacketObjectAnimPlay : public WorldPacket
    {
    public:
        PacketObjectAnimPlay(RakNet::RakPeerInterface *peer);

        virtual void Object(WorldObject &worldObject, bool send);
    };
}

#endif //OPENMW_PACKETOBJECTANIMPLAY_HPP
