/*
 * Copyright (C) 2002-2019 by the Widelands Development Team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include "logic/map_objects/tribes/constructionsite.h"

#include <cstdio>

#include <boost/format.hpp>

#include "base/i18n.h"
#include "base/macros.h"
#include "base/wexception.h"
#include "economy/wares_queue.h"
#include "graphic/animation.h"
#include "graphic/graphic.h"
#include "graphic/rendertarget.h"
#include "logic/editor_game_base.h"
#include "logic/game.h"
#include "logic/game_data_error.h"
#include "logic/map_objects/tribes/militarysite.h"
#include "logic/map_objects/tribes/partially_finished_building.h"
#include "logic/map_objects/tribes/productionsite.h"
#include "logic/map_objects/tribes/trainingsite.h"
#include "logic/map_objects/tribes/tribe_descr.h"
#include "logic/map_objects/tribes/worker.h"
#include "logic/map_objects/world/world.h"
#include "sound/note_sound.h"
#include "sound/sound_handler.h"
#include "ui_basic/window.h"

namespace Widelands {

void ConstructionsiteInformation::draw(const Vector2f& point_on_dst,
                                       const Widelands::Coords& coords,
                                       float scale,
                                       const RGBColor& player_color,
                                       RenderTarget* dst) const {
	// Draw the construction site marker
	std::vector<std::pair<uint32_t, uint32_t>> animations;
	uint32_t total_frames = 0;
	auto push_animation = [](const BuildingDescr* d,
	                         std::vector<std::pair<uint32_t, uint32_t>>* anims, uint32_t* tf) {
		const bool known = d->is_animation_known("build");
		const uint32_t anim_idx =
		   known ? d->get_animation("build", nullptr) : d->get_unoccupied_animation();
		// If there is no build animation, we use only the first frame or we
		// would get many build steps with almost the same image...
		const uint32_t nrframes = known ? g_gr->animations().get_animation(anim_idx).nr_frames() : 1;
		assert(nrframes);
		*tf += nrframes;
		anims->push_back(std::make_pair(anim_idx, nrframes));
	};
	for (const BuildingDescr* d : intermediates) {
		push_animation(d, &animations, &total_frames);
	}
	push_animation(becomes, &animations, &total_frames);

	uint32_t frame_index =
	   totaltime ? std::min(completedtime * total_frames / totaltime, total_frames - 1) : 0;
	uint32_t animation_index = 0;
	while (frame_index >= animations[animation_index].second) {
		frame_index -= animations[animation_index].second;
		++animation_index;
		assert(animation_index < animations.size());
	}
	const uint32_t anim_time = frame_index * FRAME_LENGTH;

	if (frame_index > 0) {
		// Not the first pic within this animation – draw the previous one
		dst->blit_animation(point_on_dst, Widelands::Coords::null(), scale,
		                    animations[animation_index].first, anim_time - FRAME_LENGTH,
		                    &player_color);
	} else if (animation_index > 0) {
		// The first pic, but not the first series of animations – draw the last pic of the previous
		// series
		dst->blit_animation(
		   point_on_dst, Widelands::Coords::null(), scale, animations[animation_index - 1].first,
		   FRAME_LENGTH * (animations[animation_index - 1].second - 1), &player_color);
	} else if (was) {
		//  First pic in first series, but there was another building here before –
		//  get its most fitting picture and draw it instead
		const uint32_t unocc = was->get_unoccupied_animation();
		dst->blit_animation(point_on_dst, Widelands::Coords::null(), scale, unocc,
		                    FRAME_LENGTH * (g_gr->animations().get_animation(unocc).nr_frames() - 1),
		                    &player_color);
	}
	// Now blit a segment of the current construction phase from the bottom.
	int percent = 100 * completedtime * total_frames;
	if (totaltime) {
		percent /= totaltime;
	}
	percent -= 100 * frame_index;
	for (uint32_t i = 0; i < animation_index; ++i) {
		percent -= 100 * animations[i].second;
	}
	dst->blit_animation(point_on_dst, coords, scale, animations[animation_index].first, anim_time,
	                    &player_color, percent);
}

/**
 * The contents of 'table' are documented in
 * /data/tribes/buildings/partially_finished/constructionsite/init.lua
 */
