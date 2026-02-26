#pragma once

#include <string>
#include <vector>

struct AppState;
void renderLeftPanel(AppState& state);
void runConvertAllFromUi(AppState& state);
void runConvertForPaths(AppState& state, const std::vector<std::string>& inputPaths);
