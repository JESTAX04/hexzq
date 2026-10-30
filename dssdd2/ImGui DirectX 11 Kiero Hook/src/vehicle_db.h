// vehicle_db.h — FiveM/GTA V vehicle catalogue.
// Model is a string; the hash is computed at runtime via GetHashKey in the emitted Lua,
// so the table never needs a per-build hash retarget. Only real models; the live
// "nearby" list is populated from the game, not from this file.
#pragma once
#include <cstdint>

enum VehicleClass : uint8_t {
	VC_COMPACTS       = 0,
	VC_SEDANS         = 1,
	VC_SUVS           = 2,
	VC_COUPES         = 3,
	VC_MUSCLE         = 4,
	VC_SPORTS         = 5,
	VC_SPORTS_CLASSIC = 6,
	VC_SUPER          = 7,
	VC_MOTORCYCLES    = 8,
	VC_OFF_ROAD       = 9,
	VC_INDUSTRIAL     = 10,
	VC_UTILITY        = 11,
	VC_VANS           = 12,
	VC_CYCLES         = 13,
	VC_BOATS          = 14,
	VC_HELICOPTERS    = 15,
	VC_PLANES         = 16,
	VC_SERVICE        = 17,
	VC_EMERGENCY      = 18,
	VC_MILITARY       = 19,
	VC_COMMERCIAL     = 20,
	VC_TRAINS         = 21,
	VC_OPEN_WHEEL     = 22,
};

inline const char* VehicleClassName(VehicleClass c) {
	switch (c) {
		case VC_COMPACTS:       return "Compacts";
		case VC_SEDANS:         return "Sedans";
		case VC_SUVS:           return "SUVs";
		case VC_COUPES:         return "Coupes";
		case VC_MUSCLE:         return "Muscle";
		case VC_SPORTS:         return "Sports";
		case VC_SPORTS_CLASSIC: return "Sports Classic";
		case VC_SUPER:          return "Super";
		case VC_MOTORCYCLES:    return "Motorcycles";
		case VC_OFF_ROAD:       return "Off-Road";
		case VC_INDUSTRIAL:     return "Industrial";
		case VC_UTILITY:        return "Utility";
		case VC_VANS:           return "Vans";
		case VC_CYCLES:         return "Cycles";
		case VC_BOATS:          return "Boats";
		case VC_HELICOPTERS:    return "Helicopters";
		case VC_PLANES:         return "Planes";
		case VC_SERVICE:        return "Service";
		case VC_EMERGENCY:      return "Emergency";
		case VC_MILITARY:       return "Military";
		case VC_COMMERCIAL:     return "Commercial";
		case VC_TRAINS:         return "Trains";
		case VC_OPEN_WHEEL:     return "Open Wheel";
	}
	return "Other";
}

struct VehicleEntry {
	const char* model;   // FiveM / GTA V model name
	const char* display; // friendly name
	const char* maker;   // manufacturer
	VehicleClass vclass;
};

