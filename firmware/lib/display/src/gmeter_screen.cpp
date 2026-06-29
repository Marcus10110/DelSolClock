#include "gmeter_screen.h"

#include <algorithm>
#include <cmath>

#include "draw_helpers.h"
#include "fonts.h"

namespace display {

void DrawGMeter(Adafruit_GFX* gfx, const GMeterProps& props) {
  Clear(gfx);
  gfx->setFont(&JetBrainsMono_Thin7pt7b);

  // All layout is anchored to the bezel-aware drawable region so it reflows when
  // the bezel insets change. Two graphs occupy the left column; the g-force ring
  // cluster sits in the right column, vertically centered.
  const Rect screen = ScreenRect();

  // draw rings: cluster centered in the right portion of the visible area.
  int center_x = screen.x + screen.w - 50;
  int center_y = screen.y + screen.h / 2;
  constexpr uint16_t line_color = 0xFFFF;
  constexpr uint16_t color = 0xF800;
  constexpr double max_g_ring = 0.6;
  constexpr double max_g_graph = 1.0;
  gfx->drawCircle(center_x, center_y, 50, line_color);
  gfx->drawCircle(center_x, center_y, 33, line_color);
  gfx->drawCircle(center_x, center_y, 16, line_color);
  gfx->drawCircle(center_x, center_y, 7, line_color);

  // draw lines:
  gfx->drawLine(center_x - 50, center_y, center_x + 50, center_y, line_color);
  gfx->drawLine(center_x, center_y - 50, center_x, center_y + 50, line_color);

  // add labels:
  auto drawText = [&](int16_t x, int16_t y, const char* text) {
    gfx->setCursor(x, y + JetBrainsMono_Thin7pt7b.yAdvance);
    gfx->print(text);
  };
  // Labels around the ring cluster, anchored to its center.
  drawText(center_x - 20, center_y - 70, "Brake");
  drawText(center_x - 21, center_y + 47, "Accel");
  // Labels for the left-column graphs, anchored to the visible top-left.
  drawText(screen.x, screen.y + 5, "Brake/Accel");
  drawText(screen.x, screen.y + 63, "Lateral");
  // Ring scale ticks, offset diagonally from the center along a radius.
  drawText(center_x + 7, center_y + 4, ".2");
  drawText(center_x + 17, center_y + 15, ".4");
  drawText(center_x + 30, center_y + 28, ".6");

  // draw red dot.
  int16_t dot_x = center_x + static_cast<int16_t>(props.lateralLive / max_g_ring * 50);
  int16_t dot_y = center_y + static_cast<int16_t>(props.brakeLive / max_g_ring * 50);
  gfx->fillCircle(dot_x, dot_y, 5, color);

  // draw graphs.
  auto drawGraph = [&](int row, const double* history) {
    if (!history) return;
    constexpr int16_t height = 38;
    constexpr int16_t width = 105;
    int16_t x = screen.x;
    int16_t y = screen.y + (row == 0 ? 25 : 83);

    gfx->drawRect(x, y, width, height, line_color);
    // fill in the graph (width-2) with HistorySize elements; 1px at a time.
    for (int i = 1; i < width - 2; i++) {
      int index = (i - 1) * static_cast<int>(GMeterProps::HistorySize) / (width - 2);
      double element = history[index];
      if (std::isinf(element)) {
        continue;
      }
      int16_t graph_y = static_cast<int16_t>(
          (element + max_g_graph) / (2 * max_g_graph) * (height - 2));
      graph_y = std::clamp<int16_t>(graph_y, int16_t(0), height - 2);
      gfx->drawPixel(x - i + width - 2, y + height - 2 - graph_y, color);
    }
  };

  drawGraph(0, props.brakeHistory);
  drawGraph(1, props.lateralHistory);
}

}  // namespace display
