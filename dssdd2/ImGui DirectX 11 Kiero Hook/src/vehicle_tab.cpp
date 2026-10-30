// vehicle_tab.cpp — Online Menu > Vehicles: catalogue browser + spawn parameters.
// UI only. No engine calls, no emitted Lua — state is local to this pane.
#include "vehicle_tab.h"
#include "vehicle_db.h"
#include "../imgui/imgui.h"
#include <string>
#include <vector>
#include <cstdio>

namespace {

struct VehParams {
	char  plate[9] = "REDENG";
	int   primary = 0;
	int   secondary = 0;
	int   tint = 0;
	bool  neon = false;
	int   neonR = 255, neonG = 0, neonB = 255;
	bool  maxPerf = true;
	bool  warp = true;
	bool  deletePrev = true;
	float spawnDist = 5.0f;
};

struct FeatureToggle {
	const char* label;
	bool        on;
};

static VehParams            s_params;
static char                 s_search[64] = "";
static int                  s_classFilter = -1;   // -1 = all
static int                  s_selected = 0;
static std::vector<char>    s_fav;
static char                 s_status[128] = "Select a model, set parameters, then spawn.";

static FeatureToggle s_features[] = {
	{ "Repair Engine",   false },
	{ "Vehicle Godmode", true  },
	{ "Speed Boost",     true  },
	{ "Drift Mode",      false },
	{ "Auto Flip",       true  },
	{ "Rainbow Color",   false },
};

static void EnsureFav() {
	if (s_fav.size() != (size_t)g_vehicleDbCount)
		s_fav.assign((size_t)g_vehicleDbCount, 0);
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

void VehicleTab_Render(float x, float y, float w, float h)
{
	EnsureFav();

	const float leftW = 245.0f;
	const float gap = 6.0f;
	const float rightW = w - leftW - gap;

	// ── Left pane: catalogue ─────────────────────────────────────────
	ImGui::SetCursorScreenPos(ImVec2(x, y));
	PushPane();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
	ImGui::BeginChild("vhp_list", ImVec2(leftW, h), true);

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
	ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(16, 15, 18, 255));
	ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(35, 35, 40, 255));
	ImGui::SetNextItemWidth(leftW - 20.0f);
	ImGui::InputTextWithHint("##vhsearch", "Search model / maker", s_search, sizeof(s_search));
	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar();

	static const char* kChips[] = { "All", "Super", "Sports", "Muscle", "Sedans",
		"Off-Road", "Military", "Emergency", "Motos", "Boats" };
	static const int kChipClass[] = { -1, VC_SUPER, VC_SPORTS, VC_MUSCLE, VC_SEDANS,
		VC_OFF_ROAD, VC_MILITARY, VC_EMERGENCY, VC_MOTORCYCLES, VC_BOATS };

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
	for (int i = 0; i < g_vehicleDbCount; ++i) {
		const VehicleEntry& v = g_vehicleDb[i];
		if (s_classFilter >= 0 && (int)v.vclass != s_classFilter) continue;
		if (!IContains(v.display, s_search) && !IContains(v.model, s_search) && !IContains(v.maker, s_search)) continue;

		++shown;
		ImGui::PushID(i);
		bool sel = (s_selected == i);

		char label[160];
		snprintf(label, sizeof(label), "%s%s", s_fav[(size_t)i] ? "* " : "", v.display);

		bool isSel = sel;
		if (isSel) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
		else       ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.80f, 0.86f, 1.0f));
		if (ImGui::Selectable(label, isSel, ImGuiSelectableFlags_None, ImVec2(leftW - 52.0f, 0.0f)))
			s_selected = i;
		ImGui::PopStyleColor();

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s\n%s  |  %s", v.model, v.maker, VehicleClassName(v.vclass));

		ImGui::SameLine();
		if (ImGui::SmallButton(s_fav[(size_t)i] ? "-" : "+"))
			s_fav[(size_t)i] = (char)!s_fav[(size_t)i];
		ImGui::PopID();
	}
	if (shown == 0)
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "No models match.");

	ImGui::EndChild();
	ImGui::PopStyleVar();
	PopPane();

	// ── Right pane: parameters + vehicle features ────────────────────
	const VehicleEntry& sel = g_vehicleDb[(s_selected >= 0 && s_selected < g_vehicleDbCount) ? s_selected : 0];

	ImGui::SetCursorScreenPos(ImVec2(x + leftW + gap, y));
	PushPane();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
	ImGui::BeginChild("vhp_params", ImVec2(rightW, h), true);

	ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", sel.display);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.52f, 0.54f, 0.62f, 1.0f), " %s  |  %s", sel.maker, VehicleClassName(sel.vclass));

	SectionLabel("SPAWN SETTINGS");
	ImGui::SetNextItemWidth(110.0f);
	ImGui::InputText("Plate", s_params.plate, sizeof(s_params.plate));
	ImGui::SameLine();
	ImGui::SetNextItemWidth(60.0f);
	ImGui::InputInt("Color 1", &s_params.primary);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(60.0f);
	ImGui::InputInt("Color 2", &s_params.secondary);

	static const char* kTint[] = { "None", "Pure Black", "Dark Smoke", "Light Smoke", "Stock", "Limo", "Green" };
	ImGui::SetNextItemWidth(150.0f);
	ImGui::Combo("Tint", &s_params.tint, kTint, 7);

	ImGui::SetNextItemWidth(170.0f);
	ImGui::SliderFloat("Spawn Offset", &s_params.spawnDist, 1.0f, 25.0f, "%.1f m");

	ImGui::Checkbox("Max Performance", &s_params.maxPerf);
	ImGui::SameLine();
	ImGui::Checkbox("Warp In", &s_params.warp);
	ImGui::SameLine();
	ImGui::Checkbox("Replace Prev", &s_params.deletePrev);

	ImGui::Checkbox("Neon", &s_params.neon);
	if (s_params.neon) {
		ImGui::SetNextItemWidth(190.0f);
		ImGui::SliderInt("R", &s_params.neonR, 0, 255);
		ImGui::SetNextItemWidth(190.0f);
		ImGui::SliderInt("G", &s_params.neonG, 0, 255);
		ImGui::SetNextItemWidth(190.0f);
		ImGui::SliderInt("B", &s_params.neonB, 0, 255);
	}

	SectionLabel("VEHICLE FEATURES");
	for (FeatureToggle& f : s_features)
		ImGui::Checkbox(f.label, &f.on);

	ImGui::Dummy(ImVec2(0.0f, 6.0f));
	ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(245, 248, 255, 255));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 255, 255, 255));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(220, 226, 240, 255));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.06f, 0.06f, 0.08f, 1.0f));
	if (ImGui::Button("SPAWN VEHICLE", ImVec2(rightW - 36.0f, 34.0f)))
		snprintf(s_status, sizeof(s_status), "%s queued for spawn.", sel.display);
	ImGui::PopStyleColor(4);

	ImGui::Dummy(ImVec2(0.0f, 2.0f));
	ImGui::TextColored(ImVec4(0.52f, 0.55f, 0.63f, 1.0f), "%s", s_status);

	ImGui::EndChild();
	ImGui::PopStyleVar();
	PopPane();
}
