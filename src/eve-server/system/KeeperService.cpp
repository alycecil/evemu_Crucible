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
    Updates:    Allan
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

#include "PyBoundObject.h"
#include "PyServiceCD.h"
#include "system/KeeperService.h"
#include "system/SystemManager.h"
#include "dungeon/DungeonDB.h"

KeeperBound::KeeperBound(PyServiceMgr *mgr, SystemDB *db)
: PyBoundObject(mgr),
m_db(db),
m_dispatch(new Dispatcher(this))
{
    _SetCallDispatcher(m_dispatch);

    m_strBoundObjectName = "KeeperBound";

    PyCallable_REG_CALL(KeeperBound, EditDungeon);
    PyCallable_REG_CALL(KeeperBound, PlayDungeon);
    PyCallable_REG_CALL(KeeperBound, Reset);
    PyCallable_REG_CALL(KeeperBound, GotoRoom); //(int room)
    PyCallable_REG_CALL(KeeperBound, GetCurrentlyEditedRoomID);
    PyCallable_REG_CALL(KeeperBound, GetRoomObjects);
    PyCallable_REG_CALL(KeeperBound, GetRoomGroups);
    PyCallable_REG_CALL(KeeperBound, ObjectSelection);
    PyCallable_REG_CALL(KeeperBound, BatchStart);
    PyCallable_REG_CALL(KeeperBound, BatchEnd);

}

PyCallable_Make_InnerDispatcher(KeeperService)

KeeperService::KeeperService(PyServiceMgr *mgr)
: PyService(mgr, "keeper"),
  m_dispatch(new Dispatcher(this))
{
    _SetCallDispatcher(m_dispatch);

    PyCallable_REG_CALL(KeeperService, GetLevelEditor);
	PyCallable_REG_CALL(KeeperService, ActivateAccelerationGate);
	PyCallable_REG_CALL(KeeperService, CanWarpToPathPlex);

    //sm.RemoteSvc('keeper').ClientBSDRevisionChange(action, schemaName, tableName, rowKeys, columnValues, reverting)
}

KeeperService::~KeeperService() {
    delete m_dispatch;
}

PyBoundObject *KeeperService::CreateBoundObject(Client *pClient, const PyRep *bind_args) {
    _log(DUNG__TRACE, "KeeperService bind request for:");
    bind_args->Dump(DUNG__TRACE, "    ");

    return new KeeperBound(m_manager, &m_db);
}