ConstructionSiteDescr::ConstructionSiteDescr(const std::string& init_descname,
                                             const LuaTable& table,
                                             const Tribes& tribes)
   : BuildingDescr(init_descname, MapObjectType::CONSTRUCTIONSITE, table, tribes),
     creation_fx_(
        SoundHandler::register_fx(SoundType::kAmbient, "sound/create_construction_site")) {
	add_attribute(MapObject::CONSTRUCTIONSITE);
}

Building& ConstructionSiteDescr::create_object() const {
	return *new ConstructionSite(*this);
}

FxId ConstructionSiteDescr::creation_fx() const {
	return creation_fx_;
}

/*
==============================

IMPLEMENTATION

==============================
*/

ConstructionSite::ConstructionSite(const ConstructionSiteDescr& cs_descr)
   : PartiallyFinishedBuilding(cs_descr),
     fetchfromflag_(0),
     builder_idle_(false),
     settings_(nullptr) {
}

void ConstructionSite::update_statistics_string(std::string* s) {
	unsigned int percent = (get_built_per64k() * 100) >> 16;
	*s = g_gr->styles().color_tag((boost::format(_("%i%% built")) % percent).str(),
	                              g_gr->styles().building_statistics_style().construction_color());
}

/*
=======
Access to the wares queues by id
=======
*/
InputQueue& ConstructionSite::inputqueue(DescriptionIndex const wi, WareWorker const type) {
	// There are no worker queues here
	// Hopefully, our construction sites are safe enough not to kill workers
	if (type != wwWARE) {
		throw wexception("%s (%u) (building %s) has no WorkersQueues", descr().name().c_str(),
		                 serial(), building_->name().c_str());
	}
	for (WaresQueue* ware : wares_) {
		if (ware->get_index() == wi) {
			return *ware;
		}
	}
	throw wexception("%s (%u) (building %s) has no WaresQueue for %u", descr().name().c_str(),
	                 serial(), building_->name().c_str(), wi);
}

/*
===============
Set the type of building we're going to build
===============
*/
void ConstructionSite::set_building(const BuildingDescr& building_descr) {
	PartiallyFinishedBuilding::set_building(building_descr);

	info_.becomes = &building_descr;
}

/*
===============
Initialize the construction site by starting orders
===============
*/
bool ConstructionSite::init(EditorGameBase& egbase) {
	Notifications::publish(
	   NoteSound(SoundType::kAmbient, descr().creation_fx(), position_, kFxPriorityAlwaysPlay));
	PartiallyFinishedBuilding::init(egbase);

	const std::map<DescriptionIndex, uint8_t>* buildcost = nullptr;
	if (!old_buildings_.empty()) {
		// Enhancement and/or built over immovable
		for (auto it = old_buildings_.end(); it != old_buildings_.begin();) {
			--it;
			if (it->second.empty()) {
				const BuildingDescr* was_descr = owner().tribe().get_building_descr(it->first);
				info_.was = was_descr;
				buildcost = &building_->enhancement_cost();
				break;
			}
		}
	}
	if (!buildcost) {
		buildcost = &building_->buildcost();
	}
	assert(buildcost);

	//  TODO(unknown): figure out whether planing is necessary

	//  initialize the wares queues
	size_t const buildcost_size = buildcost->size();
	wares_.resize(buildcost_size);
	std::map<DescriptionIndex, uint8_t>::const_iterator it = buildcost->begin();

	for (size_t i = 0; i < buildcost_size; ++i, ++it) {
		WaresQueue& wq = *(wares_[i] = new WaresQueue(*this, it->first, it->second));

		wq.set_callback(ConstructionSite::wares_queue_callback, this);
		wq.set_consume_interval(CONSTRUCTIONSITE_STEP_TIME);

		work_steps_ += it->second;
	}

	init_settings();

	return true;
}

