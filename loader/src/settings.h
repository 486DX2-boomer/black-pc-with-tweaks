// Port settings remembered between launches (black.ini in the game root): what PC SETTINGS and the F-keys
// change. Command-line flags win over the file for that session. Test runs (--background) neither read nor
// write it.
#pragma once

void settings_mark_cli(const char *key); // a command-line flag set this setting: the file must not override it
void settings_load();                    // after the command line, once the root folder is current
void settings_save();
