/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2024 Source2ZE
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <../cs2fixes.h>
#include "addresses.h"
#include "utlstring.h"
#include "playermanager.h"
#include "entity/ccsplayercontroller.h"
#include "recipientfilters.h"
#include "tracefilter.h"

#define VPROF_ENABLED
#include "tier0/vprof.h"

#include "tier0/memdbgon.h"


extern IVEngineServer2 *g_pEngineServer2;
extern CGameEntitySystem *g_pEntitySystem;
extern CGlobalVars *gpGlobals;

ZEPlayerHandle::ZEPlayerHandle() : m_Index(INVALID_ZEPLAYERHANDLE_INDEX) {};

ZEPlayerHandle::ZEPlayerHandle(CPlayerSlot slot)
{
	m_Parts.m_PlayerSlot = slot.Get();
	m_Parts.m_Serial = ++iZEPlayerHandleSerial;
}

ZEPlayerHandle::ZEPlayerHandle(const ZEPlayerHandle &other)
{
	m_Index = other.m_Index;
}

ZEPlayerHandle::ZEPlayerHandle(ZEPlayer *pZEPlayer)
{
	Set(pZEPlayer);
}

bool ZEPlayerHandle::operator==(ZEPlayer *pZEPlayer) const
{
	return Get() == pZEPlayer;
}

bool ZEPlayerHandle::operator!=(ZEPlayer *pZEPlayer) const
{
	return Get() != pZEPlayer;
}

void ZEPlayerHandle::Set(ZEPlayer *pZEPlayer)
{
	if (pZEPlayer)
		m_Index = pZEPlayer->GetHandle().m_Index;
	else
		m_Index = INVALID_ZEPLAYERHANDLE_INDEX;
}

ZEPlayer *ZEPlayerHandle::Get() const
{
	ZEPlayer *pZEPlayer = g_playerManager->GetPlayer((CPlayerSlot) m_Parts.m_PlayerSlot);

	if (!pZEPlayer)
		return nullptr;
	
	if (pZEPlayer->GetHandle().m_Index != m_Index)
		return nullptr;
	
	return pZEPlayer;
}

void ZEPlayer::OnAuthenticated()
{
	m_bAuthenticated = true;
	m_SteamID = m_UnauthenticatedSteamID;

	Message("%lli authenticated\n", GetSteamId64());
}

bool CPlayerManager::OnClientConnected(CPlayerSlot slot, uint64 xuid)
{
	ZEPlayer *pPlayer = GetPlayer(slot);
	if (pPlayer)
	{
		delete pPlayer;
	}

	Message("%d connected\n", slot.Get());

	pPlayer = new ZEPlayer(slot);
	pPlayer->SetUnauthenticatedSteamId(new CSteamID(xuid));

	pPlayer->SetConnected();
	m_vecPlayers.at(slot.Get()) = pPlayer;

	return true;
}

void CPlayerManager::OnLateLoad()
{
	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);

		if (!pController || !pController->IsController() || !pController->IsConnected())
			continue;

		OnClientConnected(i, pController->m_steamID());
	}
}

ZEPlayer *CPlayerManager::GetPlayer(CPlayerSlot slot)
{
	if (slot.Get() < 0 || slot.Get() >= gpGlobals->maxClients)
		return nullptr;

	return m_vecPlayers.at(slot.Get());
};

// In userids, the lower byte is always the player slot
CPlayerSlot CPlayerManager::GetSlotFromUserId(uint16 userid)
{
	return CPlayerSlot(userid & 0xFF);
}

ZEPlayer *CPlayerManager::GetPlayerFromUserId(uint16 userid)
{
	uint8 index = userid & 0xFF;

	if (index >= gpGlobals->maxClients)
		return nullptr;

	return m_vecPlayers.at(index);
}

ZEPlayer* CPlayerManager::GetPlayerFromSteamId(uint64 steamid)
{
	for (ZEPlayer* player : m_vecPlayers)
	{
		if (player && player->IsAuthenticated() && player->GetSteamId64() == steamid)
			return player;
	}

	return nullptr;
}

CCSPlayerController *ZEPlayer::GetController()
{
	return CCSPlayerController::FromSlot(this->GetPlayerSlot());
}