void ConstructionSite::init_settings() {
	assert(building_);
	assert(!settings_);
	if (upcast(const WarehouseDescr, wd, building_)) {
		settings_.reset(new WarehouseSettings(*wd, owner().tribe()));
	} else if (upcast(const TrainingSiteDescr, td, building_)) {
		settings_.reset(new TrainingsiteSettings(*td));
	} else if (upcast(const ProductionSiteDescr, pd, building_)) {
		settings_.reset(new ProductionsiteSettings(*pd));
	} else if (upcast(const MilitarySiteDescr, md, building_)) {
		settings_.reset(new MilitarysiteSettings(*md));
	} else {
		// TODO(Nordfriese): Add support for markets when trading is implemented
		log("WARNING: Created constructionsite for a %s, which is not of any known building type\n",
		    building_->name().c_str());
	}
}

/*
===============
Release worker and material (if any is left).
If construction was finished successfully, place the building at our position.
===============
*/
void ConstructionSite::cleanup(EditorGameBase& egbase) {
	if (work_steps_ <= work_completed_) {
		// If the building is finished, register whether the window was open
		Notifications::publish(NoteBuilding(serial(), NoteBuilding::Action::kStartWarp));
	}

	PartiallyFinishedBuilding::cleanup(egbase);

	if (work_steps_ <= work_completed_) {
		// Put the real building in place
		DescriptionIndex becomes_idx = owner().tribe().building_index(building_->name());
		old_buildings_.push_back(std::make_pair(becomes_idx, ""));
		Building& b = building_->create(egbase, get_owner(), position_, false, false, old_buildings_);
		if (Worker* const builder = builder_.get(egbase)) {
			builder->reset_tasks(dynamic_cast<Game&>(egbase));
			builder->set_location(&b);
		}

		// Apply settings
		if (settings_) {
			if (upcast(ProductionsiteSettings, ps, settings_.get())) {
				for (const auto& pair : ps->ware_queues) {
					b.inputqueue(pair.first, wwWARE).set_max_fill(pair.second.desired_fill);
					b.set_priority(wwWARE, pair.first, pair.second.priority);
				}
				for (const auto& pair : ps->worker_queues) {
					b.inputqueue(pair.first, wwWORKER).set_max_fill(pair.second.desired_fill);
					b.set_priority(wwWORKER, pair.first, pair.second.priority);
				}
				if (upcast(TrainingsiteSettings, ts, ps)) {
					assert(b.soldier_control());
					assert(ts->desired_capacity >= b.soldier_control()->min_soldier_capacity());
					assert(ts->desired_capacity <= b.soldier_control()->max_soldier_capacity());
					if (ts->desired_capacity != b.soldier_control()->soldier_capacity()) {
						b.mutable_soldier_control()->set_soldier_capacity(ts->desired_capacity);
					}
				}
				dynamic_cast<ProductionSite&>(b).set_stopped(ps->stopped);
			} else if (upcast(MilitarysiteSettings, ms, settings_.get())) {
				assert(b.soldier_control());
				assert(ms->desired_capacity >= b.soldier_control()->min_soldier_capacity());
				assert(ms->desired_capacity <= b.soldier_control()->max_soldier_capacity());
				if (ms->desired_capacity != b.soldier_control()->soldier_capacity()) {
					b.mutable_soldier_control()->set_soldier_capacity(ms->desired_capacity);
				}
				dynamic_cast<MilitarySite&>(b).set_soldier_preference(
				   ms->prefer_heroes ? SoldierPreference::kHeroes : SoldierPreference::kRookies);
			} else if (upcast(WarehouseSettings, ws, settings_.get())) {
				Warehouse& site = dynamic_cast<Warehouse&>(b);
				for (const auto& pair : ws->ware_preferences) {
					site.set_ware_policy(pair.first, pair.second);
				}
				for (const auto& pair : ws->worker_preferences) {
					site.set_worker_policy(pair.first, pair.second);
				}
				if (ws->launch_expedition) {
					get_owner()->start_or_cancel_expedition(site);
				}
			} else {
				NEVER_HERE();
			}
		}

		// Open the new building window if needed
		Notifications::publish(NoteBuilding(b.serial(), NoteBuilding::Action::kFinishWarp));
	}
}