PyResult KeeperService::Handle_GetLevelEditor(PyCallArgs &call)
{
    // self.ed = sm.RemoteSvc('keeper').GetLevelEditor()  (this is to bind new editor object)
    _log(DUNG__CALL,  "KeeperService::Handle_GetLevelEditor  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    KeeperBound *ib = new KeeperBound(m_manager, &m_db);

    return m_manager->BindObject(call.client, ib);
}

PyResult KeeperService::Handle_CanWarpToPathPlex(PyCallArgs &call) {
    /*
                    resp = sm.RemoteSvc('keeper').CanWarpToPathPlex(node.rec.instanceID)
                if resp:
                    if resp is True:
                        m.append((uiutil.MenuLabel('UI/Inflight/WarpToBookmark'), self.WarpToHiddenDungeon, (node.id, node)))
                    else:
                        mickey = sm.StartService('michelle')
                        me = mickey.GetBall(eve.session.shipid)
                        dist = (foo.Vector3(resp) - foo.Vector3(me.x, me.y, me.z)).Length()

                        */
    _log(DUNG__CALL,  "KeeperService::Handle_CanWarpToPathPlex  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

	return nullptr;
}

/**  Hard-coded to random location....just to play with right now.
		will need to edit later to implement in missions/etc  */
PyResult KeeperService::Handle_ActivateAccelerationGate(PyCallArgs &call) {
    _log(DUNG__CALL,  "KeeperService::Handle_ActivateAccelerationGate  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    Call_SingleIntegerArg args;
    if (!args.Decode(&call.tuple)) {
        codelog(SERVICE__ERROR, "%s: Failed to decode arguments.", GetName());
        return nullptr;
    }

    Client *pClient(call.client);

    /** @todo   this should be called for gate... */
    //pClient->GetShipSE()->DestinyMgr()->SendSpecialEffect10(args.arg, 0, "effects.WarpGateEffect", 0, 1, 0);

    double distance = MakeRandomFloat(5, 25) * ONE_AU_IN_METERS;
    GPoint currentPosition(pClient->GetShipSE()->GetPosition());
    GPoint deltaPosition;
    deltaPosition.x = MakeRandomFloat(-1.0, 1.0) * distance;
    deltaPosition.y = MakeRandomFloat(-1.0, 1.0) * distance;
    deltaPosition.z = MakeRandomFloat(-2.0, 2.0) * ONE_AU_IN_METERS;
    GPoint warpToPoint(currentPosition+deltaPosition);              // Make a warp-in point variable
    GVector vectorToDestination(currentPosition, warpToPoint);
    double distanceToDestination = vectorToDestination.length();
    pClient->GetShipSE()->DestinyMgr()->WarpTo(warpToPoint, distanceToDestination);

    /* return error msg from this call, if applicable, else nodeid and timestamp */
    return new PyLong(Win32TimeNow());
}

PyResult KeeperBound::Handle_EditDungeon(PyCallArgs &call)
{
    //ed.EditDungeon(dungeonID, roomID=roomID)
    _log(DUNG__CALL,  "KeeperBound::Handle_EditDungeon  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    Call_SingleIntegerArg args;
    if (!args.Decode(&call.tuple)) {
        codelog(SERVICE__ERROR, "%s: Failed to decode arguments.", GetName());
        return nullptr;
    }

    /*
    Tasks to accomplish
    1. Set current position as 0,0,0 in room
    2. Load all room objects from dunRoomObjects into a vector
    3. Spawn all room objects using their relative positions from 0,0,0
    */

    Client *pClient(call.client);

    GPoint roomPos = pClient->GetShipSE()->GetPosition();

    pClient->GetSession()->SetFloat("editor_room_x", roomPos.x);
    pClient->GetSession()->SetFloat("editor_room_y", roomPos.y);
    pClient->GetSession()->SetFloat("editor_room_z", roomPos.z);

    pClient->GetSession()->SetInt("editor_bind_id", bindID());

    std::vector<Dungeon::RoomObject> objects;
    DungeonDB::GetRoomObjects(call.byname["roomID"]->AsInt()->value(), objects);

    // Spawn the items in the object list
    for (auto cur : objects) {
        GPoint objPos;
        objPos.x = roomPos.x + cur.x;
        objPos.y = roomPos.y + cur.y;
        objPos.z = roomPos.z + cur.z;

        ItemData dData(cur.typeID, 1/*EVE SYSTEM*/, pClient->GetLocationID(), flagNone, "", objPos);
        InventoryItemRef iRef = InventoryItem::SpawnItem(sItemFactory.GetNextTempID(), dData);
        if (iRef.get() == nullptr) // Failed to spawn the item
            continue;
        DungeonEditSE* oSE;
        oSE = new DungeonEditSE(iRef, *(m_manager), pClient->SystemMgr(), cur);
        m_roomObjects.push_back(oSE);
        pClient->SystemMgr()->AddEntity(oSE, false);
        // Set the radius from the data structure
        oSE->DestinyMgr()->SetRadius(cur.radius, true);
    }

    // Send notification to client to update UI
    PyList* posList = new PyList();
        posList->AddItem(new PyFloat(roomPos.x));
        posList->AddItem(new PyFloat(roomPos.y));
        posList->AddItem(new PyFloat(roomPos.z));

    PyTuple* payload = new PyTuple(3);
    payload->SetItem(0, new PyInt(args.arg)); //dungeonID
    payload->SetItem(1, new PyInt(call.byname["roomID"]->AsInt()->value())); //roomID
    payload->SetItem(2, posList); //roomPos

    pClient->SendNotification("OnDungeonEdit", "charid", payload, false);

    // update local variables with what we're editing right now
    this->m_currentRoom = call.byname["roomID"]->AsInt()->value();
    this->m_currentDungeon = args.arg;

    return nullptr;
}

PyResult KeeperBound::Handle_GetRoomObjects(PyCallArgs &call)
{
    _log(DUNG__CALL,  "KeeperBound:::Handle_GetRoomObjects  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    Call_SingleIntegerArg args;
    if (!args.Decode(&call.tuple))
    {
        _log(SERVICE__ERROR, "%s: Failed to decode arguments.", GetName());
        return nullptr;
    }

    DBRowDescriptor *header = new DBRowDescriptor();
    header->AddColumn("objectID", DBTYPE_I4);
    header->AddColumn("groupID", DBTYPE_I4);

    CRowSet *rowset = new CRowSet(&header);

    for (auto cur : m_roomObjects) {
        PyPackedRow *newRow = rowset->NewRow();
        newRow->SetField("objectID", new PyInt(cur->GetID()));
        newRow->SetField("groupID", new PyInt(cur->GetData().groupID));
    }

    return rowset;
}

PyResult KeeperBound::Handle_GetRoomGroups( PyCallArgs& call )
{
    _log(DUNG__CALL,  "KeeperBound::Handle_GetRoomGroups  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    Call_SingleIntegerArg args;
    if (!args.Decode(&call.tuple))
    {
        _log(SERVICE__ERROR, "%s: Failed to decode arguments.", GetName());
        return nullptr;
    }

    return DungeonDB::GetRoomGroups(args.arg);
}

PyResult KeeperBound::Handle_PlayDungeon(PyCallArgs &call)
{
    //ed.PlayDungeon(dungeonID, roomID=roomID, godmode=godmode)
    _log(DUNG__CALL,  "KeeperBound::Handle_PlayDungeon  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    return nullptr;
}

PyResult KeeperBound::Handle_Reset(PyCallArgs &call)
{
    _log(DUNG__CALL,  "KeeperBound::Handle_Reset  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    if (this->m_roomObjects.size())
        // reset means unload everything
        for (auto cur : m_roomObjects)
            cur->Delete();

    // empty the list
    this->m_roomObjects.clear();

    // make sure state is sent, this should call the correct flow in the client to update the item
    call.client->SetStateSent(false);
    call.client->GetShipSE()->DestinyMgr()->SendSetState();

    return nullptr;
}

PyResult KeeperBound::Handle_GotoRoom(PyCallArgs &call)
{
    _log(DUNG__CALL,  "KeeperBound::Handle_GotoRoom  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    return nullptr;
}

PyResult KeeperBound::Handle_GetCurrentlyEditedRoomID(PyCallArgs &call)
{
//return sm.RemoteSvc('keeper').GetLevelEditor().GetCurrentlyEditedRoomID()
    _log(DUNG__CALL,  "KeeperBound::Handle_GetCurrentlyEditedRoomID  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    return nullptr;
}

PyResult KeeperBound::Handle_ObjectSelection(PyCallArgs &call)
{
    _log(DUNG__CALL,  "KeeperBound::Handle_ObjectSelection  size: %li", call.tuple->size());
    call.Dump(DUNG__CALL_DUMP);

    Call_SingleIntList args;
    if (!args.Decode(&call.tuple))
    {
        _log(SERVICE__ERROR, "%s: Failed to decode arguments.", GetName());
        return nullptr;
    }

    this->m_selectedObjects = args.ints;

    // a copy of the list has to be returned, otherwise the client won't know what they're selecting
    // i feel like this would be handled client-side if the jessica package was loaded
    // but i have no real idea... so the server it is
    PyTuple* data = args.Encode();

    PyRep* result = data->GetItem(0);

    PySafeDecRef(data);

    return result;
}

PyResult KeeperBound::Handle_BatchStart(PyCallArgs &call)
{
    // nothing needed for now
    // might be used by CCP to lock selected items or something
    return nullptr;
}

PyResult KeeperBound::Handle_BatchEnd(PyCallArgs &call)
{
    // make sure state is sent, this should call the correct flow in the client to update the item
    call.client->SetStateSent(false);
    call.client->GetShipSE()->DestinyMgr()->SendSetState();

    // send new set state to the client
    return nullptr;
}

void KeeperBound::RemoveRoomObject(uint32 itemID)
{
    uint32 objectID = 0;
    for (std::vector <DungeonEditSE*> ::iterator cur = m_roomObjects.begin(); cur != m_roomObjects.end(); cur++) {
        if ((*cur)->GetID() == itemID) {
            objectID = (*cur)->GetData().objectID;
            (*cur)->Delete();
            SafeDelete(*cur);
            m_roomObjects.erase(cur);
            break;
        }
    }

    DungeonDB::DeleteObject(objectID);
}

DungeonEditSE* KeeperBound::GetRoomObject(uint32 itemID)
{
    for (std::vector <DungeonEditSE*> ::iterator cur = m_roomObjects.begin(); cur != m_roomObjects.end(); cur++) {
        if ((*cur)->GetID() == itemID)
            return (*cur);
    }

    return nullptr;
}