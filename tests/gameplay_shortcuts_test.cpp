#include "gameplay_shortcuts.h"
#include <cassert>
#include <cstdio>

int main() {
    GameplayShortcuts keys;
    unsigned short pads[4] = {0, 0, 0, 0};
    typedef GameplayShortcuts K;
    // Either button order opens the menu once, including with RS held.
    pads[0] = 0x10;
    assert(keys.Update(pads, true) == K::None);
    pads[0] |= 0x20 | 0x80;
    assert(keys.Update(pads, true) == K::OpenMenu);
    assert(keys.Update(pads, true) == K::None);
    assert(keys.Update(pads, false) == K::None);
    assert(keys.Update(pads, true) == K::None);
    pads[0] = 0;
    keys.Update(pads, true);
    pads[0] = 0x20;
    assert(keys.Update(pads, true) == K::None);
    pads[0] |= 0x10;
    assert(keys.Update(pads, true) == K::OpenMenu);
    pads[0] = 0;
    keys.Update(pads, true);
    // Buttons from different controllers must not combine into a chord.
    pads[0] = 0x10; pads[1] = 0x20;
    assert(keys.Update(pads, true) == K::None);
    pads[1] |= 0x10;
    assert(keys.Update(pads, true) == K::OpenMenu);
    pads[0] = pads[1] = 0;
    keys.Update(pads, true);
    // All four controller slots work, including after a disconnect/reconnect.
    for (int i = 0; i < 4; ++i) {
        pads[i] = 0x30;
        assert(keys.Update(pads, true) == K::OpenMenu);
        pads[i] = 0;
        assert(keys.Update(pads, true) == K::None);
    }
    // Console wins over screenshot, and releasing LS does not capture.
    pads[0] = 0xc0;
    assert(keys.Update(pads, true) == K::ToggleConsole);
    assert(keys.Update(pads, true) == K::None);
    pads[0] = 0x80;
    assert(keys.Update(pads, true) == K::None);
    pads[0] = 0;
    keys.Update(pads, true);
    pads[0] = 0x80;
    assert(keys.Update(pads, true) == K::Screenshot);
    assert(keys.Update(pads, true) == K::None);
    pads[0] = 0;
    keys.Update(pads, false);
    pads[0] = 0xf0;
    assert(keys.Update(pads, false) == K::None);
    assert(keys.Update(pads, true) == K::None);
    pads[0] = 0;
    keys.Update(pads, true);
    pads[0] = 0xf0;
    assert(keys.Update(pads, true) == K::OpenMenu);
    // Exhaust the input masks: menu chords can never become screenshots.
    for (unsigned int mask = 0; mask < 65536; ++mask) {
        GameplayShortcuts fresh;
        pads[0] = static_cast<unsigned short>(mask);
        if ((mask & 0x30) == 0x30)
            assert(fresh.Update(pads, true) == K::OpenMenu);
    }
    puts("Gameplay shortcut tests passed");
}