/*
===============
Start building the next enhancement even before the base building is completed.
===============
*/
void ConstructionSite::enhance(Game&) {
	assert(building_->enhancement() != INVALID_INDEX);
	Notifications::publish(NoteImmovable(this, NoteImmovable::Ownership::LOST));

	info_.intermediates.push_back(building_);
	old_buildings_.push_back(std::make_pair(owner().tribe().building_index(building_->name()), ""));
	building_ = owner().tribe().get_building_descr(building_->enhancement());
	assert(building_);
	info_.becomes = building_;

	const std::map<DescriptionIndex, uint8_t>& buildcost = building_->enhancement_cost();
	std::set<DescriptionIndex> new_ware_types;
	for (const auto& pair : buildcost) {
		bool found = false;
		for (const auto& queue : wares_) {
			if (queue->get_index() == pair.first) {
				found = true;
				break;
			}
		}
		if (!found) {
			new_ware_types.insert(pair.first);
		}
	}

	const size_t old_size = wares_.size();
	wares_.resize(old_size + new_ware_types.size());

	size_t new_index = 0;
	for (const auto& pair : buildcost) {
		if (new_ware_types.count(pair.first)) {
			WaresQueue& wq =
			   *(wares_[old_size + new_index] = new WaresQueue(*this, pair.first, pair.second));
			wq.set_callback(ConstructionSite::wares_queue_callback, this);
			wq.set_consume_interval(CONSTRUCTIONSITE_STEP_TIME);
			++new_index;
		} else {
			for (size_t i = 0; i < old_size; ++i) {
				WaresQueue& wq = *wares_[i];
				if (wq.get_index() == pair.first) {
					wq.set_max_size(wq.get_max_size() + pair.second);
					wq.set_max_fill(wq.get_max_fill() + pair.second);
					break;
				}
			}
		}
		work_steps_ += pair.second;
	}

	BuildingSettings* old_settings = settings_.release();
	if (upcast(const WarehouseDescr, wd, building_)) {
		upcast(WarehouseSettings, ws, old_settings);
		assert(ws);
		WarehouseSettings* new_settings = new WarehouseSettings(*wd, owner().tribe());
		settings_.reset(new_settings);
		for (const auto& pair : ws->ware_preferences) {
			new_settings->ware_preferences[pair.first] = pair.second;
		}
		for (const auto& pair : ws->worker_preferences) {
			new_settings->worker_preferences[pair.first] = pair.second;
		}
		new_settings->launch_expedition = ws->launch_expedition && building_->get_isport();
	} else if (upcast(const TrainingSiteDescr, td, building_)) {
		upcast(TrainingsiteSettings, ts, old_settings);
		assert(ts);
		TrainingsiteSettings* new_settings = new TrainingsiteSettings(*td);
		settings_.reset(new_settings);
		new_settings->stopped = ts->stopped;
		for (const auto& pair_old : ts->ware_queues) {
			for (auto& pair_new : new_settings->ware_queues) {
				if (pair_new.first == pair_old.first) {
					pair_new.second.priority = pair_old.second.priority;
					pair_new.second.desired_fill =
					   std::min(pair_old.second.desired_fill, pair_new.second.max_fill);
					break;
				}
			}
		}
		for (const auto& pair_old : ts->worker_queues) {
			for (auto& pair_new : new_settings->worker_queues) {
				if (pair_new.first == pair_old.first) {
					pair_new.second.priority = pair_old.second.priority;
					pair_new.second.desired_fill =
					   std::min(pair_old.second.desired_fill, pair_new.second.max_fill);
					break;
				}
			}
		}
		new_settings->desired_capacity = std::min(new_settings->max_capacity, ts->desired_capacity);
	} else if (upcast(const ProductionSiteDescr, pd, building_)) {
		upcast(ProductionsiteSettings, ps, old_settings);
		assert(ps);
		ProductionsiteSettings* new_settings = new ProductionsiteSettings(*pd);
		settings_.reset(new_settings);
		new_settings->stopped = ps->stopped;
		for (const auto& pair_old : ps->ware_queues) {
			for (auto& pair_new : new_settings->ware_queues) {
				if (pair_new.first == pair_old.first) {
					pair_new.second.priority = pair_old.second.priority;
					pair_new.second.desired_fill =
					   std::min(pair_old.second.desired_fill, pair_new.second.max_fill);
					break;
				}
			}
		}
		for (const auto& pair_old : ps->worker_queues) {
			for (auto& pair_new : new_settings->worker_queues) {
				if (pair_new.first == pair_old.first) {
					pair_new.second.priority = pair_old.second.priority;
					pair_new.second.desired_fill =
					   std::min(pair_old.second.desired_fill, pair_new.second.max_fill);
					break;
				}
			}
		}
	} else if (upcast(const MilitarySiteDescr, md, building_)) {
		upcast(MilitarysiteSettings, ms, old_settings);
		assert(ms);
		MilitarysiteSettings* new_settings = new MilitarysiteSettings(*md);
		settings_.reset(new_settings);
		new_settings->desired_capacity = std::max<uint32_t>(
		   1, std::min<uint32_t>(new_settings->max_capacity, ms->desired_capacity));
		new_settings->prefer_heroes = ms->prefer_heroes;
	} else {
		// TODO(Nordfriese): Add support for markets when trading is implemented
		log("WARNING: Enhanced constructionsite to a %s, which is not of any known building type\n",
		    building_->name().c_str());
	}
	Notifications::publish(NoteImmovable(this, NoteImmovable::Ownership::GAINED));
	Notifications::publish(NoteBuilding(serial(), NoteBuilding::Action::kChanged));
}

