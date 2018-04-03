/*
 * Copyright (C) 2017 by the Widelands Development Team
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

#include "graphic/style_manager.h"

#include <memory>

#include <boost/format.hpp>

#include "base/wexception.h"
#include "graphic/graphic.h"
#include "scripting/lua_interface.h"

namespace {
// Read RGB color from LuaTable
RGBColor read_rgb_color(const LuaTable& table) {
	std::vector<int> rgbcolor = table.array_entries<int>();
	if (rgbcolor.size() != 3) {
		throw wexception("Expected 3 entries for RGB color, but got %" PRIuS ".", rgbcolor.size());
	}
	return RGBColor(rgbcolor[0], rgbcolor[1], rgbcolor[2]);
}

UI::FontStyleInfo read_font_style(const LuaTable& parent_table, const std::string& table_key) {
	std::unique_ptr<LuaTable> style_table = parent_table.get_table(table_key);
	UI::FontStyleInfo font(style_table->get_string("face"), read_rgb_color(*style_table->get_table("color")), style_table->get_int("size"));
	if (font.size < 1) {
		throw wexception("Font size too small for %s, must be at least 1!", table_key.c_str());
	}
	if (style_table->has_key("bold")) {
		font.bold = style_table->get_bool("bold");
	}
	if (style_table->has_key("italic")) {
		font.italic = style_table->get_bool("italic");
	}
	if (style_table->has_key("shadow")) {
		font.shadow = style_table->get_bool("shadow");
	}
	if (style_table->has_key("underline")) {
		font.underline = style_table->get_bool("underline");
	}
	return font;
}
// NOCOM code duplication to avoid memory leaks. Meh.
UI::FontStyleInfo* read_font_style_to_pointer(const LuaTable& parent_table, const std::string& table_key) {
	std::unique_ptr<LuaTable> style_table = parent_table.get_table(table_key);
	UI::FontStyleInfo* font = new UI::FontStyleInfo(style_table->get_string("face"), read_rgb_color(*style_table->get_table("color")), style_table->get_int("size"));
	if (font->size < 1) {
		throw wexception("Font size too small for %s, must be at least 1!", table_key.c_str());
	}
	if (style_table->has_key("bold")) {
		font->bold = style_table->get_bool("bold");
	}
	if (style_table->has_key("italic")) {
		font->italic = style_table->get_bool("italic");
	}
	if (style_table->has_key("shadow")) {
		font->shadow = style_table->get_bool("shadow");
	}
	if (style_table->has_key("underline")) {
		font->underline = style_table->get_bool("underline");
	}
	return font;
}

// Read image filename and RGBA color from LuaTable
UI::PanelStyleInfo* read_style(const LuaTable& table, const std::string& keyname = "", size_t expected_fonts = 0) {
	const std::string image = table.get_string("image");
	std::vector<int> rgbcolor = table.get_table("color")->array_entries<int>();
	if (rgbcolor.size() != 3) {
		throw wexception("Expected 3 entries for RGB color, but got %" PRIuS ".", rgbcolor.size());
	}
	UI::PanelStyleInfo* result = new UI::PanelStyleInfo(image.empty() ? nullptr : g_gr->images().get(image),
	                              RGBAColor(rgbcolor[0], rgbcolor[1], rgbcolor[2], 0));

	if (table.has_key("fonts")) {
		std::unique_ptr<LuaTable> fonts_table = table.get_table("fonts");
		for (const std::string& key : fonts_table->keys<std::string>()) {
			result->fonts.emplace(std::make_pair(key, std::unique_ptr<UI::FontStyleInfo>(read_font_style_to_pointer(*fonts_table, key))));
		}
	}

	if (result->fonts.size() != expected_fonts) {
		throw wexception("StyleManager: There is a definition missing for a style's fonts: %s", keyname.c_str());
	}

	return result;
}

// Stupid completeness check - enum classes weren't meant for iterating, so we just compare the size
// to the last enum member. This assumes that there are no holes in the enum, and will need
// adjusting if the last enum member changes.
void check_completeness(const std::string& name, size_t map_size, size_t last_enum_member) {
	if (map_size != last_enum_member + 1) {
		throw wexception("StyleManager: There is a definition missing for the '%s'.", name.c_str());
	}
}
}  // namespace

void StyleManager::init() {
	buttonstyles_.clear();
	sliderstyles_.clear();
	tabpanelstyles_.clear();
	editboxstyles_.clear();
	dropdownstyles_.clear();
	scrollbarstyles_.clear();

	LuaInterface lua;
	std::unique_ptr<LuaTable> table(lua.run_script(kTemplateDir + "init.lua"));

	// Buttons
	std::unique_ptr<LuaTable> element_table = table->get_table("buttons");
	std::unique_ptr<LuaTable> style_table = element_table->get_table("fsmenu");
	add_button_style(UI::ButtonStyle::kFsMenuMenu, *style_table, "menu");
	add_button_style(UI::ButtonStyle::kFsMenuPrimary, *style_table, "primary");
	add_button_style(UI::ButtonStyle::kFsMenuSecondary, *style_table, "secondary");
	style_table = element_table->get_table("wui");
	add_button_style(UI::ButtonStyle::kWuiMenu, *style_table, "menu");
	add_button_style(UI::ButtonStyle::kWuiPrimary, *style_table, "primary");
	add_button_style(UI::ButtonStyle::kWuiSecondary, *style_table, "secondary");
	add_button_style(
	   UI::ButtonStyle::kWuiBuildingStats, *style_table, "building_stats");
	check_completeness(
	   "buttons", buttonstyles_.size(), static_cast<size_t>(UI::ButtonStyle::kWuiBuildingStats));

	// Sliders
	element_table = table->get_table("sliders");
	style_table = element_table->get_table("fsmenu");
	add_slider_style(UI::SliderStyle::kFsMenu, *style_table, "menu");
	style_table = element_table->get_table("wui");
	add_slider_style(UI::SliderStyle::kWuiLight, *style_table, "light");
	add_slider_style(UI::SliderStyle::kWuiDark, *style_table, "dark");
	check_completeness(
	   "sliders", sliderstyles_.size(), static_cast<size_t>(UI::SliderStyle::kWuiDark));

	// Tabpanels
	element_table = table->get_table("tabpanels");
	style_table = element_table->get_table("fsmenu");
	add_tabpanel_style(UI::TabPanelStyle::kFsMenu, *style_table->get_table("menu").get());
	style_table = element_table->get_table("wui");
	add_tabpanel_style(UI::TabPanelStyle::kWuiLight, *style_table->get_table("light").get());
	add_tabpanel_style(UI::TabPanelStyle::kWuiDark, *style_table->get_table("dark").get());
	check_completeness(
	   "tabpanels", tabpanelstyles_.size(), static_cast<size_t>(UI::TabPanelStyle::kWuiDark));

	// Editboxes
	element_table = table->get_table("editboxes");
	style_table = element_table->get_table("fsmenu");
	add_editbox_style(UI::PanelStyle::kFsMenu, *style_table, "menu");
	style_table = element_table->get_table("wui");
	add_editbox_style(UI::PanelStyle::kWui, *style_table, "menu");
	check_completeness(
	   "editboxes", editboxstyles_.size(), static_cast<size_t>(UI::PanelStyle::kWui));

	// Dropdowns
	element_table = table->get_table("dropdowns");
	style_table = element_table->get_table("fsmenu");
	add_style(UI::PanelStyle::kFsMenu, *style_table->get_table("menu").get(), &dropdownstyles_);
	style_table = element_table->get_table("wui");
	add_style(UI::PanelStyle::kWui, *style_table->get_table("menu").get(), &dropdownstyles_);
	check_completeness(
	   "dropdowns", dropdownstyles_.size(), static_cast<size_t>(UI::PanelStyle::kWui));

	// Scrollbars
	element_table = table->get_table("scrollbars");
	style_table = element_table->get_table("fsmenu");
	add_style(UI::PanelStyle::kFsMenu, *style_table->get_table("menu").get(), &scrollbarstyles_);
	style_table = element_table->get_table("wui");
	add_style(UI::PanelStyle::kWui, *style_table->get_table("menu").get(), &scrollbarstyles_);
	check_completeness(
	   "scrollbars", scrollbarstyles_.size(), static_cast<size_t>(UI::PanelStyle::kWui));

	// Building statistics etc. for map objects
	UI::MapObjectStyleInfo* map_object_info = new UI::MapObjectStyleInfo();
	// Fonts
	element_table = table->get_table("map_object");
	map_object_info->building_statistics_font = read_font_style(*element_table, "building_statistics_font");
	map_object_info->census_font = read_font_style(*element_table, "census_font");
	map_object_info->statistics_font = read_font_style(*element_table, "statistics_font");

	// Colors
	style_table = element_table->get_table("colors");
	map_object_info->construction_color = read_rgb_color(*style_table->get_table("construction"));
	map_object_info->neutral_color = read_rgb_color(*style_table->get_table("neutral"));
	map_object_info->low_color = read_rgb_color(*style_table->get_table("low"));
	map_object_info->medium_color = read_rgb_color(*style_table->get_table("medium"));
	map_object_info->high_color = read_rgb_color(*style_table->get_table("high"));
	map_object_style_.reset(std::move(map_object_info));

	// Progress bars
	element_table = table->get_table("progressbar");
	add_progressbar_style(UI::PanelStyle::kFsMenu, *element_table->get_table("fsmenu"));
	add_progressbar_style(UI::PanelStyle::kWui, *element_table->get_table("wui"));
	check_completeness(
	   "progressbars", progressbar_styles_.size(), static_cast<size_t>(UI::PanelStyle::kWui));

	// Table and listselect
	element_table = table->get_table("tables");
	add_table_style(UI::PanelStyle::kFsMenu, *element_table->get_table("fsmenu"));
	add_table_style(UI::PanelStyle::kWui, *element_table->get_table("wui"));
	check_completeness(
	   "tables", table_styles_.size(), static_cast<size_t>(UI::PanelStyle::kWui));

	// Statistics plot
	UI::StatisticsPlotStyleInfo* statistics_plot_info = new UI::StatisticsPlotStyleInfo();
	element_table = table->get_table("statistics_plot");
	// Fonts
	style_table = element_table->get_table("fonts");
	statistics_plot_info->x_tick_font = read_font_style(*style_table, "x_tick");
	statistics_plot_info->y_min_value_font = read_font_style(*style_table, "y_min_value");
	statistics_plot_info->y_max_value_font = read_font_style(*style_table, "y_max_value");
	style_table = element_table->get_table("colors");
	// Line colors
	statistics_plot_info->axis_line_color = read_rgb_color(*style_table->get_table("axis_line"));
	statistics_plot_info->zero_line_color = read_rgb_color(*style_table->get_table("zero_line"));
	statistics_plot_style_.reset(std::move(statistics_plot_info));

	// Special elements
	minimum_font_size_ = table->get_int("minimum_font_size");
	if (minimum_font_size_ < 1) {
		throw wexception("Font size too small for minimum_font_size, must be at least 1!");
	}
	minimap_icon_frame_ = read_rgb_color(*table->get_table("minimap_icon_frame"));

	// Fonts
	element_table = table->get_table("fonts");
	add_font_style(UI::FontStyle::kChatMessage, *element_table, "chat_message");
   add_font_style(UI::FontStyle::kChatPlayername, *element_table, "chat_playername");
   add_font_style(UI::FontStyle::kChatServer, *element_table, "chat_server");
   add_font_style(UI::FontStyle::kChatTimestamp, *element_table, "chat_timestamp");
   add_font_style(UI::FontStyle::kChatWhisper, *element_table, "chat_whisper");
   add_font_style(UI::FontStyle::kFsGameSetupHeadings, *element_table, "fsmenu_game_setup_headings");
   add_font_style(UI::FontStyle::kFsGameSetupIrcClient, *element_table, "fsmenu_game_setup_irc_client");
   add_font_style(UI::FontStyle::kFsGameSetupMapname, *element_table, "fsmenu_game_setup_mapname");
   add_font_style(UI::FontStyle::kFsMenuGameTip, *element_table, "fsmenu_gametip");
   add_font_style(UI::FontStyle::kFsMenuInfoPanelHeading, *element_table, "fsmenu_info_panel_heading");
   add_font_style(UI::FontStyle::kFsMenuInfoPanelParagraph, *element_table, "fsmenu_info_panel_paragraph");
   add_font_style(UI::FontStyle::kFsMenuIntro, *element_table, "fsmenu_intro");
	add_font_style(UI::FontStyle::kFsMenuTitle, *element_table, "fsmenu_title");
   add_font_style(UI::FontStyle::kLabel, *element_table, "label");
   add_font_style(UI::FontStyle::kTooltip, *element_table, "tooltip");
   add_font_style(UI::FontStyle::kWarning, *element_table, "warning");
   add_font_style(UI::FontStyle::kWuiGameSpeedAndCoordinates, *element_table, "wui_game_speed_and_coordinates");
   add_font_style(UI::FontStyle::kWuiInfoPanelHeading, *element_table, "wui_info_panel_heading");
   add_font_style(UI::FontStyle::kWuiInfoPanelParagraph, *element_table, "wui_info_panel_paragraph");
   add_font_style(UI::FontStyle::kWuiMessageHeading, *element_table, "wui_message_heading");
   add_font_style(UI::FontStyle::kWuiMessageParagraph, *element_table, "wui_message_paragraph");
   add_font_style(UI::FontStyle::kWuiWaresInfo, *element_table, "wui_waresinfo");
   add_font_style(UI::FontStyle::kWuiWindowTitle, *element_table, "wui_window_title");
	check_completeness("fonts", fontstyles_.size(), static_cast<size_t>(UI::FontStyle::kWuiWindowTitle));
}

// Return functions for the styles
const UI::PanelStyleInfo* StyleManager::button_style(UI::ButtonStyle style) const {
	assert(buttonstyles_.count(style) == 1);
	return buttonstyles_.at(style).get();
}

const UI::PanelStyleInfo* StyleManager::slider_style(UI::SliderStyle style) const {
	assert(sliderstyles_.count(style) == 1);
	return sliderstyles_.at(style).get();
}

const UI::PanelStyleInfo* StyleManager::tabpanel_style(UI::TabPanelStyle style) const {
	assert(tabpanelstyles_.count(style) == 1);
	return tabpanelstyles_.at(style).get();
}

const UI::PanelStyleInfo* StyleManager::editbox_style(UI::PanelStyle style) const {
	assert(editboxstyles_.count(style) == 1);
	return editboxstyles_.at(style).get();
}

const UI::PanelStyleInfo* StyleManager::dropdown_style(UI::PanelStyle style) const {
	assert(dropdownstyles_.count(style) == 1);
	return dropdownstyles_.at(style).get();
}

const UI::PanelStyleInfo* StyleManager::scrollbar_style(UI::PanelStyle style) const {
	assert(scrollbarstyles_.count(style) == 1);
	return scrollbarstyles_.at(style).get();
}

const UI::MapObjectStyleInfo& StyleManager::map_object_style() const {
	return *map_object_style_;
}

const UI::ProgressbarStyleInfo& StyleManager::progressbar_style(UI::PanelStyle style) const {
	assert(progressbar_styles_.count(style) == 1);
	return *progressbar_styles_.at(style);
}

const UI::StatisticsPlotStyleInfo& StyleManager::statistics_plot_style() const {
	return *statistics_plot_style_;
}

const UI::TableStyleInfo& StyleManager::table_style(UI::PanelStyle style) const {
	assert(table_styles_.count(style) == 1);
	return *table_styles_.at(style);
}

const UI::FontStyleInfo& StyleManager::font_style(UI::FontStyle style) const {
	return *fontstyles_.at(style);
}

int StyleManager::minimum_font_size() const {
	return minimum_font_size_;
}

const RGBColor& StyleManager::minimap_icon_frame() const {
	return minimap_icon_frame_;
}

std::string StyleManager::color_tag(const std::string& text, const RGBColor& color) {
	static boost::format f("<font color=%s>%s</font>");
	f % color.hex_value();
	f % text;
	return f.str();
}

// Fill the maps
void StyleManager::add_button_style(UI::ButtonStyle style, const LuaTable& table, const std::string& key) {
	buttonstyles_.insert(
	   std::make_pair(style, std::unique_ptr<UI::PanelStyleInfo>(read_style(*table.get_table(key), key, 2))));
	if (buttonstyles_.at(style)->fonts.count("enabled") == 0) {
		throw wexception("Missing 'enabled' font style for button '%s'", key.c_str());
	}
	if (buttonstyles_.at(style)->fonts.count("disabled") == 0) {
		throw wexception("Missing disabled' font style for button '%s'", key.c_str());
	}
}

void StyleManager::add_slider_style(UI::SliderStyle style, const LuaTable& table, const std::string& key) {
	sliderstyles_.insert(
	   std::make_pair(style, std::unique_ptr<UI::PanelStyleInfo>(read_style(*table.get_table(key), key, 1))));
	if (sliderstyles_.at(style)->fonts.count("labels") == 0) {
		throw wexception("Missing 'labels' font style for slider '%s'", key.c_str());
	}
}

void StyleManager::add_editbox_style(UI::PanelStyle style, const LuaTable& table, const std::string& key) {
	editboxstyles_.insert(
	   std::make_pair(style, std::unique_ptr<UI::PanelStyleInfo>(read_style(*table.get_table(key), key, 1))));
	if (editboxstyles_.at(style)->fonts.count("default") == 0) {
		throw wexception("Missing 'default' font style for editbox '%s'", key.c_str());
	}
}


void StyleManager::add_tabpanel_style(UI::TabPanelStyle style, const LuaTable& table) {
	tabpanelstyles_.insert(
	   std::make_pair(style, std::unique_ptr<UI::PanelStyleInfo>(read_style(table))));
}

void StyleManager::add_progressbar_style(UI::PanelStyle style, const LuaTable& table) {
	UI::ProgressbarStyleInfo* progress_bar_style = new UI::ProgressbarStyleInfo();
	progress_bar_style->font = read_font_style(table, "font");
	std::unique_ptr<LuaTable> color_table = table.get_table("background_colors");
	progress_bar_style->low_color = read_rgb_color(*color_table->get_table("low"));
	progress_bar_style->medium_color = read_rgb_color(*color_table->get_table("medium"));
	progress_bar_style->high_color = read_rgb_color(*color_table->get_table("high"));
	progressbar_styles_.insert(std::make_pair(style, std::unique_ptr<const UI::ProgressbarStyleInfo>(std::move(progress_bar_style))));

}

void StyleManager::add_table_style(UI::PanelStyle style, const LuaTable& table) {
	UI::TableStyleInfo* table_style = new UI::TableStyleInfo();
	table_style->enabled = read_font_style(table, "enabled");
	table_style->disabled = read_font_style(table, "disabled");
	table_styles_.insert(std::make_pair(style, std::unique_ptr<const UI::TableStyleInfo>(std::move(table_style))));
}

void StyleManager::add_style(UI::PanelStyle style, const LuaTable& table, PanelStyleMap* map) {
	map->insert(std::make_pair(style, std::unique_ptr<UI::PanelStyleInfo>(read_style(table))));
}

void StyleManager::add_font_style(UI::FontStyle font_key, const LuaTable& table, const std::string& table_key) {
	fontstyles_.emplace(std::make_pair(font_key, std::unique_ptr<UI::FontStyleInfo>(read_font_style_to_pointer(table, table_key))));
}
