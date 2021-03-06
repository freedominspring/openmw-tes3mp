#ifndef OPENMW_ACTORAPI_HPP
#define OPENMW_ACTORAPI_HPP

#define ACTORAPI \
    {"ReadLastActorList",           ActorFunctions::ReadLastActorList},\
    {"ReadCellActorList",           ActorFunctions::ReadCellActorList},\
    {"InitializeActorList",         ActorFunctions::InitializeActorList},\
    \
    {"GetActorListSize",            ActorFunctions::GetActorListSize},\
    {"GetActorListAction",          ActorFunctions::GetActorListAction},\
    \
    {"GetActorCell",                ActorFunctions::GetActorCell},\
    {"GetActorRefId",               ActorFunctions::GetActorRefId},\
    {"GetActorRefNumIndex",         ActorFunctions::GetActorRefNumIndex},\
    {"GetActorMpNum",               ActorFunctions::GetActorMpNum},\
    \
    {"GetActorPosX",                ActorFunctions::GetActorPosX},\
    {"GetActorPosY",                ActorFunctions::GetActorPosY},\
    {"GetActorPosZ",                ActorFunctions::GetActorPosZ},\
    {"GetActorRotX",                ActorFunctions::GetActorRotX},\
    {"GetActorRotY",                ActorFunctions::GetActorRotY},\
    {"GetActorRotZ",                ActorFunctions::GetActorRotZ},\
    \
    {"GetActorHealthBase",          ActorFunctions::GetActorHealthBase},\
    {"GetActorHealthCurrent",       ActorFunctions::GetActorHealthCurrent},\
    {"GetActorHealthModified",      ActorFunctions::GetActorHealthModified},\
    {"GetActorMagickaBase",         ActorFunctions::GetActorMagickaBase},\
    {"GetActorMagickaCurrent",      ActorFunctions::GetActorMagickaCurrent},\
    {"GetActorMagickaModified",     ActorFunctions::GetActorMagickaModified},\
    {"GetActorFatigueBase",         ActorFunctions::GetActorFatigueBase},\
    {"GetActorFatigueCurrent",      ActorFunctions::GetActorFatigueCurrent},\
    {"GetActorFatigueModified",     ActorFunctions::GetActorFatigueModified},\
    \
    {"GetActorEquipmentItemRefId",  ActorFunctions::GetActorEquipmentItemRefId},\
    {"GetActorEquipmentItemCount",  ActorFunctions::GetActorEquipmentItemCount},\
    {"GetActorEquipmentItemCharge", ActorFunctions::GetActorEquipmentItemCharge},\
    \
    {"DoesActorHavePosition",       ActorFunctions::DoesActorHavePosition},\
    {"DoesActorHaveStatsDynamic",   ActorFunctions::DoesActorHaveStatsDynamic},\
    \
    {"SetActorListCell",            ActorFunctions::SetActorListCell},\
    {"SetActorListAction",          ActorFunctions::SetActorListAction},\
    \
    {"SetActorCell",                ActorFunctions::SetActorCell},\
    {"SetActorRefId",               ActorFunctions::SetActorRefId},\
    {"SetActorRefNumIndex",         ActorFunctions::SetActorRefNumIndex},\
    {"SetActorMpNum",               ActorFunctions::SetActorMpNum},\
    \
    {"SetActorPosition",            ActorFunctions::SetActorPosition},\
    {"SetActorRotation",            ActorFunctions::SetActorRotation},\
    \
    {"SetActorHealthBase",          ActorFunctions::SetActorHealthBase},\
    {"SetActorHealthCurrent",       ActorFunctions::SetActorHealthCurrent},\
    {"SetActorHealthModified",      ActorFunctions::SetActorHealthModified},\
    {"SetActorMagickaBase",         ActorFunctions::SetActorMagickaBase},\
    {"SetActorMagickaCurrent",      ActorFunctions::SetActorMagickaCurrent},\
    {"SetActorMagickaModified",     ActorFunctions::SetActorMagickaModified},\
    {"SetActorFatigueBase",         ActorFunctions::SetActorFatigueBase},\
    {"SetActorFatigueCurrent",      ActorFunctions::SetActorFatigueCurrent},\
    {"SetActorFatigueModified",     ActorFunctions::SetActorFatigueModified},\
    \
    {"EquipActorItem",              ActorFunctions::EquipActorItem},\
    {"UnequipActorItem",            ActorFunctions::UnequipActorItem},\
    \
    {"AddActor",                    ActorFunctions::AddActor},\
    \
    {"SendActorList",               ActorFunctions::SendActorList},\
    {"SendActorAuthority",          ActorFunctions::SendActorAuthority},\
    {"SendActorPosition",           ActorFunctions::SendActorPosition},\
    {"SendActorStatsDynamic",       ActorFunctions::SendActorStatsDynamic},\
    {"SendActorEquipment",          ActorFunctions::SendActorEquipment},\
    {"SendActorCellChange",         ActorFunctions::SendActorCellChange}