/*
===============
Construction sites only burn if some of the work has been completed.
===============
*/
bool ConstructionSite::burn_on_destroy() {
	if (work_completed_ >= work_steps_)
		return false;  // completed, so don't burn

	return work_completed_ || !old_buildings_.empty();
}

/*
===============
Remember the ware on the flag. The worker will be sent from get_building_work().
===============
*/
bool ConstructionSite::fetch_from_flag(Game& game) {
	++fetchfromflag_;

	if (Worker* const builder = builder_.get(game))
		builder->update_task_buildingwork(game);

	return true;
}

/*
===============
Called by our builder to get instructions.
===============
*/
bool ConstructionSite::get_building_work(Game& game, Worker& worker, bool) {
	if (&worker != builder_.get(game)) {
		// Not our construction worker; e.g. a miner leaving a mine
		// that is supposed to be enhanced. Make him return to a warehouse
		worker.pop_task(game);
		worker.start_task_leavebuilding(game, true);
		return true;
	}

	if (!work_steps_)           //  Happens for building without buildcost.
		schedule_destroy(game);  //  Complete the building immediately.

	// Check if one step has completed
	if (working_) {
		if (static_cast<int32_t>(game.get_gametime() - work_steptime_) < 0) {
			worker.start_task_idle(game, worker.descr().get_animation("work", &worker),
			                       work_steptime_ - game.get_gametime());
			builder_idle_ = false;
			return true;
		} else {
			// TODO(fweber): cause "construction sounds" to be played -
			// perhaps dependent on kind of construction?

			++work_completed_;
			if (work_completed_ >= work_steps_)
				schedule_destroy(game);

			working_ = false;
		}
	}

	// Fetch wares from flag
	if (fetchfromflag_) {
		--fetchfromflag_;
		builder_idle_ = false;
		worker.start_task_fetchfromflag(game);
		return true;
	}

	// Drop all the wares that are too much out to the flag.
	for (WaresQueue* iqueue : wares_) {
		WaresQueue* queue = iqueue;
		if (queue->get_filled() > queue->get_max_fill()) {
			queue->set_filled(queue->get_filled() - 1);
			const WareDescr& wd = *owner().tribe().get_ware_descr(queue->get_index());
			WareInstance& ware = *new WareInstance(queue->get_index(), &wd);
			ware.init(game);
			worker.start_task_dropoff(game, ware);
			return true;
		}
	}

	// Check if we've got wares to consume
	if (work_completed_ < work_steps_) {
		for (uint32_t i = 0; i < wares_.size(); ++i) {
			WaresQueue& wq = *wares_[i];

			if (!wq.get_filled())
				continue;

			wq.set_filled(wq.get_filled() - 1);
			wq.set_max_size(wq.get_max_size() - 1);

			// Update consumption statistic
			get_owner()->ware_consumed(wq.get_index(), 1);

			working_ = true;
			work_steptime_ = game.get_gametime() + CONSTRUCTIONSITE_STEP_TIME;

			worker.start_task_idle(
			   game, worker.descr().get_animation("work", &worker), CONSTRUCTIONSITE_STEP_TIME);
			builder_idle_ = false;
			return true;
		}
	}
	// The only work we have got for you, is to run around to look cute ;)
	if (!builder_idle_) {
		worker.set_animation(game, worker.descr().get_animation("idle", &worker));
		builder_idle_ = true;
	}
	worker.schedule_act(game, 2000);
	return true;
}

