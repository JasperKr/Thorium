#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Keyboard {
enum class Scancode : uint8_t {
  SCANCODE_UNKNOWN,

  SCANCODE_A,
  SCANCODE_B,
  SCANCODE_C,
  SCANCODE_D,
  SCANCODE_E,
  SCANCODE_F,
  SCANCODE_G,
  SCANCODE_H,
  SCANCODE_I,
  SCANCODE_J,
  SCANCODE_K,
  SCANCODE_L,
  SCANCODE_M,
  SCANCODE_N,
  SCANCODE_O,
  SCANCODE_P,
  SCANCODE_Q,
  SCANCODE_R,
  SCANCODE_S,
  SCANCODE_T,
  SCANCODE_U,
  SCANCODE_V,
  SCANCODE_W,
  SCANCODE_X,
  SCANCODE_Y,
  SCANCODE_Z,

  SCANCODE_1,
  SCANCODE_2,
  SCANCODE_3,
  SCANCODE_4,
  SCANCODE_5,
  SCANCODE_6,
  SCANCODE_7,
  SCANCODE_8,
  SCANCODE_9,
  SCANCODE_0,

  SCANCODE_RETURN,
  SCANCODE_ESCAPE,
  SCANCODE_BACKSPACE,
  SCANCODE_TAB,
  SCANCODE_SPACE,

  SCANCODE_MINUS,
  SCANCODE_EQUALS,
  SCANCODE_LEFTBRACKET,
  SCANCODE_RIGHTBRACKET,
  SCANCODE_BACKSLASH,
  SCANCODE_NONUSHASH,
  SCANCODE_SEMICOLON,
  SCANCODE_APOSTROPHE,
  SCANCODE_GRAVE,
  SCANCODE_COMMA,
  SCANCODE_PERIOD,
  SCANCODE_SLASH,

  SCANCODE_CAPSLOCK,

  SCANCODE_F1,
  SCANCODE_F2,
  SCANCODE_F3,
  SCANCODE_F4,
  SCANCODE_F5,
  SCANCODE_F6,
  SCANCODE_F7,
  SCANCODE_F8,
  SCANCODE_F9,
  SCANCODE_F10,
  SCANCODE_F11,
  SCANCODE_F12,

  SCANCODE_PRINTSCREEN,
  SCANCODE_SCROLLLOCK,
  SCANCODE_PAUSE,
  SCANCODE_INSERT,
  SCANCODE_HOME,
  SCANCODE_PAGEUP,
  SCANCODE_DELETE,
  SCANCODE_END,
  SCANCODE_PAGEDOWN,
  SCANCODE_RIGHT,
  SCANCODE_LEFT,
  SCANCODE_DOWN,
  SCANCODE_UP,

  SCANCODE_NUMLOCKCLEAR,
  SCANCODE_KP_DIVIDE,
  SCANCODE_KP_MULTIPLY,
  SCANCODE_KP_MINUS,
  SCANCODE_KP_PLUS,
  SCANCODE_KP_ENTER,
  SCANCODE_KP_1,
  SCANCODE_KP_2,
  SCANCODE_KP_3,
  SCANCODE_KP_4,
  SCANCODE_KP_5,
  SCANCODE_KP_6,
  SCANCODE_KP_7,
  SCANCODE_KP_8,
  SCANCODE_KP_9,
  SCANCODE_KP_0,
  SCANCODE_KP_PERIOD,

  SCANCODE_NONUSBACKSLASH,
  SCANCODE_APPLICATION,
  SCANCODE_POWER,
  SCANCODE_KP_EQUALS,
  SCANCODE_F13,
  SCANCODE_F14,
  SCANCODE_F15,
  SCANCODE_F16,
  SCANCODE_F17,
  SCANCODE_F18,
  SCANCODE_F19,
  SCANCODE_F20,
  SCANCODE_F21,
  SCANCODE_F22,
  SCANCODE_F23,
  SCANCODE_F24,
  SCANCODE_EXECUTE,
  SCANCODE_HELP,
  SCANCODE_MENU,
  SCANCODE_SELECT,
  SCANCODE_STOP,
  SCANCODE_AGAIN,
  SCANCODE_UNDO,
  SCANCODE_CUT,
  SCANCODE_COPY,
  SCANCODE_PASTE,
  SCANCODE_FIND,
  SCANCODE_MUTE,
  SCANCODE_VOLUMEUP,
  SCANCODE_VOLUMEDOWN,
  SCANCODE_KP_COMMA,
  SCANCODE_KP_EQUALSAS400,