class ActorFunctions
{
public:

    static void ReadLastActorList() noexcept;
    static void ReadCellActorList(const char* cellDescription) noexcept;
    static void InitializeActorList(unsigned short pid) noexcept;

    static unsigned int GetActorListSize() noexcept;
    static unsigned char GetActorListAction() noexcept;

    static const char *GetActorCell(unsigned int i) noexcept;
    static const char *GetActorRefId(unsigned int i) noexcept;
    static int GetActorRefNumIndex(unsigned int i) noexcept;
    static int GetActorMpNum(unsigned int i) noexcept;

    static double GetActorPosX(unsigned int i) noexcept;
    static double GetActorPosY(unsigned int i) noexcept;
    static double GetActorPosZ(unsigned int i) noexcept;
    static double GetActorRotX(unsigned int i) noexcept;
    static double GetActorRotY(unsigned int i) noexcept;
    static double GetActorRotZ(unsigned int i) noexcept;

    static double GetActorHealthBase(unsigned int i) noexcept;
    static double GetActorHealthCurrent(unsigned int i) noexcept;
    static double GetActorHealthModified(unsigned int i) noexcept;
    static double GetActorMagickaBase(unsigned int i) noexcept;
    static double GetActorMagickaCurrent(unsigned int i) noexcept;
    static double GetActorMagickaModified(unsigned int i) noexcept;
    static double GetActorFatigueBase(unsigned int i) noexcept;
    static double GetActorFatigueCurrent(unsigned int i) noexcept;
    static double GetActorFatigueModified(unsigned int i) noexcept;

    static const char *GetActorEquipmentItemRefId(unsigned int i, unsigned short slot) noexcept;
    static int GetActorEquipmentItemCount(unsigned int i, unsigned short slot) noexcept;
    static int GetActorEquipmentItemCharge(unsigned int i, unsigned short slot) noexcept;

    static bool DoesActorHavePosition(unsigned int i) noexcept;
    static bool DoesActorHaveStatsDynamic(unsigned int i) noexcept;

    static void SetActorListCell(const char* cellDescription) noexcept;
    static void SetActorListAction(unsigned char action) noexcept;

    static void SetActorCell(const char* cellDescription) noexcept;
    static void SetActorRefId(const char* refId) noexcept;
    static void SetActorRefNumIndex(int refNumIndex) noexcept;
    static void SetActorMpNum(int mpNum) noexcept;

    static void SetActorPosition(double x, double y, double z) noexcept;
    static void SetActorRotation(double x, double y, double z) noexcept;

    static void SetActorHealthBase(double value) noexcept;
    static void SetActorHealthCurrent(double value) noexcept;
    static void SetActorHealthModified(double value) noexcept;
    static void SetActorMagickaBase(double value) noexcept;
    static void SetActorMagickaCurrent(double value) noexcept;
    static void SetActorMagickaModified(double value) noexcept;
    static void SetActorFatigueBase(double value) noexcept;
    static void SetActorFatigueCurrent(double value) noexcept;
    static void SetActorFatigueModified(double value) noexcept;

    static void EquipActorItem(unsigned short slot, const char* refId, unsigned int count, int charge) noexcept;
    static void UnequipActorItem(unsigned short slot) noexcept;

    static void AddActor() noexcept;

    static void SendActorList() noexcept;
    static void SendActorAuthority() noexcept;
    static void SendActorPosition() noexcept;
    static void SendActorStatsDynamic() noexcept;
    static void SendActorEquipment() noexcept;
    static void SendActorCellChange() noexcept;
};


#endif //OPENMW_ACTORAPI_HPP