/*
===============
Called by InputQueue code when an ware has arrived
===============
*/
void ConstructionSite::wares_queue_callback(
   Game& game, InputQueue*, DescriptionIndex, Worker*, void* const data) {
	ConstructionSite& cs = *static_cast<ConstructionSite*>(data);

	if (!cs.working_)
		if (Worker* const builder = cs.builder_.get(game))
			builder->update_task_buildingwork(game);
}

/*
===============
Overwrite as many values of the current settings with those of the given settings as possible.
===============
*/
void ConstructionSite::apply_settings(const BuildingSettings& cs) {
	assert(settings_);
	settings_->apply(cs);
	delete &cs;
}

/*
===============
Draw the construction site.
===============
*/
void ConstructionSite::draw(uint32_t gametime,
                            TextToDraw draw_text,
                            const Vector2f& point_on_dst,
                            const Widelands::Coords& coords,
                            float scale,
                            RenderTarget* dst) {
	uint32_t tanim = gametime - animstart_;
	const RGBColor& player_color = get_owner()->get_playercolor();
	if (was_immovable_) {
		dst->blit_animation(
		   point_on_dst, coords, scale, was_immovable_->main_animation(), tanim, &player_color);
	} else {
		// Draw the construction site marker
		dst->blit_animation(
		   point_on_dst, Widelands::Coords::null(), scale, anim_, tanim, &player_color);
	}

	// Draw the partially finished building

	static_assert(
	   0 <= CONSTRUCTIONSITE_STEP_TIME, "assert(0 <= CONSTRUCTIONSITE_STEP_TIME) failed.");
	info_.totaltime = CONSTRUCTIONSITE_STEP_TIME * work_steps_;
	info_.completedtime = CONSTRUCTIONSITE_STEP_TIME * work_completed_;

	if (working_) {
		assert(work_steptime_ <= info_.completedtime + CONSTRUCTIONSITE_STEP_TIME + gametime);
		info_.completedtime += CONSTRUCTIONSITE_STEP_TIME + gametime - work_steptime_;
	}

	info_.draw(point_on_dst, coords, scale, player_color, dst);

	// Draw help strings
	draw_info(draw_text, point_on_dst, scale, dst);
}
}  // namespace Widelands
