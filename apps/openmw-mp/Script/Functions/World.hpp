#ifndef OPENMW_WORLDAPI_HPP
#define OPENMW_WORLDAPI_HPP

#define WORLDAPI \
    {"ReadLastEvent",               WorldFunctions::ReadLastEvent},\
    {"InitiateEvent",               WorldFunctions::InitiateEvent},\
    \
    {"GetObjectChangesSize",        WorldFunctions::GetObjectChangesSize},\
    {"GetEventAction",              WorldFunctions::GetEventAction},\
    \
    {"GetObjectRefId",              WorldFunctions::GetObjectRefId},\
    {"GetObjectRefNumIndex",        WorldFunctions::GetObjectRefNumIndex},\
    {"GetObjectMpNum",              WorldFunctions::GetObjectMpNum},\
    {"GetObjectCount",              WorldFunctions::GetObjectCount},\
    {"GetObjectCharge",             WorldFunctions::GetObjectCharge},\
    {"GetObjectGoldValue",          WorldFunctions::GetObjectGoldValue},\
    {"GetObjectScale",              WorldFunctions::GetObjectScale},\
    {"GetObjectDoorState",          WorldFunctions::GetObjectDoorState},\
    {"GetObjectLockLevel",          WorldFunctions::GetObjectLockLevel},\
    {"GetObjectPosX",               WorldFunctions::GetObjectPosX},\
    {"GetObjectPosY",               WorldFunctions::GetObjectPosY},\
    {"GetObjectPosZ",               WorldFunctions::GetObjectPosZ},\
    {"GetObjectRotX",               WorldFunctions::GetObjectRotX},\
    {"GetObjectRotY",               WorldFunctions::GetObjectRotY},\
    {"GetObjectRotZ",               WorldFunctions::GetObjectRotZ},\
    \
    {"GetContainerChangesSize",     WorldFunctions::GetContainerChangesSize},\
    {"GetContainerItemRefId",       WorldFunctions::GetContainerItemRefId},\
    {"GetContainerItemCount",       WorldFunctions::GetContainerItemCount},\
    {"GetContainerItemCharge",      WorldFunctions::GetContainerItemCharge},\
    {"GetContainerItemActionCount", WorldFunctions::GetContainerItemActionCount},\
    \
    {"SetEventCell",                WorldFunctions::SetEventCell},\
    {"SetEventAction",              WorldFunctions::SetEventAction},\
    \
    {"SetObjectRefId",              WorldFunctions::SetObjectRefId},\
    {"SetObjectRefNumIndex",        WorldFunctions::SetObjectRefNumIndex},\
    {"SetObjectMpNum",              WorldFunctions::SetObjectMpNum},\
    {"SetObjectCount",              WorldFunctions::SetObjectCount},\
    {"SetObjectCharge",             WorldFunctions::SetObjectCharge},\
    {"SetObjectGoldValue",          WorldFunctions::SetObjectGoldValue},\
    {"SetObjectScale",              WorldFunctions::SetObjectScale},\
    {"SetObjectDoorState",          WorldFunctions::SetObjectDoorState},\
    {"SetObjectLockLevel",          WorldFunctions::SetObjectLockLevel},\
    {"SetObjectDisarmState",        WorldFunctions::SetObjectDisarmState},\
    {"SetObjectMasterState",        WorldFunctions::SetObjectMasterState},\
    {"SetObjectPosition",           WorldFunctions::SetObjectPosition},\
    {"SetObjectRotation",           WorldFunctions::SetObjectRotation},\
    \
    {"SetContainerItemRefId",       WorldFunctions::SetContainerItemRefId},\
    {"SetContainerItemCount",       WorldFunctions::SetContainerItemCount},\
    {"SetContainerItemCharge",      WorldFunctions::SetContainerItemCharge},\
    \
    {"AddWorldObject",              WorldFunctions::AddWorldObject},\
    {"AddContainerItem",            WorldFunctions::AddContainerItem},\
    \
    {"SendObjectPlace",             WorldFunctions::SendObjectPlace},\
    {"SendObjectSpawn",             WorldFunctions::SendObjectSpawn},\
    {"SendObjectDelete",            WorldFunctions::SendObjectDelete},\
    {"SendObjectLock",              WorldFunctions::SendObjectLock},\
    {"SendObjectTrap",              WorldFunctions::SendObjectTrap},\
    {"SendObjectScale",             WorldFunctions::SendObjectScale},\
    {"SendDoorState",               WorldFunctions::SendDoorState},\
    {"SendContainer",               WorldFunctions::SendContainer},\
    \
    {"SetHour",                     WorldFunctions::SetHour},\
    {"SetMonth",                    WorldFunctions::SetMonth},\
    {"SetDay",                      WorldFunctions::SetDay}

