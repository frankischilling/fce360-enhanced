#ifndef GAMEPLAY_SHORTCUTS_H
#define GAMEPLAY_SHORTCUTS_H

// XInput button masks. Kept independent of the SDK for host regression tests.
class GameplayShortcuts {
public:
    enum Action { None, OpenMenu, ToggleConsole, Screenshot };
    GameplayShortcuts() {
        for (int i = 0; i < 4; ++i) m_previous[i] = 0;
    }

    Action Update(const unsigned short buttons[4], bool gameplayActive) {
        const unsigned short menu = 0x0010 | 0x0020; // START + BACK
        const unsigned short sticks = 0x0040 | 0x0080; // LS + RS
        bool openMenu = false, console = false, screenshot = false;
        bool menuHeld = false, consoleHeld = false;
        for (int i = 0; i < 4; ++i) {
            const unsigned short now = buttons[i], prev = m_previous[i];
            const bool chord = (now & menu) == menu;
            const bool thumbChord = (now & sticks) == sticks;
            menuHeld = menuHeld || chord;
            consoleHeld = consoleHeld || thumbChord;
            openMenu = openMenu || (chord && (prev & menu) != menu);
            console = console || (thumbChord && (prev & sticks) != sticks);
            // Releasing LS from a console chord must not take a screenshot.
            screenshot = screenshot || ((now & 0x0080) && !(prev & 0x0080));
            m_previous[i] = now;
        }
        // Track held buttons in menus too, preventing a new action on resume.
        if (!gameplayActive) return None;
        if (menuHeld) return openMenu ? OpenMenu : None;
        if (consoleHeld) return console ? ToggleConsole : None;
        return screenshot ? Screenshot : None;
    }

private:
    unsigned short m_previous[4];
};
#endif
