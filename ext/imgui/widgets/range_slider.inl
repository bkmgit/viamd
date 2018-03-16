// https://github.com/ocornut/imgui/issues/76
// Taken from: https://github.com/wasikuss/imgui/commit/a50515ace6d9a62ebcd69817f1da927d31c39bb1

#include <unordered_map>

namespace ImGui {

struct RangeSliderState {
    bool min_grab = false;
    bool max_grab = false;
    bool range_grab = false;
	float v1_diff = 0.f;
	float v2_diff = 0.f;
};

std::unordered_map<ImGuiID, RangeSliderState> range_slider_states;

// ~80% common code with ImGui::SliderBehavior
bool RangeSliderBehavior(const ImRect& frame_bb, ImGuiID id, float* v1, float* v2, float v_min,
                         float v_max, float power, int decimal_precision, ImGuiSliderFlags flags) {
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = GetCurrentWindow();
    const ImGuiStyle& style = g.Style;

    // Draw frame
    RenderFrame(frame_bb.Min, frame_bb.Max, GetColorU32(ImGuiCol_FrameBg), true,
                style.FrameRounding);

    const bool is_non_linear = (power < 1.0f - 0.00001f) || (power > 1.0f + 0.00001f);
    const bool is_horizontal = (flags & ImGuiSliderFlags_Vertical) == 0;

    const float grab_padding = 2.0f;
    const float slider_sz = is_horizontal ? (frame_bb.GetWidth() - grab_padding * 2.0f)
                                          : (frame_bb.GetHeight() - grab_padding * 2.0f);
    float grab_sz;
    if (decimal_precision > 0)
        grab_sz = ImMin(style.GrabMinSize, slider_sz);
    else
        grab_sz = ImMin(
            ImMax(1.0f * (slider_sz / ((v_min < v_max ? v_max - v_min : v_min - v_max) + 1.0f)),
                  style.GrabMinSize),
            slider_sz);  // Integer sliders, if possible have the grab size represent 1 unit
    const float slider_usable_sz = slider_sz - grab_sz;
    const float slider_usable_pos_min =
        (is_horizontal ? frame_bb.Min.x : frame_bb.Min.y) + grab_padding + grab_sz * 0.5f;
    const float slider_usable_pos_max =
        (is_horizontal ? frame_bb.Max.x : frame_bb.Max.y) - grab_padding - grab_sz * 0.5f;

    // For logarithmic sliders that cross over sign boundary we want the exponential increase to be
    // symmetric around 0.0f
    float linear_zero_pos = 0.0f;  // 0.0->1.0f
    if (v_min * v_max < 0.0f) {
        // Different sign
        const float linear_dist_min_to_0 = powf(fabsf(0.0f - v_min), 1.0f / power);
        const float linear_dist_max_to_0 = powf(fabsf(v_max - 0.0f), 1.0f / power);
        linear_zero_pos = linear_dist_min_to_0 / (linear_dist_min_to_0 + linear_dist_max_to_0);
    } else {
        // Same sign
        linear_zero_pos = v_min < 0.0f ? 1.0f : 0.0f;
    }

    // Read state
    RangeSliderState state;
    auto it = range_slider_states.find(id);
    if (it != range_slider_states.end()) {
        state = it->second;
    }

    // Process clicking on the slider
    bool value_changed = false;
    if (g.ActiveId == id) {
        const float mouse_abs_pos = is_horizontal ? g.IO.MousePos.x : g.IO.MousePos.y;

		auto compute_val = [&]() -> float{
			float clicked_t =
				(slider_usable_sz > 0.0f)
				? ImClamp((mouse_abs_pos - slider_usable_pos_min) / slider_usable_sz, 0.0f,
					1.0f)
				: 0.0f;
			if (!is_horizontal) clicked_t = 1.0f - clicked_t;

			float new_value;
			if (is_non_linear) {
				// Account for logarithmic scale on both sides of the zero
				if (clicked_t < linear_zero_pos) {
					// Negative: rescale to the negative range before powering
					float a = 1.0f - (clicked_t / linear_zero_pos);
					a = powf(a, power);
					new_value = ImLerp(ImMin(v_max, 0.0f), v_min, a);
				}
				else {
					// Positive: rescale to the positive range before powering
					float a;
					if (fabsf(linear_zero_pos - 1.0f) > 1.e-6f)
						a = (clicked_t - linear_zero_pos) / (1.0f - linear_zero_pos);
					else
						a = clicked_t;
					a = powf(a, power);
					new_value = ImLerp(ImMax(v_min, 0.0f), v_max, a);
				}
			}
			else {
				// Linear slider
				new_value = ImLerp(v_min, v_max, clicked_t);
			}

			// Round past decimal precision
			new_value = RoundScalar(new_value, decimal_precision);

			return new_value;
		};

        if (g.IO.MouseClicked[0]) {
			const float min_dist = 4.f;

			float t1 = SliderBehaviorCalcRatioFromValue(*v1, v_min, v_max, power, linear_zero_pos);
			if (!is_horizontal) t1 = 1.0f - t1;
			float p1 = ImLerp(slider_usable_pos_min, slider_usable_pos_max, t1);

			float t2 = SliderBehaviorCalcRatioFromValue(*v2, v_min, v_max, power, linear_zero_pos);
			if (!is_horizontal) t2 = 1.0f - t2;
			float p2 = ImLerp(slider_usable_pos_min, slider_usable_pos_max, t2);

			if (fabsf(mouse_abs_pos - p1) < min_dist) {
				state.min_grab = true;
			}
			else if (fabsf(mouse_abs_pos - p2) < min_dist) {
				state.max_grab = true;
			}
			else if (p1 < mouse_abs_pos && mouse_abs_pos < p2) {
				state.range_grab = true;
				float val = compute_val();
				state.v1_diff = *v1 - val;
				state.v2_diff = *v2 - val;
			}
        } else if (g.IO.MouseDown[0]) {
			if (state.min_grab) {
				*v1 = compute_val();
				value_changed = true;
			}
			else if (state.max_grab) {
				*v2 = compute_val();
				value_changed = true;
			}
			else if (state.range_grab) {
				float val = compute_val();
				float v_diff = *v2 - *v1;

				*v1 = val + state.v1_diff;
				*v2 = val + state.v2_diff;
				*v1 = ImClamp(*v1, v_min, v_max);
				*v2 = ImClamp(*v2, v_min, v_max);

				if (*v1 == v_min) *v2 = *v1 + v_diff;
				if (*v2 == v_max) *v1 = *v2 - v_diff;

				value_changed = true;
			}
        } else {
            state.min_grab = false;
            state.max_grab = false;
            state.range_grab = false;
            ClearActiveID();
        }
    }
    else {
        state.min_grab = false;
        state.max_grab = false;
        state.range_grab = false;
    }

    // Store state
    range_slider_states[id] = state;

    // Calculate slider grab positioning
    float grab_t = SliderBehaviorCalcRatioFromValue(*v1, v_min, v_max, power, linear_zero_pos);

    // Draw
    if (!is_horizontal) grab_t = 1.0f - grab_t;
    float grab_pos = ImLerp(slider_usable_pos_min, slider_usable_pos_max, grab_t);
    ImRect grab_bb1;
    if (is_horizontal)
        grab_bb1 = ImRect(ImVec2(grab_pos - grab_sz * 0.5f, frame_bb.Min.y + grab_padding),
                          ImVec2(grab_pos + grab_sz * 0.5f, frame_bb.Max.y - grab_padding));
    else
        grab_bb1 = ImRect(ImVec2(frame_bb.Min.x + grab_padding, grab_pos - grab_sz * 0.5f),
                          ImVec2(frame_bb.Max.x - grab_padding, grab_pos + grab_sz * 0.5f));
    window->DrawList->AddRectFilled(
        grab_bb1.Min, grab_bb1.Max,
        GetColorU32(state.min_grab ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab),
        style.GrabRounding);

    // Calculate slider grab positioning
    grab_t = SliderBehaviorCalcRatioFromValue(*v2, v_min, v_max, power, linear_zero_pos);

    // Draw
    if (!is_horizontal) grab_t = 1.0f - grab_t;
    grab_pos = ImLerp(slider_usable_pos_min, slider_usable_pos_max, grab_t);
    ImRect grab_bb2;
    if (is_horizontal)
        grab_bb2 = ImRect(ImVec2(grab_pos - grab_sz * 0.5f, frame_bb.Min.y + grab_padding),
                          ImVec2(grab_pos + grab_sz * 0.5f, frame_bb.Max.y - grab_padding));
    else
        grab_bb2 = ImRect(ImVec2(frame_bb.Min.x + grab_padding, grab_pos - grab_sz * 0.5f),
                          ImVec2(frame_bb.Max.x - grab_padding, grab_pos + grab_sz * 0.5f));
    window->DrawList->AddRectFilled(
        grab_bb2.Min, grab_bb2.Max,
        GetColorU32(state.max_grab ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab),
        style.GrabRounding);

    ImRect connector(grab_bb1.Min, grab_bb2.Max);
    connector.Min.x += grab_sz;
    connector.Min.y += grab_sz * 0.3f;
    connector.Max.x -= grab_sz;
    connector.Max.y -= grab_sz * 0.3f;

    window->DrawList->AddRectFilled(connector.Min, connector.Max, GetColorU32(state.range_grab ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab),
                                    style.GrabRounding);

    return value_changed;
}

// ~95% common code with ImGui::SliderFloat
bool RangeSliderFloat(const char* label, float* v1, float* v2, float v_min, float v_max,
                      const char* display_format, float power) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const float w = CalcItemWidth();

    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    const ImRect frame_bb(
        window->DC.CursorPos,
        window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y * 2.0f));
    const ImRect total_bb(
        frame_bb.Min,
        frame_bb.Max +
            ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

    // NB- we don't call ItemSize() yet because we may turn into a text edit box below
    if (!ItemAdd(total_bb, id)) {
        ItemSize(total_bb, style.FramePadding.y);
        return false;
    }

    const bool hovered = IsItemHovered();
    if (hovered) SetHoveredID(id);

    if (!display_format) display_format = "(%.3f, %.3f)";
    int decimal_precision = ParseFormatPrecision(display_format, 3);

    // Tabbing or CTRL-clicking on Slider turns it into an input box
    bool start_text_input = false;
    const bool tab_focus_requested = FocusableItemRegister(window, g.ActiveId == id);
    if (tab_focus_requested || (hovered && g.IO.MouseClicked[0])) {
        SetActiveID(id, window);
        FocusWindow(window);

        if (tab_focus_requested || g.IO.KeyCtrl) {
            start_text_input = true;
            g.ScalarAsInputTextId = 0;
        }
    }
    if (start_text_input || (g.ActiveId == id && g.ScalarAsInputTextId == id))
        return InputScalarAsWidgetReplacement(frame_bb, label, ImGuiDataType_Float, v1, id,
                                              decimal_precision);

    ItemSize(total_bb, style.FramePadding.y);

    // Actual slider behavior + render grab
    const bool value_changed =
        RangeSliderBehavior(frame_bb, id, v1, v2, v_min, v_max, power, decimal_precision, 0);

    // Display value using user-provided display format so user can add prefix/suffix/decorations to
    // the value.
    char value_buf[64];
    const char* value_buf_end =
        value_buf + ImFormatString(value_buf, IM_ARRAYSIZE(value_buf), display_format, *v1, *v2);
    RenderTextClipped(frame_bb.Min, frame_bb.Max, value_buf, value_buf_end, NULL,
                      ImVec2(0.5f, 0.5f));

    if (label_size.x > 0.0f)
        RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x,
                          frame_bb.Min.y + style.FramePadding.y),
                   label);

    return value_changed;
}

}  // namespace ImGui
