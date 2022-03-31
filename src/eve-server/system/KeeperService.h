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

#ifndef __KEEPER_SERVICE_H_INCL__
#define __KEEPER_SERVICE_H_INCL__

#include "system/SystemDB.h"
#include "PyService.h"
#include "PyServiceCD.h"
#include "PyBoundObject.h"

class KeeperService
: public PyService
{
public:
    KeeperService(PyServiceMgr *mgr);
    virtual ~KeeperService();

protected:
    class Dispatcher;
    Dispatcher *const m_dispatch;

    SystemDB m_db;

    PyCallable_DECL_CALL(GetLevelEditor);
	PyCallable_DECL_CALL(ActivateAccelerationGate);
	PyCallable_DECL_CALL(CanWarpToPathPlex);

    //overloaded in order to support bound objects:
    virtual PyBoundObject *CreateBoundObject(Client *pClient, const PyRep *bind_args);
};


class KeeperBound
: public PyBoundObject
{
public:

    PyCallable_Make_Dispatcher(KeeperBound);

    KeeperBound(PyServiceMgr *mgr, SystemDB *db);
    virtual ~KeeperBound() { delete m_dispatch; }
    virtual void Release() {
        //I hate this statement
        delete this;
    }

    PyCallable_DECL_CALL(EditDungeon);
    PyCallable_DECL_CALL(PlayDungeon);
    PyCallable_DECL_CALL(Reset);
    PyCallable_DECL_CALL(GotoRoom);
    PyCallable_DECL_CALL(GetCurrentlyEditedRoomID);
    PyCallable_DECL_CALL(GetRoomObjects);
    PyCallable_DECL_CALL(GetRoomGroups);
    PyCallable_DECL_CALL(ObjectSelection);
    PyCallable_DECL_CALL(BatchStart);
    PyCallable_DECL_CALL(BatchEnd);

    virtual void AddRoomObject(DungeonEditSE *pSE) { m_roomObjects.push_back(pSE); }
    virtual void RemoveRoomObject(uint32 itemID);
    DungeonEditSE* GetRoomObject(uint32 itemID);
    virtual uint32 GetCurrentRoomID() { return m_currentRoom; }

protected:
    SystemDB *const m_db;

    Dispatcher *const m_dispatch;   //we own this

private:
    uint32 m_currentDungeon;
    uint32 m_currentRoom;
    std::vector<DungeonEditSE*> m_roomObjects;
    std::vector<int32> m_selectedObjects;
};


#endif


