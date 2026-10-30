// weapon_tab.cpp — Online Menu > Weapons: catalogue browser + give parameters.
// UI only. No engine calls, no emitted Lua — state is local to this pane.
#include "weapon_tab.h"
#include "weapon_db.h"
#include "../imgui/imgui.h"
#include <vector>
#include <cstdio>

namespace {

struct WeaponParams {
	int  ammo = 250;
	bool suppressor = false;
	bool scope = false;
	bool grip = false;
	bool flashlight = false;
	bool luxuryFinish = false;
	bool allAttachments = false;
	int  tint = 0;
};

struct AttachmentToggle {
	const char* label;
	bool        on;
};

static WeaponParams      s_params;
static char              s_search[64] = "";
static int               s_classFilter = -1;   // -1 = all
static int               s_selected = 0;
static std::vector<char> s_fav;
static char              s_status[128] = "Select a weapon, set attachments, then spawn.";

static void EnsureFav() {
	if (s_fav.size() != (size_t)g_weaponDbCount)
		s_fav.assign((size_t)g_weaponDbCount, 0);
}

static bool IContains(const char* hay, const char* needle) {
	if (!needle || !*needle) return true;
	if (!hay) return false;
	for (const char* h = hay; *h; ++h) {
		const char* a = h;
		const char* b = needle;
		while (*a && *b) {
			char ca = *a, cb = *b;
			if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
			if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
			if (ca != cb) break;
			++a; ++b;
		}
		if (!*b) return true;
	}
	return false;
}

static void PushPane() {
	ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(13, 14, 20, 240));
	ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(32, 35, 48, 255));
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
}

static void PopPane() {
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(2);
}

static void SectionLabel(const char* text) {
	ImGui::Dummy(ImVec2(0.0f, 2.0f));
	ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.68f, 1.0f), "%s", text);
	ImGui::Separator();
}

} // namespace

void WeaponTab_Render(float x, float y, float w, float h)
{
	EnsureFav();

	const float leftW = 245.0f;
	const float gap = 6.0f;
	const float rightW = w - leftW - gap;

	// ── Left pane: catalogue ─────────────────────────────────────────
	ImGui::SetCursorScreenPos(ImVec2(x, y));
	PushPane();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
	ImGui::BeginChild("whp_list", ImVec2(leftW, h), true);

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
	ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(16, 15, 18, 255));
	ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(35, 35, 40, 255));
	ImGui::SetNextItemWidth(leftW - 20.0f);
	ImGui::InputTextWithHint("##whsearch", "Search weapon", s_search, sizeof(s_search));
	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar();

	static const char* kChips[] = { "All", "Handgun", "SMG", "Rifle", "Shotgun",
		"Sniper", "Heavy", "Throw", "Melee", "Misc" };
	static const int kChipClass[] = { -1, WC_HANDGUN, WC_SMG, WC_RIFLE, WC_SHOTGUN,
		WC_SNIPER, WC_HEAVY, WC_THROWABLE, WC_MELEE, WC_MISC };

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
	for (int i = 0; i < (int)(sizeof(kChips) / sizeof(kChips[0])); ++i) {
		bool active = (s_classFilter == kChipClass[i]);
		ImGui::PushStyleColor(ImGuiCol_Button, active ? IM_COL32(245, 248, 255, 255) : IM_COL32(22, 24, 33, 255));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? IM_COL32(255, 255, 255, 255) : IM_COL32(34, 37, 50, 255));
		ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(0.06f, 0.06f, 0.08f, 1.0f) : ImVec4(0.76f, 0.78f, 0.85f, 1.0f));
		if (ImGui::Button(kChips[i], ImVec2(52.0f, 20.0f)))
			s_classFilter = kChipClass[i];
		ImGui::PopStyleColor(3);
		if (i % 4 != 3) ImGui::SameLine();
	}
	ImGui::PopStyleVar();
	ImGui::Separator();

	int shown = 0;
	for (int i = 0; i < g_weaponDbCount; ++i) {
		const WeaponEntry& wp = g_weaponDb[i];
		if (s_classFilter >= 0 && (int)wp.wclass != s_classFilter) continue;
		if (!IContains(wp.display, s_search) && !IContains(wp.model, s_search)) continue;

		++shown;
		ImGui::PushID(i);
		bool sel = (s_selected == i);

		char label[160];
		snprintf(label, sizeof(label), "%s%s", s_fav[(size_t)i] ? "* " : "", wp.display);

		if (sel) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
		else     ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.80f, 0.86f, 1.0f));
		if (ImGui::Selectable(label, sel, ImGuiSelectableFlags_None, ImVec2(leftW - 52.0f, 0.0f)))
			s_selected = i;
		ImGui::PopStyleColor();

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s\n%s", wp.model, WeaponClassName(wp.wclass));

		ImGui::SameLine();
		if (ImGui::SmallButton(s_fav[(size_t)i] ? "-" : "+"))
			s_fav[(size_t)i] = (char)!s_fav[(size_t)i];
		ImGui::PopID();
	}
	if (shown == 0)
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "No weapons match.");

	ImGui::EndChild();
	ImGui::PopStyleVar();
	PopPane();

	// ── Right pane: give parameters ──────────────────────────────────
	const WeaponEntry& sel = g_weaponDb[(s_selected >= 0 && s_selected < g_weaponDbCount) ? s_selected : 0];

	ImGui::SetCursorScreenPos(ImVec2(x + leftW + gap, y));
	PushPane();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
	ImGui::BeginChild("whp_params", ImVec2(rightW, h), true);

	ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", sel.display);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.52f, 0.54f, 0.62f, 1.0f), " %s  |  %s", WeaponClassName(sel.wclass), sel.model);

	SectionLabel("GIVE SETTINGS");
	ImGui::SetNextItemWidth(190.0f);
	ImGui::SliderInt("Ammo", &s_params.ammo, 0, 9999);

	static const char* kTint[] = { "Default", "Green", "Gold", "Pink", "Army", "LSPD", "Orange", "Platinum" };
	ImGui::SetNextItemWidth(150.0f);
	ImGui::Combo("Tint", &s_params.tint, kTint, 8);

	SectionLabel("ATTACHMENTS");
	ImGui::Checkbox("All Attachments", &s_params.allAttachments);
	if (s_params.allAttachments) {
		s_params.suppressor = true;
		s_params.scope = true;
		s_params.grip = true;
		s_params.flashlight = true;
		s_params.luxuryFinish = true;
	}
	ImGui::Checkbox("Suppressor", &s_params.suppressor);
	ImGui::SameLine();
	ImGui::Checkbox("Scope", &s_params.scope);
	ImGui::Checkbox("Grip", &s_params.grip);
	ImGui::SameLine();
	ImGui::Checkbox("Flashlight", &s_params.flashlight);
	ImGui::Checkbox("Luxury Finish", &s_params.luxuryFinish);

	ImGui::Dummy(ImVec2(0.0f, 6.0f));
	ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(245, 248, 255, 255));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 255, 255, 255));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(220, 226, 240, 255));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.06f, 0.06f, 0.08f, 1.0f));
	if (ImGui::Button("SPAWN WEAPON", ImVec2(rightW - 36.0f, 34.0f)))
		snprintf(s_status, sizeof(s_status), "%s queued for spawn.", sel.display);
	ImGui::PopStyleColor(4);

	ImGui::Dummy(ImVec2(0.0f, 2.0f));
	ImGui::TextColored(ImVec4(0.52f, 0.55f, 0.63f, 1.0f), "%s", s_status);

	ImGui::EndChild();
	ImGui::PopStyleVar();
	PopPane();
}