  SCANCODE_INTERNATIONAL1,
  SCANCODE_INTERNATIONAL2,
  SCANCODE_INTERNATIONAL3,
  SCANCODE_INTERNATIONAL4,
  SCANCODE_INTERNATIONAL5,
  SCANCODE_INTERNATIONAL6,
  SCANCODE_INTERNATIONAL7,
  SCANCODE_INTERNATIONAL8,
  SCANCODE_INTERNATIONAL9,
  SCANCODE_LANG1,
  SCANCODE_LANG2,
  SCANCODE_LANG3,
  SCANCODE_LANG4,
  SCANCODE_LANG5,
  SCANCODE_LANG6,
  SCANCODE_LANG7,
  SCANCODE_LANG8,
  SCANCODE_LANG9,

  SCANCODE_ALTERASE,
  SCANCODE_SYSREQ,
  SCANCODE_CANCEL,
  SCANCODE_CLEAR,
  SCANCODE_PRIOR,
  SCANCODE_RETURN2,
  SCANCODE_SEPARATOR,
  SCANCODE_OUT,
  SCANCODE_OPER,
  SCANCODE_CLEARAGAIN,
  SCANCODE_CRSEL,
  SCANCODE_EXSEL,

  SCANCODE_KP_00,
  SCANCODE_KP_000,
  SCANCODE_THOUSANDSSEPARATOR,
  SCANCODE_DECIMALSEPARATOR,
  SCANCODE_CURRENCYUNIT,
  SCANCODE_CURRENCYSUBUNIT,
  SCANCODE_KP_LEFTPAREN,
  SCANCODE_KP_RIGHTPAREN,
  SCANCODE_KP_LEFTBRACE,
  SCANCODE_KP_RIGHTBRACE,
  SCANCODE_KP_TAB,
  SCANCODE_KP_BACKSPACE,
  SCANCODE_KP_A,
  SCANCODE_KP_B,
  SCANCODE_KP_C,
  SCANCODE_KP_D,
  SCANCODE_KP_E,
  SCANCODE_KP_F,
  SCANCODE_KP_XOR,
  SCANCODE_KP_POWER,
  SCANCODE_KP_PERCENT,
  SCANCODE_KP_LESS,
  SCANCODE_KP_GREATER,
  SCANCODE_KP_AMPERSAND,
  SCANCODE_KP_DBLAMPERSAND,
  SCANCODE_KP_VERTICALBAR,
  SCANCODE_KP_DBLVERTICALBAR,
  SCANCODE_KP_COLON,
  SCANCODE_KP_HASH,
  SCANCODE_KP_SPACE,
  SCANCODE_KP_AT,
  SCANCODE_KP_EXCLAM,
  SCANCODE_KP_MEMSTORE,
  SCANCODE_KP_MEMRECALL,
  SCANCODE_KP_MEMCLEAR,
  SCANCODE_KP_MEMADD,
  SCANCODE_KP_MEMSUBTRACT,
  SCANCODE_KP_MEMMULTIPLY,
  SCANCODE_KP_MEMDIVIDE,
  SCANCODE_KP_PLUSMINUS,
  SCANCODE_KP_CLEAR,
  SCANCODE_KP_CLEARENTRY,
  SCANCODE_KP_BINARY,
  SCANCODE_KP_OCTAL,
  SCANCODE_KP_DECIMAL,
  SCANCODE_KP_HEXADECIMAL,

