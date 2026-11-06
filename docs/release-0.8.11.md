# Version 0.8.11

This release addresses [issue #9](https://github.com/frankischilling/fce360-enhanced/issues/9):
START + BACK should open the in-game menu, rather than take a screenshot.

Menu, screenshot, and Lua console shortcuts now share one handler. START + BACK
has priority over both other actions and selects the embedded OSD tab directly.
The buttons must be held on the same controller, and either can be pressed first.
Release the combo before opening the menu again. Right-stick click remains the
screenshot shortcut; both stick clicks open the Lua console.

The handler tracks whether the gameplay tab is active. The OSD also draws the
game, so the shared rendering flag cannot identify which tab owns input. Menu
input no longer triggers gameplay shortcuts, and held buttons do not trigger a
new action when you resume. The duplicate Lua console handler has been removed.

Copy `fceux.xex` and the included `media` folder together. Keep your existing
configuration, scripts, ROMs, BIOS files, screenshots, saves, and states.

Host tests cover button order, held chords, menu priority across all 65,536
button masks, controller slots, and screenshot/console separation. A scene test
compiles the production gameplay scene against an XUI substitute and checks OSD
navigation and input suppression while the menu is active. Existing screenshot
worker and PNG tests remain part of CI.

The Xbox 360 Release_LTCG build uses Visual Studio 2008 SP1 and XDK 2.0.9328.0.
These checks do not reproduce the report on console hardware. Issue #9 stays
open for confirmation with this release.