CCSPlayerPawn *ZEPlayer::GetPawn()
{
	CCSPlayerController *controller = this->GetController();
	if (!controller)
	{
		return nullptr;
	}
	return controller->m_hPlayerPawn().Get();
}

void ZEPlayer::GetOrigin(Vector *origin)
{
	// if (this->processingMovement && this->currentMoveData)
	//  {
	//  	*origin = this->currentMoveData->m_vecAbsOrigin;
	//  }
	//  else
	//  {
	CBasePlayerPawn *pawn = this->GetPawn();
	if (!pawn)
	{
		return;
	}
	*origin = pawn->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin();
	// }
}

void ZEPlayer::SetOrigin(const Vector &origin)
{
	if (this->processingMovement && this->currentMoveData)
	{
		this->currentMoveData->m_vecAbsOrigin = origin;
	}
	else
	{
		CBasePlayerPawn *pawn = this->GetPawn();
		if (!pawn)
		{
			return;
		}
		pawn->Teleport(&origin, NULL, NULL);
	}
}

void ZEPlayer::GetVelocity(Vector *velocity)
{
	if (this->processingMovement && this->currentMoveData)
	{
		*velocity = this->currentMoveData->m_vecVelocity;
	}
	else
	{
	CBasePlayerPawn *pawn = this->GetPawn();
	if (!pawn)
	{
		return;
	}
	*velocity = pawn->m_vecAbsVelocity();
	}
}

void ZEPlayer::SetVelocity(const Vector &velocity)
{
	if (this->processingMovement && this->currentMoveData)
	{
		this->currentMoveData->m_vecVelocity = velocity;
	}
	else
	{
		CBasePlayerPawn *pawn = this->GetPawn();
		if (!pawn)
		{
			return;
		}
		pawn->Teleport(NULL, NULL, &velocity);
	}
}

void ZEPlayer::GetBBoxBounds(bbox_t *bounds)
{
	if (!bounds)
	{
		return;
	}

	bounds->mins = {-16.0f, -16.0f, 0.0f};
	bounds->maxs = {16.0f, 16.0f, 72.0f};

	CCSPlayerPawn *pawn = this->GetPawn();
	if (pawn && pawn->m_pMovementServices() && static_cast<CCSPlayer_MovementServices *>(pawn->m_pMovementServices())->m_bDucked())
	{
		bounds->maxs.z = 54.0f;
	}
}

void ZEPlayer::RegisterLanding(const Vector &velocity)
{
	if (this->processingMovement && this->currentMoveData)
	{
		this->landingOrigin = this->currentMoveData->m_vecAbsOrigin;
	}
	else
	{
		this->GetOrigin(&this->landingOrigin);
	}

	this->landingVelocity = velocity;
}

void ZEPlayer::ApplySlopeFix()
{
	if (!this->processingMovement || !this->currentMoveData)
	{
		return;
	}

	CCSPlayerPawn *pawn = this->GetPawn();
	if (!pawn)
	{
		return;
	}

	bbox_t bounds;
	this->GetBBoxBounds(&bounds);

	CTraceFilterPlayerMovementCS filter(pawn);
	Vector ground = this->currentMoveData->m_vecAbsOrigin;
	ground.z -= 2.0f;
	trace_t trace;

	addresses::TracePlayerBBox(this->currentMoveData->m_vecAbsOrigin, ground, bounds, &filter, trace);

	if (trace.m_bStartInSolid || trace.m_flFraction == 1.0f || trace.m_vHitNormal.z < 0.7f || trace.m_vHitNormal.z >= 1.0f)
	{
		return;
	}

	Vector newVelocity;
	float backoff = DotProduct(this->landingVelocity, trace.m_vHitNormal);

	for (int i = 0; i < 3; i++)
	{
		float change = trace.m_vHitNormal[i] * backoff;
		newVelocity[i] = this->landingVelocity[i] - change;
	}

	float adjust = DotProduct(newVelocity, trace.m_vHitNormal);
	if (adjust < 0.0f)
	{
		newVelocity -= trace.m_vHitNormal * adjust;
	}

	if (newVelocity.Length2D() >= this->landingVelocity.Length2D())
	{
		this->currentMoveData->m_vecVelocity.x = newVelocity.x;
		this->currentMoveData->m_vecVelocity.y = newVelocity.y;
		this->landingVelocity.x = newVelocity.x;
		this->landingVelocity.y = newVelocity.y;
	}
}
