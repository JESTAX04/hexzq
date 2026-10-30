// weapon_db.h — FiveM/GTA V weapon catalogue. Real model names only; the pane is UI,
// so these are display/selection data and nothing here reaches the game.
#pragma once
#include <cstdint>

enum WeaponClass : uint8_t {
	WC_MELEE     = 0,
	WC_HANDGUN   = 1,
	WC_SMG       = 2,
	WC_RIFLE     = 3,
	WC_SHOTGUN   = 4,
	WC_SNIPER    = 5,
	WC_HEAVY     = 6,
	WC_THROWABLE = 7,
	WC_MISC      = 8,
};

inline const char* WeaponClassName(WeaponClass c) {
	switch (c) {
		case WC_MELEE:     return "Melee";
		case WC_HANDGUN:   return "Handgun";
		case WC_SMG:       return "SMG";
		case WC_RIFLE:     return "Rifle";
		case WC_SHOTGUN:   return "Shotgun";
		case WC_SNIPER:    return "Sniper";
		case WC_HEAVY:     return "Heavy";
		case WC_THROWABLE: return "Throwable";
		case WC_MISC:      return "Misc";
	}
	return "Other";
}

struct WeaponEntry {
	const char* model;       // full model name, e.g. WEAPON_PISTOL
	const char* display;     // friendly name
	WeaponClass wclass;
	int         defaultAmmo; // sensible give amount
};