  SCANCODE_LCTRL,
  SCANCODE_LSHIFT,
  SCANCODE_LALT,
  SCANCODE_LGUI,
  SCANCODE_RCTRL,
  SCANCODE_RSHIFT,
  SCANCODE_RALT,
  SCANCODE_RGUI,

  SCANCODE_MODE,

  SCANCODE_AUDIONEXT,
  SCANCODE_AUDIOPREV,
  SCANCODE_AUDIOSTOP,
  SCANCODE_AUDIOPLAY,
  SCANCODE_AUDIOMUTE,
  SCANCODE_MEDIASELECT,
  SCANCODE_AC_SEARCH,
  SCANCODE_AC_HOME,
  SCANCODE_AC_BACK,
  SCANCODE_AC_FORWARD,
  SCANCODE_AC_STOP,
  SCANCODE_AC_REFRESH,
  SCANCODE_AC_BOOKMARKS,

  SCANCODE_EJECT,
  SCANCODE_SLEEP,

  SCANCODE_MAX_ENUM
};

/**
 * Modifier keys. These are special keys that temporarily modify the normal
 * function of a button when active.
 **/
enum class ModifierKey : uint8_t {
  MODKEY_NUMLOCK,
  MODKEY_CAPSLOCK,
  MODKEY_SCROLLLOCK,
  MODKEY_MODE,
  MODKEY_MAX_ENUM
};

constexpr int MaxScancode = 512;

inline auto Register(std::vector<std::string> &vector, Scancode scancode,
                     const std::string &name) -> void {
  vector.at(static_cast<uint32_t>(scancode)) = name;
}

