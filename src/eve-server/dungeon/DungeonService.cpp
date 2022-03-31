/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2006 - 2021 The EVEmu Team
    For the latest information visit https://evemu.dev
    ------------------------------------------------------------------------------------
    This program is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option) any later
    version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along with
    this program; if not, write to the Free Software Foundation, Inc., 59 Temple
    Place - Suite 330, Boston, MA 02111-1307, USA, or go to
    http://www.gnu.org/copyleft/lesser.txt.
    ------------------------------------------------------------------------------------
    Author:        Zhur
    Rewrite:    Allan
*/

/* Dungeon Logging
 * DUNG__ERROR
 * DUNG__WARNING
 * DUNG__INFO
 * DUNG__MESSAGE
 * DUNG__TRACE
 * DUNG__CALL
 * DUNG__CALL_DUMP
 * DUNG__RSP_DUMP
 * DUNG__DB_ERROR
 * DUNG__DB_WARNING
 * DUNG__DB_INFO
 * DUNG__DB_MESSAGE
 */

#include "eve-server.h"

#include "PyServiceCD.h"
#include "dungeon/DungeonService.h"
#include "dungeon/DungeonDB.h"
#include "system/cosmicMgrs/DungeonMgr.h"
#include "packets/Missions.h"

PyCallable_Make_InnerDispatcher(DungeonService)

DungeonService::DungeonService(PyServiceMgr *mgr)
: PyService(mgr, "dungeon"),
  m_dispatch(new Dispatcher(this))
{
    _SetCallDispatcher(m_dispatch);

    // objects
    PyCallable_REG_CALL(DungeonService, IsObjectLocked);
    PyCallable_REG_CALL(DungeonService, AddObject);
    PyCallable_REG_CALL(DungeonService, RemoveObject);
    PyCallable_REG_CALL(DungeonService, CopyObject);
    PyCallable_REG_CALL(DungeonService, EditObject);
    PyCallable_REG_CALL(DungeonService, EditObjectName);
    PyCallable_REG_CALL(DungeonService, EditObjectRadius);
    PyCallable_REG_CALL(DungeonService, EditObjectXYZ);
    PyCallable_REG_CALL(DungeonService, EditObjectYawPitchRoll);
    // templates
    PyCallable_REG_CALL(DungeonService, TemplateAdd);
    PyCallable_REG_CALL(DungeonService, TemplateRemove);
    PyCallable_REG_CALL(DungeonService, TemplateEdit);
    PyCallable_REG_CALL(DungeonService, AddTemplateObjects);
    PyCallable_REG_CALL(DungeonService, TemplateObjectAddDungeonList);
    // others
    PyCallable_REG_CALL(DungeonService, GetArchetypes);
    PyCallable_REG_CALL(DungeonService, DEGetFactions);
    PyCallable_REG_CALL(DungeonService, DEGetDungeons);
    PyCallable_REG_CALL(DungeonService, DEGetTemplates);
    PyCallable_REG_CALL(DungeonService, DEGetRooms);
    PyCallable_REG_CALL(DungeonService, DEGetRoomObjectPaletteData);
}

/*
== How Dungeons Are Organized ==

Dungeons
Templates
Rooms
Types
Factions
Groups
Objects

According to client/dungeonEditor:

Archetype ->  Dungeon -> Room -> Template -> Object
 / faction            -> Room -> Template -> Object
                      -> Room -> Template -> Object
              Dungeon -> Room -> Template -> Object
                      -> Room -> Template -> Object
                      -> Room -> Template -> Object

*/

DungeonService::~DungeonService() {
    delete m_dispatch;
}


