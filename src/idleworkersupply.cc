/*
 * Copyright (C) 2002-2004, 2006-2008 by the Widelands Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "game.h"
#include "idleworkersupply.h"
#include "player.h"
#include "request.h"
#include "requirements.h"
#include "soldier.h"
#include "tribe.h"
#include "worker.h"
#include "wexception.h"

namespace Widelands {

/**
 * Automatically register with the worker's economy.
 */
IdleWorkerSupply::IdleWorkerSupply(Worker* w): m_worker (w), m_economy(0)
{
	set_economy(w->get_economy());
}


/**
 * Automatically unregister from economy.
 */
IdleWorkerSupply::~IdleWorkerSupply()
{
	set_economy(0);
}


/**
 * Add/remove this supply from the Economy as appropriate.
 */
void IdleWorkerSupply::set_economy(Economy* e)
{
	if (m_economy == e)
		return;

	if (m_economy)
		m_economy->remove_supply(this);

	m_economy = e;

	if (m_economy)
		m_economy->add_supply(this);
}


/**
 * Return the worker's position.
 */
PlayerImmovable* IdleWorkerSupply::get_position(Game* g)
{
	return m_worker->get_location(g);
}


uint32_t IdleWorkerSupply::nr_supplies(Game*, const Request* req)
{
	if
		(req->get_type() == Request::WORKER &&
		 m_worker->descr().can_act_as(req->get_index()) &&
		 req->get_requirements().check(m_worker))
		return 1;

	return 0;
}

WareInstance & IdleWorkerSupply::launch_item(Game *, const Request*)
{
	throw wexception("IdleWorkerSupply::launch_item() makes no sense.");
}


/**
 * No need to explicitly launch the worker.
 */
Worker* IdleWorkerSupply::launch_worker(Game *, const Request* req)
{
	if (req->get_type() != Request::WORKER)
		throw wexception("IdleWorkerSupply: not a worker request");
	if
		(!m_worker->descr().can_act_as(req->get_index()) ||
		 !req->get_requirements().check(m_worker))
		throw wexception("IdleWorkerSupply: worker type mismatch");

	return m_worker;
}


};
