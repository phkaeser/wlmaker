# FIXME: Parser for desktop files

* https://specifications.freedesktop.org/desktop-entry/latest/
* https://github.com/benhoyt/inih

# References from wlmmenugen

parse all files

Only parse what's in "[Desktop Entry]", ignore all else

Consider only: (skip others)
Type=Application

Use:
- Name (localized -> see the "localized values for keys" in entry spec.
- TryExec
- Exec (can have % with fFuUdDnNickvm => skip these; and: un-escape for ", `, $, \
- Path
- store if Terminal=true
- use first matching of Categories  (c1;c2;c3)
  => and lookup using table below.
  => fold all else into "Other"

Skip:
- NoDisplay=true
- Hidden=true







category lookup:

    p = strtok(category, ";");
    while (p) {     /* get a known category */
        if (strcmp(p, "AudioVideo") == 0) {
            snprintf(buf, sizeof(buf), "%s", _("Audio & Video"));
            break;
        } else if (strcmp(p, "Audio") == 0) {
            snprintf(buf, sizeof(buf), "%s", _("Audio"));
            break;
        } else if (strcmp(p, "Video") == 0) {
            snprintf(buf, sizeof(buf), "%s", _("Video"));
            break;
        } else if (strcmp(p, "Development") == 0) {
            snprintf(buf, sizeof(buf), "%s", _("Development"));
            break;
        } else if (strcmp(p, "Education") == 0) {
            snprintf(buf, sizeof(buf), "%s", _("Education"));
            break;
        } else if (strcmp(p, "Game") == 0) {
            snprintf(buf, sizeof(buf), "%s", _("Game"));
            break;
        } else if (strcmp(p, "Graphics") == 0) {
            snprintf(buf, sizeof(buf), "%s", _("Graphics"));
            break;
        } else if (strcmp(p, "Network") == 0) {
            snprintf(buf, sizeof(buf), "%s", _("Network"));
            break;
        } else if (strcmp(p, "Office") == 0) {
            snprintf(buf, sizeof(buf), "%s", _("Office"));
            break;
        } else if (strcmp(p, "Science") == 0) {
            snprintf(buf, sizeof(buf), "%s", _("Science"));
            break;
        } else if (strcmp(p, "Settings") == 0) {
            snprintf(buf, sizeof(buf), "%s", _("Settings"));
            break;
        } else if (strcmp(p, "System") == 0) {
            snprintf(buf, sizeof(buf), "%s", _("System"));
            break;
        } else if (strcmp(p, "Utility") == 0) {
            snprintf(buf, sizeof(buf), "%s", _("Utility"));
            break;
        /* reserved categories */
        } else if (strcmp(p, "Screensaver") == 0) {
            snprintf(buf, sizeof(buf), "%s", _("Screensaver"));
            break;
        } else if (strcmp(p, "TrayIcon") == 0) {
            snprintf(buf, sizeof(buf), "%s", _("Tray Icon"));
            break;
        } else if (strcmp(p, "Applet") == 0) {
            snprintf(buf, sizeof(buf), "%s", _("Applet"));
            break;
        } else if (strcmp(p, "Shell") == 0) {
            snprintf(buf, sizeof(buf), "%s", _("Shell"));
            break;
        }
        p = strtok(NULL, ";");
    }
