#pragma once

#include "cheat_features.h"

namespace Menu {

// Initialize menu system
void Initialize();

// Render menu UI
void Render();

// Handle input
void HandleInput();

// Toggle menu visibility
void Toggle();

// Check if menu is open
bool IsOpen();

// Style configuration
void ApplyStyle();

} // namespace Menu