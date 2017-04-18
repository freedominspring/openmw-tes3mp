//
// Created by koncord on 18.04.17.
//

#ifndef OPENMW_PROCESSORACTORTEST_HPP
#define OPENMW_PROCESSORACTORTEST_HPP


#include "apps/openmw/mwmp/ActorProcessor.hpp"
#include "apps/openmw/mwmp/Main.hpp"
#include "apps/openmw/mwmp/CellController.hpp"

namespace mwmp
{
    class ProcessorActorTest : public ActorProcessor
    {
    public:
        ProcessorActorTest()
        {
            BPP_INIT(ID_ACTOR_TEST);
        }

        virtual void Do(ActorPacket &packet, ActorList &actorList)
        {

        }
    };
}

#endif //OPENMW_PROCESSORACTORTEST_HPP
