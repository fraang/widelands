dirname = path.dirname (__file__)

tribes:new_trainingsite_type {
   msgctxt = "frisians_building",
   name = "frisians_training_arena",
   -- TRANSLATORS: This is a building name used in lists of buildings
   descname = pgettext ("frisians_building", "Training Arena"),
   helptext_script = dirname .. "helptexts.lua",
   icon = dirname .. "menu.png",
   size = "big",

   buildcost = {
      brick = 6,
      granite = 3,
      log = 4,
      gold = 3,
      reed = 4
   },
   return_on_dismantle = {
      brick = 3,
      granite = 2,
      log = 2,
      gold = 1,
      reed = 1
   },

   animations = {
      idle = {
         pictures = path.list_files (dirname .. "idle_??.png"),
         hotspot = {114, 124},
         fps = 10,
      },
      working = {
         pictures = path.list_files (dirname .. "working_??.png"),
         hotspot = {114, 124},
         fps = 10,
      },
      unoccupied = {
         pictures = path.list_files (dirname .. "unoccupied_?.png"),
         hotspot = {114, 98},
      },
   },

   aihints = {
      trainingsites_max_percent = 40,
      prohibited_till = 1500,
      very_weak_ai_limit = 0,
      weak_ai_limit = 1
   },

   working_positions = {
      frisians_trainer = 1
   },

   inputs = {
      { name = "smoked_fish", amount = 6 },
      { name = "smoked_meat", amount = 6 },
      { name = "mead", amount = 6 },
      { name = "honey_bread", amount = 6 },
      { name = "sword_long", amount = 3 },
      { name = "sword_broad", amount = 3 },
      { name = "sword_double", amount = 3 },
      { name = "helmet_golden", amount = 2 },
      { name = "fur_garment_golden", amount = 2 },
   },
   outputs = {
      "frisians_soldier",
      "scrap_iron",
      "scrap_metal_mixed",
      "fur_garment_old",
   },

   ["soldier attack"] = {
      min_level = 3,
      max_level = 5,
      food = {
         {"smoked_fish", "smoked_meat"},
         {"mead"},
         {"honey_bread"}
      },
      weapons = {
         "sword_long",
         "sword_broad",
         "sword_double",
      }
   },
   ["soldier health"] = {
      min_level = 1,
      max_level = 1,
      food = {
         {"smoked_fish", "smoked_meat"},
         {"honey_bread", "mead"}
      },
      weapons = {
         "helmet_golden",
      }
   },
   ["soldier defense"] = {
      min_level = 1,
      max_level = 1,
      food = {
         {"smoked_fish", "smoked_meat"},
         {"honey_bread", "mead"}
      },
      weapons = {
         "fur_garment_golden",
      }
   },

   programs = {
      sleep = {
         -- TRANSLATORS: Completed/Skipped/Did not start sleeping because ...
         descname = _"sleeping",
         actions = {
            "sleep=5000",
            "return=no_stats",
         }
      },
      upgrade_soldier_attack_3 = {
         -- TRANSLATORS: Completed/Skipped/Did not start upgrading ... because ...
         descname = pgettext ("frisians_building", "upgrading soldier attack from level 3 to level 4"),
         actions = {
            "checksoldier=soldier attack 3",
            "return=failed unless site has sword_long",
            "return=failed unless site has honey_bread",
            "return=failed unless site has mead",
            "return=failed unless site has smoked_fish,smoked_meat",
            "animate=working 22800",
            "checksoldier=soldier attack 3", -- Because the soldier can be expelled by the player
            "consume=sword_long honey_bread mead smoked_fish,smoked_meat",
            "train=soldier attack 3 4"
         }
      },
      upgrade_soldier_attack_4 = {
         -- TRANSLATORS: Completed/Skipped/Did not start upgrading ... because ...
         descname = pgettext ("frisians_building", "upgrading soldier attack from level 4 to level 5"),
         actions = {
            "checksoldier=soldier attack 4",
            "return=failed unless site has sword_broad",
            "return=failed unless site has honey_bread",
            "return=failed unless site has mead",
            "return=failed unless site has smoked_fish,smoked_meat",
            "animate=working 15600",
            "checksoldier=soldier attack 4", -- Because the soldier can be expelled by the player
            "consume=sword_broad honey_bread mead smoked_fish,smoked_meat",
            "train=soldier attack 4 5",
            "produce=scrap_iron:2"
         }
      },
      upgrade_soldier_attack_5 = {
         -- TRANSLATORS: Completed/Skipped/Did not start upgrading ... because ...
         descname = pgettext ("frisians_building", "upgrading soldier attack from level 5 to level 6"),
         actions = {
            "checksoldier=soldier attack 5",
            "return=failed unless site has sword_double",
            "return=failed unless site has honey_bread",
            "return=failed unless site has mead",
            "return=failed unless site has smoked_fish,smoked_meat",
            "animate=working 15600",
            "checksoldier=soldier attack 5", -- Because the soldier can be expelled by the player
            "consume=sword_double honey_bread mead smoked_fish,smoked_meat",
            "train=soldier attack 5 6",
            "produce=scrap_iron scrap_metal_mixed"
         }
      },
      upgrade_soldier_defense_1 = {
         -- TRANSLATORS: Completed/Skipped/Did not start upgrading ... because ...
         descname = pgettext ("frisians_building", "upgrading soldier defense from level 1 to level 2"),
         actions = {
            "checksoldier=soldier defense 1",
            "return=failed unless site has fur_garment_golden",
            "return=failed unless site has honey_bread,mead",
            "return=failed unless site has smoked_fish,smoked_meat",
            "animate=working 22800",
            "checksoldier=soldier defense 1", -- Because the soldier can be expelled by the player
            "consume=fur_garment_golden honey_bread,mead smoked_fish,smoked_meat",
            "train=soldier defense 1 2",
            "produce=scrap_iron fur_garment_old"
         }
      },
      upgrade_soldier_health_1 = {
         -- TRANSLATORS: Completed/Skipped/Did not start upgrading ... because ...
         descname = pgettext ("frisians_building", "upgrading soldier health from level 1 to level 2"),
         actions = {
            "checksoldier=soldier health 1",
            "return=failed unless site has helmet_golden",
            "return=failed unless site has honey_bread,mead",
            "return=failed unless site has smoked_fish,smoked_meat",
            "animate=working 22800",
            "checksoldier=soldier health 1", -- Because the soldier can be expelled by the player
            "consume=helmet_golden honey_bread,mead smoked_fish,smoked_meat",
            "train=soldier health 1 2",
            "produce=scrap_iron"
         }
      },
   },

   soldier_capacity = 6,
   trainer_patience = 3
}
