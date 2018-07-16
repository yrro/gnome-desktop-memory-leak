#define GNOME_DESKTOP_USE_UNSTABLE_API

#include <errno.h>

#include <glib/gprintf.h>
#include <libgnome-desktop/gnome-rr.h>
#include <mcheck.h>

volatile sig_atomic_t alive = 1;

void interrupted(int sig) {
    alive = 0;
    signal(sig, interrupted);
}

int main(int argc, char* argv[]) {
    mtrace();

    GdkScreen* gdk_screen;
    {
        gdk_init(&argc, &argv);
        gdk_screen = gdk_screen_get_default();
        if (!gdk_screen) {
            g_error("failed to get default screen");
            return EXIT_FAILURE;
        }
    }

    g_autoptr(GnomeRRScreen) grr_screen = NULL;
    {
        g_autoptr(GError) error = NULL;
        grr_screen = gnome_rr_screen_new(gdk_screen, &error);
        if (grr_screen == NULL) {
            g_error("failed to get screens: %s", error->message);
            return 1;
        }
    }

    gint64 rss = 0, rss0 = 0;
    g_autofree gchar* stat = g_strdup_printf("/proc/%d/status", getpid());

    signal(SIGINT, interrupted);

    for (gint64 i = 0; alive; ++i) {
        FILE* f = fopen(stat, "r");
        if (f) {
            size_t line_len = 128;
            char* line = NULL;
            while (getline(&line, &line_len, f) > 0) {
                if (strstr(line, "VmRSS:") == line) {
                    rss = g_ascii_strtoll(line + 6, NULL, 10);
                }
            }
            free(line);
            fclose(f);
        }

        if (rss == 0) {
            g_warning("failed to get VmRSS from <%s>", stat);
        } else {
            gint64 leaked = rss - rss0;
            if (leaked) {
                g_printf("leaked %" G_GINT64_FORMAT " KiB; iteration=%" G_GINT64_FORMAT "; RSS=%" G_GINT64_FORMAT " MiB\n",
                    leaked, i, rss/1024);
            }
            rss0 = rss;
        }

        {
            g_autoptr(GError) error = NULL;
            gboolean r = gnome_rr_screen_refresh(grr_screen, &error);
            if (r == FALSE && error) {
                g_error("failed to refresh screens: %s", error->message);
                return 1;
            }
        }
    }
}

// vim: ts=8 sts=4 sw=4 et