inline constexpr WeaponEntry g_weaponDb[] = {
	// ── Melee ──
	{ "WEAPON_UNARMED",        "Unarmed",           WC_MELEE,     0 },
	{ "WEAPON_KNIFE",          "Knife",             WC_MELEE,     0 },
	{ "WEAPON_NIGHTSTICK",     "Nightstick",        WC_MELEE,     0 },
	{ "WEAPON_HAMMER",         "Hammer",            WC_MELEE,     0 },
	{ "WEAPON_BAT",            "Baseball Bat",      WC_MELEE,     0 },
	{ "WEAPON_CROWBAR",        "Crowbar",           WC_MELEE,     0 },
	{ "WEAPON_GOLFCLUB",       "Golf Club",         WC_MELEE,     0 },
	{ "WEAPON_BOTTLE",         "Bottle",            WC_MELEE,     0 },
	{ "WEAPON_DAGGER",         "Dagger",            WC_MELEE,     0 },
	{ "WEAPON_WRENCH",         "Wrench",            WC_MELEE,     0 },
	{ "WEAPON_BATTLEAXE",      "Battle Axe",        WC_MELEE,     0 },
	{ "WEAPON_POOLCUE",        "Pool Cue",          WC_MELEE,     0 },
	{ "WEAPON_STONE_HATCHET",  "Stone Hatchet",     WC_MELEE,     0 },
	{ "WEAPON_FLASHLIGHT",     "Flashlight",        WC_MELEE,     0 },
	{ "WEAPON_SWITCHBLADE",    "Switchblade",       WC_MELEE,     0 },
	{ "WEAPON_MACHETE",        "Machete",           WC_MELEE,     0 },
	{ "WEAPON_HATCHET",        "Hatchet",           WC_MELEE,     0 },
	{ "WEAPON_KNUCKLE",        "Knuckles",          WC_MELEE,     0 },

	// ── Handguns ──
	{ "WEAPON_PISTOL",         "Pistol",            WC_HANDGUN,   250 },
	{ "WEAPON_PISTOL_MK2",     "Pistol Mk II",      WC_HANDGUN,   250 },
	{ "WEAPON_COMBATPISTOL",   "Combat Pistol",     WC_HANDGUN,   250 },
	{ "WEAPON_APPISTOL",       "AP Pistol",         WC_HANDGUN,   250 },
	{ "WEAPON_PISTOL50",       "Pistol .50",        WC_HANDGUN,   180 },
	{ "WEAPON_SNSPISTOL",      "SNS Pistol",        WC_HANDGUN,   250 },
	{ "WEAPON_SNSPISTOL_MK2",  "SNS Pistol Mk II",  WC_HANDGUN,   250 },
	{ "WEAPON_HEAVYPISTOL",    "Heavy Pistol",      WC_HANDGUN,   180 },
	{ "WEAPON_VINTAGEPISTOL",  "Vintage Pistol",    WC_HANDGUN,   200 },
	{ "WEAPON_FLAREGUN",       "Flare Gun",         WC_HANDGUN,    20 },
	{ "WEAPON_MARKSMANPISTOL", "Marksman Pistol",   WC_HANDGUN,   100 },
	{ "WEAPON_REVOLVER",       "Heavy Revolver",    WC_HANDGUN,   120 },
	{ "WEAPON_REVOLVER_MK2",   "Heavy Revolver Mk II", WC_HANDGUN, 120 },
	{ "WEAPON_DOUBLEACTION",   "Double Action Revolver", WC_HANDGUN, 120 },
	{ "WEAPON_CERAMICPISTOL",  "Ceramic Pistol",    WC_HANDGUN,   250 },
	{ "WEAPON_RAYCHARGER",     "Up-n-Atomizer",     WC_HANDGUN,    40 },
	// ── SMG ──
	{ "WEAPON_MICROSMG",       "Micro SMG",         WC_SMG,       600 },
	{ "WEAPON_SMG",            "SMG",               WC_SMG,       600 },
	{ "WEAPON_SMG_MK2",        "SMG Mk II",         WC_SMG,       600 },
	{ "WEAPON_ASSAULTSMG",     "Assault SMG",       WC_SMG,       600 },
	{ "WEAPON_COMBATPDW",      "Combat PDW",        WC_SMG,       600 },
	{ "WEAPON_MACHINEPISTOL",  "Machine Pistol",    WC_SMG,       600 },
	{ "WEAPON_MINISMG",        "Mini SMG",          WC_SMG,       600 },
	{ "WEAPON_TECPISTOL",      "Tec-9",             WC_SMG,       600 },

	// ── Rifles ──
	{ "WEAPON_ASSAULTRIFLE",      "Assault Rifle",     WC_RIFLE,  600 },
	{ "WEAPON_ASSAULTRIFLE_MK2",  "Assault Rifle Mk II", WC_RIFLE, 600 },
	{ "WEAPON_CARBINERIFLE",      "Carbine Rifle",     WC_RIFLE,  600 },
	{ "WEAPON_CARBINERIFLE_MK2",  "Carbine Rifle Mk II", WC_RIFLE, 600 },
	{ "WEAPON_ADVANCEDRIFLE",     "Advanced Rifle",    WC_RIFLE,  600 },
	{ "WEAPON_SPECIALCARBINE",    "Special Carbine",   WC_RIFLE,  600 },
	{ "WEAPON_SPECIALCARBINE_MK2","Special Carbine Mk II", WC_RIFLE, 600 },
	{ "WEAPON_BULLPUPRIFLE",      "Bullpup Rifle",     WC_RIFLE,  600 },
	{ "WEAPON_BULLPUPRIFLE_MK2",  "Bullpup Rifle Mk II", WC_RIFLE, 600 },
	{ "WEAPON_COMPACTRIFLE",      "Compact Rifle",     WC_RIFLE,  600 },
	{ "WEAPON_MILITARYRIFLE",     "Military Rifle",    WC_RIFLE,  600 },
	{ "WEAPON_HEAVYRIFLE",        "Heavy Rifle",       WC_RIFLE,  600 },
	{ "WEAPON_SERVICERIFLE",      "Service Rifle",     WC_RIFLE,  600 },

	// ── Shotguns ──
	{ "WEAPON_PUMPSHOTGUN",    "Pump Shotgun",      WC_SHOTGUN,  200 },
	{ "WEAPON_PUMPSHOTGUN_MK2","Pump Shotgun Mk II",WC_SHOTGUN,  200 },
	{ "WEAPON_SAWNOFFSHOTGUN", "Sawed-Off Shotgun", WC_SHOTGUN,  200 },
	{ "WEAPON_ASSAULTSHOTGUN", "Assault Shotgun",   WC_SHOTGUN,  200 },
	{ "WEAPON_BULLPUPSHOTGUN", "Bullpup Shotgun",   WC_SHOTGUN,  200 },
	{ "WEAPON_HEAVYSHOTGUN",   "Heavy Shotgun",     WC_SHOTGUN,  200 },
	{ "WEAPON_DBSHOTGUN",      "Double Barrel",     WC_SHOTGUN,  200 },
	{ "WEAPON_AUTOSHOTGUN",    "Sweeper Shotgun",   WC_SHOTGUN,  200 },
	{ "WEAPON_COMBATSHOTGUN",  "Combat Shotgun",    WC_SHOTGUN,  200 },

	// ── Snipers ──
	{ "WEAPON_SNIPERRIFLE",    "Sniper Rifle",      WC_SNIPER,    80 },
	{ "WEAPON_HEAVYSNIPER",    "Heavy Sniper",      WC_SNIPER,    60 },
	{ "WEAPON_HEAVYSNIPER_MK2","Heavy Sniper Mk II",WC_SNIPER,    60 },
	{ "WEAPON_MARKSMANRIFLE",  "Marksman Rifle",    WC_SNIPER,   100 },
	{ "WEAPON_MARKSMANRIFLE_MK2","Marksman Rifle Mk II", WC_SNIPER, 100 },
	{ "WEAPON_PRECISIONRIFLE", "Precision Rifle",   WC_SNIPER,   100 },

	// ── Heavy ──
	{ "WEAPON_GRENADELAUNCHER",   "Grenade Launcher",  WC_HEAVY,   20 },
	{ "WEAPON_GRENADELAUNCHER_SMOKE","Smoke Launcher", WC_HEAVY,   20 },
	{ "WEAPON_RPG",               "RPG",               WC_HEAVY,   10 },
	{ "WEAPON_MINIGUN",           "Minigun",           WC_HEAVY,  400 },
	{ "WEAPON_FIREWORK",          "Firework Launcher", WC_HEAVY,   20 },
	{ "WEAPON_HOMINGLAUNCHER",    "Homing Launcher",   WC_HEAVY,   10 },
	{ "WEAPON_RAILGUN",           "Railgun",           WC_HEAVY,   20 },
	{ "WEAPON_RAILGUNXM3",        "Railgun XM3",       WC_HEAVY,   20 },
	{ "WEAPON_COMPACTLAUNCHER",   "Compact Launcher",  WC_HEAVY,   20 },
	{ "WEAPON_EMPLAUNCHER",       "EMP Launcher",      WC_HEAVY,   20 },

	// ── Throwables ──
	{ "WEAPON_GRENADE",        "Grenade",           WC_THROWABLE, 25 },
	{ "WEAPON_STICKYBOMB",     "Sticky Bomb",       WC_THROWABLE, 25 },
	{ "WEAPON_PROXIMITYMINE",  "Proximity Mine",    WC_THROWABLE, 25 },
	{ "WEAPON_BZGAS",          "BZ Gas",            WC_THROWABLE, 25 },
	{ "WEAPON_SMOKEGRENADE",   "Smoke Grenade",     WC_THROWABLE, 25 },
	{ "WEAPON_FLARE",          "Flare",             WC_THROWABLE, 25 },
	{ "WEAPON_MOLOTOV",        "Molotov",           WC_THROWABLE, 25 },
	{ "WEAPON_SNOWBALL",       "Snowball",          WC_THROWABLE, 25 },
	{ "WEAPON_ACIDPACKAGE",    "Acid Package",      WC_THROWABLE, 25 },
	{ "WEAPON_PIPEBOMB",       "Pipe Bomb",         WC_THROWABLE, 25 },

	// ── Misc ──
	{ "WEAPON_STUNGUN",        "Stun Gun",          WC_MISC,     100 },
	{ "WEAPON_PARACHUTE",      "Parachute",         WC_MISC,       1 },
	{ "WEAPON_PETROLCAN",      "Jerry Can",         WC_MISC,    4500 },
	{ "WEAPON_FIREEXTINGUISHER","Fire Extinguisher",WC_MISC,    4000 },
	{ "WEAPON_HAZARDCAN",      "Hazardous Can",     WC_MISC,    4500 },
	{ "WEAPON_DIGISCANNER",    "Digi Scanner",      WC_MISC,       1 },
	{ "WEAPON_METALDETECTOR",  "Metal Detector",    WC_MISC,       1 },
};

inline constexpr int g_weaponDbCount = (int)(sizeof(g_weaponDb) / sizeof(g_weaponDb[0]));