PyResult DungeonService::Handle_IsObjectLocked( PyCallArgs& call )
{
    //return sm.RemoteSvc('dungeon').IsObjectLocked(objectID)
    _log(DUNG__CALL,  "DungeonService::Handle_IsObjectLocked size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    // TODO: IMPLEMENT THE LOCKING MECHANISM

    PyTuple* result = new PyTuple(2);

    result->SetItem(0, new PyBool(false));
    result->SetItem(1, new PyList());

    return result;
}

PyResult DungeonService::Handle_AddObject( PyCallArgs& call )
{
    // (newObjectID, revisionID,) = sm.RemoteSvc('dungeon').AddObject(roomID, typeID, x, y, z, yaw, pitch, roll, radius)

    _log(DUNG__CALL,  "DungeonService::Handle_AddObject size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    if(call.tuple->size() != 9) {
        codelog(SERVICE__ERROR, "Wrong number of arguments in call to AddObject");
        return NULL;
    }

    Dungeon::RoomObject newObject;
    newObject.roomID = PyRep::IntegerValue(call.tuple->GetItem(0));
    newObject.typeID = PyRep::IntegerValueU32(call.tuple->GetItem(1));
    newObject.x = PyRep::FloatValue(call.tuple->GetItem(2));
    newObject.y = PyRep::FloatValue(call.tuple->GetItem(3));
    newObject.z = PyRep::FloatValue(call.tuple->GetItem(4));
    newObject.yaw = PyRep::FloatValue(call.tuple->GetItem(5));
    newObject.pitch = PyRep::FloatValue(call.tuple->GetItem(6));
    newObject.roll = PyRep::FloatValue(call.tuple->GetItem(7));
    newObject.radius = PyRep::FloatValue(call.tuple->GetItem(8));

    uint32 groupID = DungeonDB::GetFirstGroupForRoom(newObject.roomID);

    newObject.objectID = DungeonDB::CreateObject(newObject.roomID, newObject.typeID, groupID, newObject.x, newObject.y, newObject.z, newObject.yaw, newObject.pitch, newObject.roll, newObject.radius);

    Client *pClient(call.client);

    GPoint objPos;
    objPos.x = newObject.x + pClient->GetSession()->GetCurrentFloat("editor_room_x");
    objPos.y = newObject.y + pClient->GetSession()->GetCurrentFloat("editor_room_y");
    objPos.z = newObject.z + pClient->GetSession()->GetCurrentFloat("editor_room_z");

    ItemData dData(newObject.typeID, 1/*EVE SYSTEM*/, pClient->GetLocationID(), flagNone, "", objPos);
    InventoryItemRef iRef = InventoryItem::SpawnItem(sItemFactory.GetNextTempID(), dData);
    if (iRef.get() == nullptr) {// Failed to spawn the item
        throw CustomError("Failed to spawn the item");
        return nullptr;
    }

    DungeonEditSE* oSE;
    oSE = new DungeonEditSE(iRef, *(m_manager), pClient->SystemMgr(), newObject);

    // Add the object to the room
    GetKeeperBound(pClient)->AddRoomObject(oSE);

    // Add the entity to the SystemManager
    pClient->SystemMgr()->AddEntity(oSE, false);

    // Return objectID and revisionID
    PyTuple *result = new PyTuple(2);
    result->SetItem(0, new PyInt(newObject.objectID));
    result->SetItem(1, new PyInt(1/*dummy value*/));

    return result;
}

PyResult DungeonService::Handle_RemoveObject( PyCallArgs& call )
{
    //sm.RemoteSvc('dungeon').RemoveObject(objectID)
    _log(DUNG__CALL,  "DungeonService::Handle_RemoveObject  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    if(call.tuple->size() != 1) {
        codelog(SERVICE__ERROR, "Wrong number of arguments in call to RemoveObject");
        return NULL;
    }

    uint32 objectID = PyRep::IntegerValue(call.tuple->GetItem(0));

    // Remove the object from the room
    GetKeeperBound(call.client)->RemoveRoomObject(objectID);

    return nullptr;
}

PyResult DungeonService::Handle_CopyObject( PyCallArgs& call )
{
    //newObjectID = sm.RemoteSvc('dungeon').CopyObject(objectID, roomID, offsetX, offsetY, offsetZ)
    _log(DUNG__CALL,  "DungeonService::Handle_CopyObject  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    if(call.tuple->size() != 5) {
        codelog(SERVICE__ERROR, "Wrong number of arguments in call to CopyObject");
        return NULL;
    }

    uint32 objectID = PyRep::IntegerValue(call.tuple->GetItem(0));
    uint32 roomID = PyRep::IntegerValue(call.tuple->GetItem(1));
    double offsetX = PyRep::FloatValue(call.tuple->GetItem(2));
    double offsetY = PyRep::FloatValue(call.tuple->GetItem(3));
    double offsetZ = PyRep::FloatValue(call.tuple->GetItem(4));

    // Copy the object to the room
    Dungeon::RoomObject newObject;
    newObject.roomID = roomID;
    newObject.typeID = GetKeeperBound(call.client)->GetRoomObject(objectID)->GetData().typeID;
    newObject.x = GetKeeperBound(call.client)->GetRoomObject(objectID)->GetData().x + offsetX;
    newObject.y = GetKeeperBound(call.client)->GetRoomObject(objectID)->GetData().y + offsetY;
    newObject.z = GetKeeperBound(call.client)->GetRoomObject(objectID)->GetData().z + offsetZ;
    newObject.yaw = GetKeeperBound(call.client)->GetRoomObject(objectID)->GetData().yaw;
    newObject.pitch = GetKeeperBound(call.client)->GetRoomObject(objectID)->GetData().pitch;
    newObject.roll = GetKeeperBound(call.client)->GetRoomObject(objectID)->GetData().roll;
    newObject.radius = GetKeeperBound(call.client)->GetRoomObject(objectID)->GetData().radius;

    uint32 groupID = DungeonDB::GetFirstGroupForRoom(newObject.roomID);

    newObject.objectID = DungeonDB::CreateObject(newObject.roomID, newObject.typeID, groupID, newObject.x, newObject.y, newObject.z, newObject.yaw, newObject.pitch, newObject.roll, newObject.radius);

    Client *pClient(call.client);

    GPoint objPos;

    objPos.x = newObject.x + pClient->GetSession()->GetCurrentFloat("editor_room_x");
    objPos.y = newObject.y + pClient->GetSession()->GetCurrentFloat("editor_room_y");
    objPos.z = newObject.z + pClient->GetSession()->GetCurrentFloat("editor_room_z");

    ItemData dData(newObject.typeID, 1/*EVE SYSTEM*/, pClient->GetLocationID(), flagNone, "", objPos);
    InventoryItemRef iRef = InventoryItem::SpawnItem(sItemFactory.GetNextTempID(), dData);
    if (iRef.get() == nullptr) {// Failed to spawn the item
        throw CustomError("Failed to spawn the item");
        return nullptr;
    }

    DungeonEditSE* oSE;
    oSE = new DungeonEditSE(iRef, *(m_manager), pClient->SystemMgr(), newObject);

    // Add the object to the room
    GetKeeperBound(pClient)->AddRoomObject(oSE);

    // Add the entity to the SystemManager
    pClient->SystemMgr()->AddEntity(oSE, false);

    return nullptr;
}

PyResult DungeonService::Handle_EditObject( PyCallArgs& call )
{
    _log(DUNG__CALL,  "DungeonService::Handle_EditObject  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    return nullptr;
}

PyResult DungeonService::Handle_EditObjectName( PyCallArgs& call )
{
    //sm.RemoteSvc('dungeon').EditObjectName(newObjectID, objectName)
    _log(DUNG__CALL,  "DungeonService::Handle_EditObjectName  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    if(call.tuple->size() != 2) {
        codelog(SERVICE__ERROR, "Wrong number of arguments in call to EditObjectName");
        return NULL;
    }

    uint32 objectID = PyRep::IntegerValue(call.tuple->GetItem(0));
    std::string objectName = call.tuple->GetItem(1)->AsString()->content();

    // Update the object name
    DungeonEditSE* oSE = GetKeeperBound(call.client)->GetRoomObject(objectID);

    oSE->Rename(objectName.c_str());

    return nullptr;
}

PyResult DungeonService::Handle_EditObjectRadius( PyCallArgs& call )
{
    _log(DUNG__CALL,  "DungeonService::Handle_EditObjectRadius  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    uint32 itemID = call.byname["objectID"]->AsInt()->value();
    double radius = call.byname["radius"]->AsFloat()->value();

    SystemEntity* entity = call.client->SystemMgr()->GetEntityByID(itemID);

    if (entity->IsDungeonEditSE() == false) {
        call.client->SendErrorMsg("The selected object is not part of the current dungeon");
        return nullptr;
    }

    DungeonEditSE* dungeonEntity = entity->GetDungeonEditSE();

    dungeonEntity->DestinyMgr()->SetRadius(radius, true);

    // save the position to the database
    DungeonDB::EditObjectRadius(dungeonEntity->GetData().objectID, radius);

    return nullptr;
}

PyResult DungeonService::Handle_EditObjectXYZ( PyCallArgs& call )
{
    _log(DUNG__CALL,  "DungeonService::Handle_EditObjectXYZ  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    uint32 itemID = call.byname["objectID"]->AsInt()->value();
    double x = call.byname["x"]->AsFloat()->value();
    double y = call.byname["y"]->AsFloat()->value();
    double z = call.byname["z"]->AsFloat()->value();

    SystemEntity* entity = call.client->SystemMgr()->GetEntityByID(itemID);

    if (entity->IsDungeonEditSE() == false) {
        call.client->SendErrorMsg("The selected object is not part of the current dungeon");
        return nullptr;
    }

    DungeonEditSE* dungeonEntity = entity->GetDungeonEditSE();

    // calculate the new position first
    GPoint roomPos = call.client->GetShipSE()->GetPosition();

    // set the new, correct position in space
    dungeonEntity->DestinyMgr()->SetPosition({
        roomPos.x + x,
        roomPos.y + y,
        roomPos.z + z
    });

    // save the position to the database
    DungeonDB::EditObjectXYZ(dungeonEntity->GetData().objectID, x, y, z);

    return nullptr;
}

PyResult DungeonService::Handle_EditObjectYawPitchRoll( PyCallArgs& call )
{
    _log(DUNG__CALL,  "DungeonService::Handle_EditObjectYawPitchRoll  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    uint32 itemID = call.byname["objectID"]->AsInt()->value();
    double yaw = call.byname["yaw"]->AsFloat()->value();
    double pitch = call.byname["pitch"]->AsFloat()->value();
    double roll = call.byname["roll"]->AsFloat()->value();

    SystemEntity* entity = call.client->SystemMgr()->GetEntityByID(itemID);

    if (entity->IsDungeonEditSE() == false) {
        call.client->SendErrorMsg("The selected object is not part of the current dungeon");
        return nullptr;
    }

    DungeonEditSE* dungeonEntity = entity->GetDungeonEditSE();

    // TODO: ADD SUPPORT FOR YAW, PITCH AND ROLL UPDATE

    // save the position to the database
    DungeonDB::EditObjectYawPitchRoll(dungeonEntity->GetData().objectID, yaw, pitch, roll);

    return nullptr;
}

PyResult DungeonService::Handle_TemplateAdd( PyCallArgs& call )
{
    //templateID = sm.RemoteSvc('dungeon').TemplateAdd(templateName, templateDescription)
    _log(DUNG__CALL,  "DungeonService::Handle_TemplateAdd  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    // Get the currently edited roomID from KeeperBound
    uint32 roomID = GetKeeperBound(call.client)->GetCurrentRoomID();

    if(call.tuple->size() != 2) {
        codelog(SERVICE__ERROR, "Wrong number of arguments in call to TemplateAdd");
        return NULL;
    }

    std::string templateName = call.tuple->GetItem(0)->AsWString()->content();
    std::string templateDescription = call.tuple->GetItem(1)->AsWString()->content();

    // Create the new template
    uint32 templateID = DungeonDB::CreateTemplate(templateName, templateDescription, roomID);

    return new PyInt(templateID);
}

PyResult DungeonService::Handle_TemplateRemove( PyCallArgs& call )
{
    //sm.RemoteSvc('dungeon').TemplateRemove(self.sr.node.id)
    _log(DUNG__CALL,  "DungeonService::Handle_TemplateRemove  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    DungeonDB::DeleteTemplate(call.tuple->GetItem(0)->AsInt()->value());

    return nullptr;
}

PyResult DungeonService::Handle_TemplateEdit( PyCallArgs& call )
{
    //dungeonSvc.TemplateEdit(self.templateRow.templateID, templateName, templateDescription)
    _log(DUNG__CALL,  "DungeonService::Handle_TemplateEdit  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    DungeonDB::EditTemplate(call.tuple->GetItem(0)->AsInt()->value(), call.tuple->GetItem(1)->AsWString()->content(), call.tuple->GetItem(2)->AsWString()->content());

    return nullptr;
}

PyResult DungeonService::Handle_AddTemplateObjects( PyCallArgs& call )
{
    // objectIDs = sm.RemoteSvc('dungeon').AddTemplateObjects(roomID, self.sr.node.id, (posInRoom.x, posInRoom.y, posInRoom.z)

    _log(DUNG__CALL,  "DungeonService::Handle_AddTemplateObjects  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    if(call.tuple->size() != 3) {
        codelog(SERVICE__ERROR, "Wrong number of arguments in call to AddTemplateObjects");
        return NULL;
    }

    uint32 roomID = call.tuple->GetItem(0)->AsInt()->value();
    uint32 templateID = call.tuple->GetItem(1)->AsInt()->value();
    double posInRoomX = call.tuple->GetItem(2)->AsTuple()->GetItem(0)->AsFloat()->value();
    double posInRoomY = call.tuple->GetItem(2)->AsTuple()->GetItem(1)->AsFloat()->value();
    double posInRoomZ = call.tuple->GetItem(2)->AsTuple()->GetItem(2)->AsFloat()->value();

    std::vector<Dungeon::RoomObject> objects;
    DungeonDB::GetTemplateObjects(templateID, objects);

    Client *pClient(call.client);
    PyList* objectIDs = new PyList();

    uint32 groupID = DungeonDB::GetFirstGroupForRoom(roomID);

    // Spawn the items in the object list
    for (auto cur : objects) {
        GPoint objPos;

        // Relative position for the object to be spawned at
        objPos.x = posInRoomX + cur.x + pClient->GetSession()->GetCurrentFloat("editor_room_x");
        objPos.y = posInRoomY + cur.y + pClient->GetSession()->GetCurrentFloat("editor_room_y");
        objPos.z = posInRoomZ + cur.z + pClient->GetSession()->GetCurrentFloat("editor_room_z");

        // Position to be stored in the DB
        double dbPosX = posInRoomX + cur.x;
        double dbPosY = posInRoomY + cur.y;
        double dbPosZ = posInRoomZ + cur.z;

        // Create the object in the database
        DungeonDB::CreateObject(roomID, cur.typeID, groupID, dbPosX, dbPosY, dbPosZ, cur.yaw, cur.pitch, cur.roll, cur.radius);

        ItemData dData(cur.typeID, 1/*EVE SYSTEM*/, pClient->GetLocationID(), flagNone, "", objPos);
        InventoryItemRef iRef = InventoryItem::SpawnItem(sItemFactory.GetNextTempID(), dData);
        if (iRef.get() == nullptr) // Failed to spawn the item
            continue;
        DungeonEditSE* oSE;
        oSE = new DungeonEditSE(iRef, *(m_manager), pClient->SystemMgr(), cur);

        GetKeeperBound(pClient)->AddRoomObject(oSE);
        pClient->SystemMgr()->AddEntity(oSE, false);

        objectIDs->AddItem(new PyInt(oSE->GetData().objectID));
    }

    return objectIDs;
}

PyResult DungeonService::Handle_TemplateObjectAddDungeonList( PyCallArgs& call )
{
    //sm.RemoteSvc('dungeon').TemplateObjectAddDungeonList(templateID, objectIDList)
    _log(DUNG__CALL,  "DungeonService::Handle_TemplateObjectAddDungeonList  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    return nullptr;
}


PyResult DungeonService::Handle_GetArchetypes( PyCallArgs& call )
{
    /**     archetypes = sm.RemoteSvc('dungeon').GetArchetypes()
     *        archetypeOptions = [ (archetype.archetypeName, archetype.archetypeID) for archetype in archetypes ]
     */
    _log(DUNG__CALL,  "DungeonService::Handle_GetArchetypes  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    DBQueryResult res;
    DungeonDB::GetArchetypes(res);

    return DBResultToCRowset(res);
}

PyResult DungeonService::Handle_DEGetDungeons( PyCallArgs& call )
{
    /* dungeon = sm.RemoteSvc('dungeon').DEGetDungeons(archetypeID=archetypeID, factionID=factionID)
     * dungeon = sm.RemoteSvc('dungeon').DEGetDungeons(dungeonID=dungeonID)[0]
     * dungeon.dungeonNameID, dungeon.dungeonID, dungeon.factionID
     */

    //PyRep *result = NULL;
    //dict args:
    // archetypeID, factionID
    // or dungeonVID

    // rows: status (1=RELEASE,2=TESTING,else Working Copy),
    //       dungeonVName
    //       dungeonVID

    _log(DUNG__CALL,  "DungeonService::Handle_DEGetDungeons  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    DBQueryResult res;

    if (call.tuple->size() == 0) { // No args
        DungeonDB::GetDungeons(res);
    } else if (call.tuple->size() == 1) { // dungeonID
        DungeonDB::GetDungeons(call.byname["dungeonID"]->AsInt()->value(), res);
    } else if (call.tuple->size() == 2) { // archetypeID, factionID
        DungeonDB::GetDungeons(call.byname["archetypeID"]->AsInt()->value(), call.byname["factionID"]->AsInt()->value(), res);
    }
    return DBResultToCRowset(res);
}

PyResult DungeonService::Handle_DEGetTemplates( PyCallArgs& call )
{
        /*
        self.templateRows = sm.RemoteSvc('dungeon').DEGetTemplates()
        for row in self.templateRows:
            data = {'label': row.templateName,
             'hint': row.description != row.templateName and row.description or '',
             'id': row.templateID,
             'form': self}
             */

    _log(DUNG__CALL,  "DungeonService::Handle_DEGetTemplates  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    DBQueryResult res;
    DungeonDB::GetTemplates(res, call.client->GetUserID());

    return DBResultToCRowset(res);
}


PyResult DungeonService::Handle_DEGetRooms( PyCallArgs& call )
{
    //rooms = sm.RemoteSvc('dungeon').DEGetRooms(dungeonID=seldungeon.dungeonID)
    //dict arg: dungeonVID
    //PyRep *result = NULL;

    //rows: roomName

    _log(DUNG__CALL,  "DungeonService::Handle_DEGetRooms  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    DBQueryResult res;
    DungeonDB::GetRooms(call.byname["dungeonID"]->AsInt()->value(), res);

    return DBResultToCRowset(res);
}

PyResult DungeonService::Handle_DEGetRoomObjectPaletteData( PyCallArgs& call )
{
    /* Object Pallete Data = Object Groups 
     * roomObjectGroups = sm.RemoteSvc('dungeon').DEGetRoomObjectPaletteData()
     * id, name  in either dict or list (client can process both)
     */
    _log(DUNG__CALL,  "DungeonService::Handle_DEGetRoomObjectPaletteData  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    DBQueryResult res;
    DungeonDB::GetGroups(res);

    std::map<uint16, Inv::TypeData> types;
    sDataMgr.GetTypes(types);

    PyList* list = new PyList();

    for (auto cur : types) {
        Inv::GrpData grp;
        sDataMgr.GetGroup(cur.second.groupID, grp);
        if (grp.catID == EVEDB::invCategories::Celestial) {
            PyTuple* tuple = new PyTuple(2);
            tuple->items[0] = new PyInt(cur.second.id);
            tuple->items[1] = new PyString(cur.second.name);
            list->AddItem(tuple);
        }
    }

    return list;
}

PyResult DungeonService::Handle_DEGetFactions( PyCallArgs& call )
{
    /* factions = sm.RemoteSvc('dungeon').DEGetFactions()
        factionOptions = [ (cfg.eveowners.Get(faction.factionID).name, faction.factionID) for faction in factions ]
        */
    _log(DUNG__CALL,  "DungeonService::Handle_DEGetFactions  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    DBQueryResult res;
    DungeonDB::GetFactions(res);
    return DBResultToCRowset(res);
}

/*{'messageKey': 'DunAuthoringError', 'dataID': 17883918, 'suppressable': False, 'bodyID': 259674, 'messageType': 'warning', 'urlAudio': '', 'urlIcon': '', 'titleID': None, 'messageID': 830}
 * {'messageKey': 'DunBlacklistCannotWarp', 'dataID': 17880454, 'suppressable': False, 'bodyID': 258397, 'messageType': 'info', 'urlAudio': '', 'urlIcon': '', 'titleID': 258396, 'messageID': 2100}
 * {'messageKey': 'DunCantAnchor', 'dataID': 17883867, 'suppressable': False, 'bodyID': 259656, 'messageType': 'notify', 'urlAudio': '', 'urlIcon': '', 'titleID': None, 'messageID': 831}
 * {'messageKey': 'DunCantUseMWD', 'dataID': 17883442, 'suppressable': False, 'bodyID': 259505, 'messageType': 'notify', 'urlAudio': '', 'urlIcon': '', 'titleID': None, 'messageID': 832}
 * {'messageKey': 'DunGateLocked_ManyKeys', 'dataID': 17883928, 'suppressable': False, 'bodyID': 259678, 'messageType': 'info', 'urlAudio': '', 'urlIcon': '', 'titleID': 259677, 'messageID': 834}
 * {'messageKey': 'DunGateLocked_OneKey', 'dataID': 17883923, 'suppressable': False, 'bodyID': 259676, 'messageType': 'info', 'urlAudio': '', 'urlIcon': '', 'titleID': 259675, 'messageID': 835}
 * {'messageKey': 'DunGateLocked_ZeroKey', 'dataID': 17883939, 'suppressable': False, 'bodyID': 259682, 'messageType': 'info', 'urlAudio': '', 'urlIcon': '', 'titleID': 259681, 'messageID': 836}
 * {'messageKey': 'DunGateNPCsAround', 'dataID': 17883447, 'suppressable': False, 'bodyID': 259507, 'messageType': 'info', 'urlAudio': '', 'urlIcon': '', 'titleID': 259506, 'messageID': 837}
 * {'messageKey': 'DunGateNoSkill', 'dataID': 17883915, 'suppressable': False, 'bodyID': 259673, 'messageType': 'info', 'urlAudio': '', 'urlIcon': '', 'titleID': 259672, 'messageID': 838}
 * {'messageKey': 'DunGateTooFarAway', 'dataID': 17883844, 'suppressable': False, 'bodyID': 259648, 'messageType': 'info', 'urlAudio': '', 'urlIcon': '', 'titleID': 259647, 'messageID': 839}
 * {'messageKey': 'DunPodsCannotWarp', 'dataID': 17883452, 'suppressable': False, 'bodyID': 259509, 'messageType': 'info', 'urlAudio': '', 'urlIcon': '', 'titleID': 259508, 'messageID': 840}
 * {'messageKey': 'DunShipCannotWarp', 'dataID': 17883907, 'suppressable': False, 'bodyID': 259670, 'messageType': 'info', 'urlAudio': '', 'urlIcon': '', 'titleID': 259669, 'messageID': 841}
 */