class WorldFunctions
{
public:

    static void ReadLastEvent() noexcept;
    static void InitiateEvent(unsigned short pid) noexcept;

    static unsigned int GetObjectChangesSize() noexcept;
    static unsigned char GetEventAction() noexcept;

    static const char *GetObjectRefId(unsigned int i) noexcept;
    static int GetObjectRefNumIndex(unsigned int i) noexcept;
    static int GetObjectMpNum(unsigned int i) noexcept;
    static int GetObjectCount(unsigned int i) noexcept;
    static int GetObjectCharge(unsigned int i) noexcept;
    static int GetObjectGoldValue(unsigned int i) noexcept;
    static double GetObjectScale(unsigned int i) noexcept;
    static int GetObjectDoorState(unsigned int i) noexcept;
    static int GetObjectLockLevel(unsigned int i) noexcept;
    static double GetObjectPosX(unsigned int i) noexcept;
    static double GetObjectPosY(unsigned int i) noexcept;
    static double GetObjectPosZ(unsigned int i) noexcept;
    static double GetObjectRotX(unsigned int i) noexcept;
    static double GetObjectRotY(unsigned int i) noexcept;
    static double GetObjectRotZ(unsigned int i) noexcept;

    static unsigned int GetContainerChangesSize(unsigned int objectIndex) noexcept;
    static const char *GetContainerItemRefId(unsigned int objectIndex, unsigned int itemIndex) noexcept;
    static int GetContainerItemCount(unsigned int objectIndex, unsigned int itemIndex) noexcept;
    static int GetContainerItemCharge(unsigned int objectIndex, unsigned int itemIndex) noexcept;
    static int GetContainerItemActionCount(unsigned int objectIndex, unsigned int itemIndex) noexcept;

    static void SetEventCell(const char* cellDescription) noexcept;
    static void SetEventAction(unsigned char action) noexcept;

    static void SetObjectRefId(const char* refId) noexcept;
    static void SetObjectRefNumIndex(int refNumIndex) noexcept;
    static void SetObjectMpNum(int mpNum) noexcept;
    static void SetObjectCount(int count) noexcept;
    static void SetObjectCharge(int charge) noexcept;
    static void SetObjectGoldValue(int goldValue) noexcept;
    static void SetObjectScale(double scale) noexcept;
    static void SetObjectDoorState(int doorState) noexcept;
    static void SetObjectLockLevel(int lockLevel) noexcept;
    static void SetObjectDisarmState(bool disarmState) noexcept;
    static void SetObjectMasterState(bool masterState) noexcept;
    static void SetObjectPosition(double x, double y, double z) noexcept;
    static void SetObjectRotation(double x, double y, double z) noexcept;

    static void SetContainerItemRefId(const char* refId) noexcept;
    static void SetContainerItemCount(int count) noexcept;
    static void SetContainerItemCharge(int charge) noexcept;

    static void AddWorldObject() noexcept;
    static void AddContainerItem() noexcept;

    static void SendObjectPlace() noexcept;
    static void SendObjectSpawn() noexcept;
    static void SendObjectDelete() noexcept;
    static void SendObjectLock() noexcept;
    static void SendObjectTrap() noexcept;
    static void SendObjectScale() noexcept;
    static void SendDoorState() noexcept;
    static void SendContainer() noexcept;

    static void SetHour(unsigned short pid, double hour) noexcept;
    static void SetMonth(unsigned short pid, int month) noexcept;
    static void SetDay(unsigned short pid, int day) noexcept;
};


#endif //OPENMW_WORLDAPI_HPP