// Real, well-known models. Add more freely — data only, no build coupling.
inline constexpr VehicleEntry g_vehicleDb[] = {
	{ "sentinel",  "Sentinel",      "Pegassi",   VC_SEDANS        },
	{ "premier",   "Premier",       "Declasse",  VC_SEDANS        },
	{ "faction",   "Faction",       "Declasse",  VC_MUSCLE        },
	{ "dominator", "Dominator",     "Vapid",     VC_MUSCLE        },
	{ "dukes",     "Dukes",         "Imponte",   VC_MUSCLE        },
	{ "gauntlet",  "Gauntlet",      "Bravado",   VC_MUSCLE        },
	{ "vigero",    "Vigero",        "Declasse",  VC_MUSCLE        },
	{ "coquette",  "Coquette",      "Invet",     VC_SPORTS        },
	{ "carbonizzare","Carbonizzare","Lampadati", VC_SPORTS        },
	{ "feltzer2",  "Feltzer",       "Benefactor",VC_SPORTS        },
	{ "9f",        "9F",            "Obey",      VC_SPORTS        },
	{ "comet2",    "Comet",         "Pfister",   VC_SPORTS        },
	{ "comet3",    "Comet S2",      "Pfister",   VC_SPORTS        },
	{ "comet4",    "Comet SR",      "Pfister",   VC_SPORTS        },
	{ "surano",    "Surano",        "Benefactor",VC_SPORTS        },
	{ "furoregt",  "Furore GT",     "Lampadati", VC_SPORTS        },
	{ "vacca",     "Vacca",         "Pegassi",   VC_SPORTS        },
	{ "zentorno",  "Zentorno",      "Pegassi",   VC_SPORTS        },
	{ "entityxf",  "Entity XF",     "Pegassi",   VC_SPORTS        },
	{ "t20",       "T20",           "Pegassi",   VC_SPORTS        },
	{ "osiris",    "Osiris",        "Pegassi",   VC_SPORTS        },
	{ "italigtb",  "Itali GTB",     "Pegassi",   VC_SPORTS        },
	{ "italigtb2", "Itali GTB",     "Pegassi",   VC_SPORTS        },
	{ "infernus2", "Infernus Classic","Pegassi", VC_SPORTS        },
	{ "nero",      "Nero",          "Pegassi",   VC_SPORTS        },
	{ "nero2",     "Nero Custom",   "Pegassi",   VC_SPORTS        },
	{ "reaper",    "Reaper",        "Pegassi",   VC_SPORTS        },
	{ "toreador",  "Toreador",      "Pegassi",   VC_SPORTS        },
	{ "cyclone",   "Cyclone",       "Pegassi",   VC_SPORTS        },
	{ "adder",     "Adder",         "Truffade",  VC_SUPER         },
	{ "bullet",    "Bullet",        "Vapid",     VC_SUPER         },
	{ "entity2",   "Entity XXR",    "Pegassi",   VC_SUPER         },
	{ "voltic",    "Voltic",        "Coil",      VC_SUPER         },
	{ "cheetah",   "Cheetah",       "Grotti",    VC_SUPER         },
	{ "turismor",  "Turismo R",     "Grotti",    VC_SUPER         },
	{ "tyrus",     "Tyrus",         "Vapid",     VC_SUPER         },
	{ "fmj",       "FMJ",           "Vapid",     VC_SUPER         },
	{ "xa21",      "XA-21",         "Truffade",  VC_SUPER         },
	{ "lm87",      "LM87",          "Lampadati", VC_SUPER         },
	{ "ignus",     "Ignus",         "Pegassi",   VC_SUPER         },
	{ "ignus2",    "Ignus Alt",     "Pegassi",   VC_SUPER         },
	{ "emerus",    "Emerus",        "Pegassi",   VC_SUPER         },
	{ "schlagen",  "Schlagen",      "Benefactor",VC_SPORTS        },
	{ "locust",    "Locust",        "Pegassi",   VC_SPORTS        },
	{ "jugular",   "Jugular",       "Ocelot",    VC_SPORTS        },
	{ "paragon",   "Paragon R",     "Ocelot",    VC_SPORTS        },
	{ "neon",      "Neon",          "Pfister",   VC_SPORTS        },
	{ "x80proto",  "X80 Proto",     "Pegassi",   VC_SUPER         },
	{ "cyclone2",  "Cyclone 2",     "Pegassi",   VC_SPORTS        },
	{ "penumbra",  "Penumbra",      "Maibatsu",  VC_SPORTS        },
	{ "300r",      "300R",          "Pfister",   VC_SPORTS        },
	{ "drafter",   "811",           "Pfister",   VC_SPORTS        },
	{ "sultanrs",  "Sultan RS",     "Karin",     VC_SPORTS        },
	{ "btype",     "B-Type",        "Albany",    VC_SPORTS_CLASSIC},
	{ "btype2",    "B-Type 2",      "Albany",    VC_SPORTS_CLASSIC},
	{ "btype3",    "B-Type 3",      "Albany",    VC_SPORTS_CLASSIC},
	{ "feltzer2",  "Feltzer",       "Benefactor",VC_SPORTS        },
	{ "kuruma",    "Kuruma",        "Karin",     VC_COMPACTS      },
	{ "kuruma2",   "Kuruma Armored","Karin",     VC_COMPACTS      },
	{ "elegy",     "Elegy",         "Annis",     VC_SPORTS        },
	{ "elegy2",    "Elegy Retro",   "Annis",     VC_SPORTS        },
	{ "sultan",    "Sultan",        "Karin",     VC_SPORTS        },
	{ "blista",    "Blista",        "Dinka",     VC_COMPACTS      },
	{ "brioso",    "Brioso",        "Pegassi",   VC_COMPACTS      },
	{ "dilettante", "Dilettante",   "Declasse",  VC_COMPACTS      },
	{ "issi2",     "Issi",          "Weeny",     VC_COMPACTS      },
	{ "panto",     "Panto",         "Weeny",     VC_COMPACTS      },
	{ "prairie",   "Prairie",       "Bollokan",  VC_COMPACTS      },
	{ "rhapsody",  "Rhapsody",      "DeClasse",  VC_COMPACTS      },
	{ "blista2",   "Blista Kanjo",  "Dinka",     VC_COMPACTS      },
	{ "blista3",   "Blista Go Go",  "Dinka",     VC_COMPACTS      },
	{ "club",      "Club",          "Pegassi",   VC_COMPACTS      },
	{ "dominator2","Dominator GTX","Vapid",     VC_SPORTS        },
	{ "ellie",     "Ellie",         "Imponte",   VC_SPORTS        },
	{ "deviant",   "Deviant",       "Imponte",   VC_SPORTS        },
	{ "rapid",     "Rapid",         "Grotti",    VC_SPORTS        },
	{ "taipan",    "Taipan",        "Grotti",    VC_SUPER         },
	{ "tempesta",  "Tempesta",      "Pegassi",   VC_SPORTS        },
	{ "boxville",  "Boxville",      "MTL",       VC_VANS          },
	{ "boxville2", "Boxville 2",    "MTL",       VC_VANS          },
	{ "boxville3", "Boxville 3",    "MTL",       VC_VANS          },
	{ "mule",      "Mule",          "HVY",       VC_COMMERCIAL    },
	{ "mule2",     "Mule 2",        "HVY",       VC_COMMERCIAL    },
	{ "mule3",     "Mule 3",        "HVY",       VC_COMMERCIAL    },
	{ "mule4",     "Mule 4",        "HVY",       VC_COMMERCIAL    },
	{ "yankee",    "Yankee",        "MTL",       VC_UTILITY       },
	{ "barracks",  "Barracks",      "MTL",       VC_MILITARY      },
	{ "barracks2", "Barracks 2",    "MTL",       VC_MILITARY      },
	{ "barracks3", "Barracks 3",    "MTL",       VC_MILITARY      },
	{ "rhino",     "Rhino",         "HVY",       VC_MILITARY      },
	{ "savage",    "Savage",        "Buckingham",VC_HELICOPTERS   },
	{ "annihilator","Annihilator",  "Buckingham",VC_HELICOPTERS   },
	{ "buzzard",   "Buzzard",       "Buckingham",VC_HELICOPTERS   },
	{ "buzzard2",  "Buzzard 2",     "Buckingham",VC_HELICOPTERS   },
	{ "cargoplane", "Cargoplane",   "Aviation",  VC_PLANES        },
	{ "jet",       "Jet",           "Aviation",  VC_PLANES        },
	{ "lazer",     "Lazer",         "Military",  VC_MILITARY      },
	{ "titan",     "Titan",         "Military",  VC_MILITARY      },
	{ "hydra",     "Hydra",         "Military",  VC_MILITARY      },
	{ "pcj600",    "PCJ-600",       "Dinka",     VC_MOTORCYCLES   },
	{ "sanchez",   "Sanchez",       "Sanchez",   VC_MOTORCYCLES   },
	{ "sanchez2",  "Sanchez 2",     "Sanchez",   VC_MOTORCYCLES   },
	{ "naked",     "Naked",         "Pegassi",   VC_MOTORCYCLES   },
	{ "double",    "Double T",      "Pegassi",   VC_MOTORCYCLES   },
	{ "akuma",     "Akuma",         "Pegassi",   VC_MOTORCYCLES   },
	{ "bagger",    "Bagger",        "Western",   VC_MOTORCYCLES   },
	{ "bati",      "Bati 801",      "Pegassi",   VC_MOTORCYCLES   },
	{ "bati2",     "Bati 801RR",    "Pegassi",   VC_MOTORCYCLES   },
	{ "bf400",     "BF400",         "Nagasaki",  VC_MOTORCYCLES   },
	{ "carbonrs",  "Carbon RS",     "Pegassi",   VC_MOTORCYCLES   },
	{ "hakuchou",  "Hakuchou",      "Pegassi",   VC_MOTORCYCLES   },
	{ "hakuchou2", "Hakuchou Drag", "Pegassi",   VC_MOTORCYCLES   },
	{ "shotaro",   "Shotaro",       "Pegassi",   VC_MOTORCYCLES   },
	{ "vortex",    "Vortex",        "Shitzu",    VC_MOTORCYCLES   },
	{ "manchez",   "Manchez",       "Maibatsu",  VC_OFF_ROAD      },
	{ "blazer",    "Blazer",        "Nagasaki",  VC_OFF_ROAD      },
	{ "bifta",     "Bifta",         "BF",        VC_OFF_ROAD      },
	{ "bfinjection","Injection",    "BF",        VC_OFF_ROAD      },
	{ "bodhi2",    "Bodhi",         "Karin",     VC_OFF_ROAD      },
	{ "dubsta3",   "Dubsta 6x6",    "Benefactor",VC_OFF_ROAD      },
	{ "mesa3",     "Mesa",          "Canis",     VC_OFF_ROAD      },
	{ "rebel2",    "Rebel",         "Karin",     VC_OFF_ROAD      },
	{ "sandking",  "Sandking XL",   "Vapid",     VC_OFF_ROAD      },
	{ "sandking2", "Sandking SWB",  "Vapid",     VC_OFF_ROAD      },
	{ "insurgent", "Insurgent",     "HVY",       VC_OFF_ROAD      },
	{ "insurgent2","Insurgent 2",   "HVY",       VC_OFF_ROAD      },
	{ "insurgent3","Insurgent 3",   "HVY",       VC_OFF_ROAD      },
	{ "trophy",    "Trophy Truck",  "Vapid",     VC_OFF_ROAD      },
	{ "trophytruck","Trophy Truck 2","Vapid",    VC_OFF_ROAD      },
	{ "caracara",  "Caracara",      "Vapid",     VC_OFF_ROAD      },
	{ "squaddie",  "Squaddie",      "BF",        VC_OFF_ROAD      },
	{ "brickade",  "Brickade",      "MTL",       VC_INDUSTRIAL    },
	{ "dloader",   "DLoader",       "HVY",       VC_INDUSTRIAL    },
	{ "dump",      "Dump",          "HVY",       VC_INDUSTRIAL    },
	{ "flatbed",   "Flatbed",       "HVY",       VC_INDUSTRIAL    },
	{ "mixer",     "Mixer",         "HVY",       VC_INDUSTRIAL    },
	{ "mixer2",    "Mixer 2",       "HVY",       VC_INDUSTRIAL    },
	{ "rubble",    "Rubble",        "HVY",       VC_INDUSTRIAL    },
	{ "tiptruck",  "Tipper",        "HVY",       VC_INDUSTRIAL    },
	{ "tiptruck2", "Tipper 2",      "HVY",       VC_INDUSTRIAL    },
	{ "bulldozer", "Bulldozer",     "HVY",       VC_INDUSTRIAL    },
	{ "towtruck",  "Tow Truck",     "HVY",       VC_UTILITY       },
	{ "towtruck2", "Tow Truck 2",   "HVY",       VC_UTILITY       },
	{ "ambulance", "Ambulance",     "Brute",     VC_EMERGENCY     },
	{ "firetruk",  "Fire Truck",    "MTL",       VC_EMERGENCY     },
	{ "police",    "Police Cruiser","Vapid",     VC_EMERGENCY     },
	{ "police2",   "Police Buffalo", "Bravado",  VC_EMERGENCY     },
	{ "police3",   "Police Interceptor","Vapid", VC_EMERGENCY     },
	{ "fbi",       "FIB",           "Vapid",     VC_EMERGENCY     },
	{ "fbi2",      "FIB 2",         "Vapid",     VC_EMERGENCY     },
	{ "riot",      "Riot",          "HVY",       VC_EMERGENCY     },
	{ "predator",  "Predator",      "HVY",       VC_BOATS         },
	{ "tropic",    "Tropic",        "Shitzu",    VC_BOATS         },
	{ "dinghy",    "Dinghy",        "Shitzu",    VC_BOATS         },
	{ "jetmax",    "Jetmax",        "Shitzu",    VC_BOATS         },
	{ "marquis",   "Marquis",       "Shitzu",    VC_BOATS         },
	{ "seashark",  "Seashark",      "Shitzu",    VC_BOATS         },
	{ "freight",   "Freight Train", "Vapid",     VC_TRAINS        },
	{ "freightcar","Freight Car",   "Vapid",     VC_TRAINS        },
	{ "bmx",       "BMX",           "Vapid",     VC_CYCLES        },
	{ "cruiser",   "Cruiser",       "Vapid",     VC_CYCLES        },
	{ "fixter",    "Fixter",        "Vapid",     VC_CYCLES        },
	{ "scorcher",  "Scorcher",      "Vapid",     VC_CYCLES        },
	{ "tribike",   "Tri Bike",      "Vapid",     VC_CYCLES        },
	{ "faggio",    "Faggio",        "Pegassi",   VC_CYCLES        },
	{ "formula",   "Formula",       "Ocelot",    VC_OPEN_WHEEL    },
	{ "formula2",  "Formula 2",     "Ocelot",    VC_OPEN_WHEEL    },
	{ "taxi",      "Taxi",          "Vapid",     VC_SERVICE       },
	{ "bus",       "Bus",           "Vapid",     VC_SERVICE       },
	{ "airbus",    "Airbus",        "Vapid",     VC_SERVICE       },
	{ "trash",     "Trashmaster",   "HVY",       VC_SERVICE       },
	{ "trash2",    "Trashmaster 2", "HVY",       VC_SERVICE       },
	{ "tourbus",   "Tour Bus",      "Vapid",     VC_SERVICE       },
};

inline constexpr int g_vehicleDbCount = (int)(sizeof(g_vehicleDb) / sizeof(g_vehicleDb[0]));

inline const VehicleEntry* FindVehicleByModel(const char* model) {
	if (!model) return nullptr;
	for (int i = 0; i < g_vehicleDbCount; ++i) {
		const char* a = g_vehicleDb[i].model;
		const char* b = model;
		while (*a && *b && *a == *b) { ++a; ++b; }
		if (*a == 0 && *b == 0) return &g_vehicleDb[i];
	}
	return nullptr;
}