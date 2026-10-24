// Keyboard input: turns the Cardputer's raw key state into simple key events.
#pragma once

enum class Key { None, Up, Down, Left, Right, Ok, Del, Tab, Esc, Char };

struct KeyEvent {
  Key key;
  char ch;  // the letter for Key::Char (always lowercase), otherwise 0
};

// Call once per loop, after M5Cardputer.update(). Returns Key::None when
// nothing new happened. Holding an arrow key repeats it.
KeyEvent pollKey();
