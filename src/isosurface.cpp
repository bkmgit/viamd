#include "isosurface.h"

bool insert(IsoSurface& surface, float value, const vec4& color) {
    if (surface.count >= IsoSurface::MaxCount) return false;
    surface.values[surface.count] = value;
    surface.colors[surface.count] = color;
    ++surface.count;
    return true;
}

void clear(IsoSurface& surface) {
    surface = {};
}

void sort(IsoSurface& surface) {
    for (int i = 0; i < surface.count - 1; ++i) {
        for (int j = i + 1; j < surface.count; ++j) {
            if (surface.values[j] < surface.values[i]) {
                const auto tmp_val = surface.values[i];
                const auto tmp_col = surface.colors[i];
                surface.values[i] = surface.values[j];
                surface.colors[i] = surface.colors[j];
                surface.values[j] = tmp_val;
                surface.colors[j] = tmp_col;
            }
        }
    }
}