// NOLINTNEXTLINE
static std::vector<std::string> ScancodeToString =
    []() -> std::vector<std::string> {
  std::vector<std::string> vec(MaxScancode + 1);
  Register(vec, Scancode::SCANCODE_UNKNOWN, "unknown");

  Register(vec, Scancode::SCANCODE_A, "a");
  Register(vec, Scancode::SCANCODE_B, "b");
  Register(vec, Scancode::SCANCODE_C, "c");
  Register(vec, Scancode::SCANCODE_D, "d");
  Register(vec, Scancode::SCANCODE_E, "e");
  Register(vec, Scancode::SCANCODE_F, "f");
  Register(vec, Scancode::SCANCODE_G, "g");
  Register(vec, Scancode::SCANCODE_H, "h");
  Register(vec, Scancode::SCANCODE_I, "i");
  Register(vec, Scancode::SCANCODE_J, "j");
  Register(vec, Scancode::SCANCODE_K, "k");
  Register(vec, Scancode::SCANCODE_L, "l");
  Register(vec, Scancode::SCANCODE_M, "m");
  Register(vec, Scancode::SCANCODE_N, "n");
  Register(vec, Scancode::SCANCODE_O, "o");
  Register(vec, Scancode::SCANCODE_P, "p");
  Register(vec, Scancode::SCANCODE_Q, "q");
  Register(vec, Scancode::SCANCODE_R, "r");
  Register(vec, Scancode::SCANCODE_S, "s");
  Register(vec, Scancode::SCANCODE_T, "t");
  Register(vec, Scancode::SCANCODE_U, "u");
  Register(vec, Scancode::SCANCODE_V, "v");
  Register(vec, Scancode::SCANCODE_W, "w");
  Register(vec, Scancode::SCANCODE_X, "x");
  Register(vec, Scancode::SCANCODE_Y, "y");
  Register(vec, Scancode::SCANCODE_Z, "z");

  Register(vec, Scancode::SCANCODE_1, "1");
  Register(vec, Scancode::SCANCODE_2, "2");
  Register(vec, Scancode::SCANCODE_3, "3");
  Register(vec, Scancode::SCANCODE_4, "4");
  Register(vec, Scancode::SCANCODE_5, "5");
  Register(vec, Scancode::SCANCODE_6, "6");
  Register(vec, Scancode::SCANCODE_7, "7");
  Register(vec, Scancode::SCANCODE_8, "8");
  Register(vec, Scancode::SCANCODE_9, "9");
  Register(vec, Scancode::SCANCODE_0, "0");

  Register(vec, Scancode::SCANCODE_RETURN, "return");
  Register(vec, Scancode::SCANCODE_ESCAPE, "escape");
  Register(vec, Scancode::SCANCODE_BACKSPACE, "backspace");
  Register(vec, Scancode::SCANCODE_TAB, "tab");
  Register(vec, Scancode::SCANCODE_SPACE, "space");

  // Punctuation and symbols
  Register(vec, Scancode::SCANCODE_MINUS, "-");
  Register(vec, Scancode::SCANCODE_EQUALS, "=");
  Register(vec, Scancode::SCANCODE_LEFTBRACKET, "[");
  Register(vec, Scancode::SCANCODE_RIGHTBRACKET, "]");
  Register(vec, Scancode::SCANCODE_BACKSLASH, "\\");
  Register(vec, Scancode::SCANCODE_NONUSHASH, "#");
  Register(vec, Scancode::SCANCODE_SEMICOLON, ";");
  Register(vec, Scancode::SCANCODE_APOSTROPHE, "'");
  Register(vec, Scancode::SCANCODE_GRAVE, "`");
  Register(vec, Scancode::SCANCODE_COMMA, ",");
  Register(vec, Scancode::SCANCODE_PERIOD, ".");
  Register(vec, Scancode::SCANCODE_SLASH, "/");
  // Lock keys
  Register(vec, Scancode::SCANCODE_CAPSLOCK, "capslock");

  // Function keys
  Register(vec, Scancode::SCANCODE_F1, "f1");
  Register(vec, Scancode::SCANCODE_F2, "f2");
  Register(vec, Scancode::SCANCODE_F3, "f3");
  Register(vec, Scancode::SCANCODE_F4, "f4");
  Register(vec, Scancode::SCANCODE_F5, "f5");
  Register(vec, Scancode::SCANCODE_F6, "f6");
  Register(vec, Scancode::SCANCODE_F7, "f7");
  Register(vec, Scancode::SCANCODE_F8, "f8");
  Register(vec, Scancode::SCANCODE_F9, "f9");
  Register(vec, Scancode::SCANCODE_F10, "f10");
  Register(vec, Scancode::SCANCODE_F11, "f11");
  Register(vec, Scancode::SCANCODE_F12, "f12");

  // System keys
  Register(vec, Scancode::SCANCODE_PRINTSCREEN, "printscreen");
  Register(vec, Scancode::SCANCODE_SCROLLLOCK, "scrolllock");
  Register(vec, Scancode::SCANCODE_PAUSE, "pause");
  Register(vec, Scancode::SCANCODE_INSERT, "insert");
  Register(vec, Scancode::SCANCODE_HOME, "home");
  Register(vec, Scancode::SCANCODE_PAGEUP, "pageup");
  Register(vec, Scancode::SCANCODE_DELETE, "delete");
  Register(vec, Scancode::SCANCODE_END, "end");
  Register(vec, Scancode::SCANCODE_PAGEDOWN, "pagedown");

  // Arrow keys
  Register(vec, Scancode::SCANCODE_RIGHT, "right");
  Register(vec, Scancode::SCANCODE_LEFT, "left");
  Register(vec, Scancode::SCANCODE_DOWN, "down");
  Register(vec, Scancode::SCANCODE_UP, "up");

  // Keypad
  Register(vec, Scancode::SCANCODE_NUMLOCKCLEAR, "numlock");
  Register(vec, Scancode::SCANCODE_KP_DIVIDE, "kp_/");
  Register(vec, Scancode::SCANCODE_KP_MULTIPLY, "kp_*");
  Register(vec, Scancode::SCANCODE_KP_MINUS, "kp_-");
  Register(vec, Scancode::SCANCODE_KP_PLUS, "kp_+");
  Register(vec, Scancode::SCANCODE_KP_ENTER, "kp_enter");
  Register(vec, Scancode::SCANCODE_KP_1, "kp_1");
  Register(vec, Scancode::SCANCODE_KP_2, "kp_2");
  Register(vec, Scancode::SCANCODE_KP_3, "kp_3");
  Register(vec, Scancode::SCANCODE_KP_4, "kp_4");
  Register(vec, Scancode::SCANCODE_KP_5, "kp_5");
  Register(vec, Scancode::SCANCODE_KP_6, "kp_6");
  Register(vec, Scancode::SCANCODE_KP_7, "kp_7");
  Register(vec, Scancode::SCANCODE_KP_8, "kp_8");
  Register(vec, Scancode::SCANCODE_KP_9, "kp_9");
  Register(vec, Scancode::SCANCODE_KP_0, "kp_0");
  Register(vec, Scancode::SCANCODE_KP_PERIOD, "kp_.");

  // Additional keys
  Register(vec, Scancode::SCANCODE_NONUSBACKSLASH, "nonusbackslash");
  Register(vec, Scancode::SCANCODE_APPLICATION, "application");
  Register(vec, Scancode::SCANCODE_POWER, "power");
  Register(vec, Scancode::SCANCODE_KP_EQUALS, "kp_equals");

  // Extended function keys
  Register(vec, Scancode::SCANCODE_F13, "f13");
  Register(vec, Scancode::SCANCODE_F14, "f14");
  Register(vec, Scancode::SCANCODE_F15, "f15");
  Register(vec, Scancode::SCANCODE_F16, "f16");
  Register(vec, Scancode::SCANCODE_F17, "f17");
  Register(vec, Scancode::SCANCODE_F18, "f18");
  Register(vec, Scancode::SCANCODE_F19, "f19");
  Register(vec, Scancode::SCANCODE_F20, "f20");
  Register(vec, Scancode::SCANCODE_F21, "f21");
  Register(vec, Scancode::SCANCODE_F22, "f22");
  Register(vec, Scancode::SCANCODE_F23, "f23");
  Register(vec, Scancode::SCANCODE_F24, "f24");

  // System functions
  Register(vec, Scancode::SCANCODE_EXECUTE, "execute");
  Register(vec, Scancode::SCANCODE_HELP, "help");
  Register(vec, Scancode::SCANCODE_MENU, "menu");
  Register(vec, Scancode::SCANCODE_SELECT, "select");
  Register(vec, Scancode::SCANCODE_STOP, "stop");
  Register(vec, Scancode::SCANCODE_AGAIN, "again");
  Register(vec, Scancode::SCANCODE_UNDO, "undo");
  Register(vec, Scancode::SCANCODE_CUT, "cut");
  Register(vec, Scancode::SCANCODE_COPY, "copy");
  Register(vec, Scancode::SCANCODE_PASTE, "paste");
  Register(vec, Scancode::SCANCODE_FIND, "find");

  // Audio controls
  Register(vec, Scancode::SCANCODE_MUTE, "mute");
  Register(vec, Scancode::SCANCODE_VOLUMEUP, "volumeup");
  Register(vec, Scancode::SCANCODE_VOLUMEDOWN, "volumedown");

  // Additional keypad
  Register(vec, Scancode::SCANCODE_KP_COMMA, "kp_,");
  Register(vec, Scancode::SCANCODE_KP_EQUALSAS400, "kp_=");

  // International keys
  Register(vec, Scancode::SCANCODE_INTERNATIONAL1, "international1");
  Register(vec, Scancode::SCANCODE_INTERNATIONAL2, "international2");
  Register(vec, Scancode::SCANCODE_INTERNATIONAL3, "international3");
  Register(vec, Scancode::SCANCODE_INTERNATIONAL4, "international4");
  Register(vec, Scancode::SCANCODE_INTERNATIONAL5, "international5");
  Register(vec, Scancode::SCANCODE_INTERNATIONAL6, "international6");
  Register(vec, Scancode::SCANCODE_INTERNATIONAL7, "international7");
  Register(vec, Scancode::SCANCODE_INTERNATIONAL8, "international8");
  Register(vec, Scancode::SCANCODE_INTERNATIONAL9, "international9");

  // Language keys
  Register(vec, Scancode::SCANCODE_LANG1, "lang1");
  Register(vec, Scancode::SCANCODE_LANG2, "lang2");
  Register(vec, Scancode::SCANCODE_LANG3, "lang3");
  Register(vec, Scancode::SCANCODE_LANG4, "lang4");
  Register(vec, Scancode::SCANCODE_LANG5, "lang5");
  Register(vec, Scancode::SCANCODE_LANG6, "lang6");
  Register(vec, Scancode::SCANCODE_LANG7, "lang7");
  Register(vec, Scancode::SCANCODE_LANG8, "lang8");
  Register(vec, Scancode::SCANCODE_LANG9, "lang9");

  // System functions
  Register(vec, Scancode::SCANCODE_ALTERASE, "alterase");
  Register(vec, Scancode::SCANCODE_SYSREQ, "sysreq");
  Register(vec, Scancode::SCANCODE_CANCEL, "cancel");
  Register(vec, Scancode::SCANCODE_CLEAR, "clear");
  Register(vec, Scancode::SCANCODE_PRIOR, "prior");
  Register(vec, Scancode::SCANCODE_RETURN2, "return2");
  Register(vec, Scancode::SCANCODE_SEPARATOR, "separator");
  Register(vec, Scancode::SCANCODE_OUT, "out");
  Register(vec, Scancode::SCANCODE_OPER, "oper");
  Register(vec, Scancode::SCANCODE_CLEARAGAIN, "clearagain");
  Register(vec, Scancode::SCANCODE_CRSEL, "crsel");
  Register(vec, Scancode::SCANCODE_EXSEL, "exsel");

  // Extended keypad
  Register(vec, Scancode::SCANCODE_KP_00, "kp_00");
  Register(vec, Scancode::SCANCODE_KP_000, "kp_000");
  Register(vec, Scancode::SCANCODE_THOUSANDSSEPARATOR, "thousandsseparator");
  Register(vec, Scancode::SCANCODE_DECIMALSEPARATOR, "decimalseparator");
  Register(vec, Scancode::SCANCODE_CURRENCYUNIT, "currencyunit");
  Register(vec, Scancode::SCANCODE_CURRENCYSUBUNIT, "currencysubunit");
  Register(vec, Scancode::SCANCODE_KP_LEFTPAREN, "kp_(");
  Register(vec, Scancode::SCANCODE_KP_RIGHTPAREN, "kp_)");
  Register(vec, Scancode::SCANCODE_KP_LEFTBRACE, "kp_{");
  Register(vec, Scancode::SCANCODE_KP_RIGHTBRACE, "kp_}");
  Register(vec, Scancode::SCANCODE_KP_TAB, "kp_tab");
  Register(vec, Scancode::SCANCODE_KP_BACKSPACE, "kp_backspace");
  Register(vec, Scancode::SCANCODE_KP_A, "kp_a");
  Register(vec, Scancode::SCANCODE_KP_B, "kp_b");
  Register(vec, Scancode::SCANCODE_KP_C, "kp_c");
  Register(vec, Scancode::SCANCODE_KP_D, "kp_d");
  Register(vec, Scancode::SCANCODE_KP_E, "kp_e");
  Register(vec, Scancode::SCANCODE_KP_F, "kp_f");
  Register(vec, Scancode::SCANCODE_KP_XOR, "kp_^");
  Register(vec, Scancode::SCANCODE_KP_POWER, "kp_**");
  Register(vec, Scancode::SCANCODE_KP_PERCENT, "kp_%");
  Register(vec, Scancode::SCANCODE_KP_LESS, "kp_<");
  Register(vec, Scancode::SCANCODE_KP_GREATER, "kp_>");
  Register(vec, Scancode::SCANCODE_KP_AMPERSAND, "kp_&");
  Register(vec, Scancode::SCANCODE_KP_DBLAMPERSAND, "kp_&&");
  Register(vec, Scancode::SCANCODE_KP_VERTICALBAR, "kp_|");
  Register(vec, Scancode::SCANCODE_KP_DBLVERTICALBAR, "kp_||");
  Register(vec, Scancode::SCANCODE_KP_COLON, "kp_:");
  Register(vec, Scancode::SCANCODE_KP_HASH, "kp_#");
  Register(vec, Scancode::SCANCODE_KP_SPACE, "kp_ ");
  Register(vec, Scancode::SCANCODE_KP_AT, "kp_@");
  Register(vec, Scancode::SCANCODE_KP_EXCLAM, "kp_!");

  // Keypad memory functions
  Register(vec, Scancode::SCANCODE_KP_MEMSTORE, "kp_memstore");
  Register(vec, Scancode::SCANCODE_KP_MEMRECALL, "kp_memrecall");
  Register(vec, Scancode::SCANCODE_KP_MEMCLEAR, "kp_memclear");
  Register(vec, Scancode::SCANCODE_KP_MEMADD, "kp_memadd");
  Register(vec, Scancode::SCANCODE_KP_MEMSUBTRACT, "kp_memsubtract");
  Register(vec, Scancode::SCANCODE_KP_MEMMULTIPLY, "kp_memmultiply");
  Register(vec, Scancode::SCANCODE_KP_MEMDIVIDE, "kp_memdivide");
  Register(vec, Scancode::SCANCODE_KP_PLUSMINUS, "kp_±");
  Register(vec, Scancode::SCANCODE_KP_CLEAR, "kp_clear");
  Register(vec, Scancode::SCANCODE_KP_CLEARENTRY, "kp_clearentry");

  // Keypad number systems
  Register(vec, Scancode::SCANCODE_KP_BINARY, "kp_binary");
  Register(vec, Scancode::SCANCODE_KP_OCTAL, "kp_octal");
  Register(vec, Scancode::SCANCODE_KP_DECIMAL, "kp_decimal");
  Register(vec, Scancode::SCANCODE_KP_HEXADECIMAL, "kp_hexadecimal");

  // Modifier keys
  Register(vec, Scancode::SCANCODE_LCTRL, "lctrl");
  Register(vec, Scancode::SCANCODE_LSHIFT, "lshift");
  Register(vec, Scancode::SCANCODE_LALT, "lalt");
  Register(vec, Scancode::SCANCODE_LGUI, "lgui");
  Register(vec, Scancode::SCANCODE_RCTRL, "rctrl");
  Register(vec, Scancode::SCANCODE_RSHIFT, "rshift");
  Register(vec, Scancode::SCANCODE_RALT, "ralt");
  Register(vec, Scancode::SCANCODE_RGUI, "rgui");

  // Mode key
  Register(vec, Scancode::SCANCODE_MODE, "mode");

  // Media keys
  Register(vec, Scancode::SCANCODE_AUDIONEXT, "audionext");
  Register(vec, Scancode::SCANCODE_AUDIOPREV, "audioprev");
  Register(vec, Scancode::SCANCODE_AUDIOSTOP, "audiostop");
  Register(vec, Scancode::SCANCODE_AUDIOPLAY, "audioplay");
  Register(vec, Scancode::SCANCODE_AUDIOMUTE, "audiomute");
  Register(vec, Scancode::SCANCODE_MEDIASELECT, "mediaselect");

  // Application control keys
  Register(vec, Scancode::SCANCODE_AC_SEARCH, "ac_search");
  Register(vec, Scancode::SCANCODE_AC_HOME, "ac_home");
  Register(vec, Scancode::SCANCODE_AC_BACK, "ac_back");
  Register(vec, Scancode::SCANCODE_AC_FORWARD, "ac_forward");
  Register(vec, Scancode::SCANCODE_AC_STOP, "ac_stop");
  Register(vec, Scancode::SCANCODE_AC_REFRESH, "ac_refresh");
  Register(vec, Scancode::SCANCODE_AC_BOOKMARKS, "ac_bookmarks");

  // System keys
  Register(vec, Scancode::SCANCODE_EJECT, "eject");
  Register(vec, Scancode::SCANCODE_SLEEP, "sleep");

  return vec;
}();

} // namespace Keyboard