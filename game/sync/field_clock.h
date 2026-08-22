#pragma once

// Install Toy Story 2's field-clock ownership at the game/framework seam.
// The generated-substrate build installs one scoped override; the compile-only
// seam build deliberately has no generated function to override.
void ts2_field_clock_install();
