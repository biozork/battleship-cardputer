// Sound effects, as short sequences of tones on the built-in speaker.
#pragma once

namespace Sound {
void toggle();  // the m key: sound on/off
bool on();
void place();
void bad();
void link();
void lost();
void turn();
}  // namespace Sound
