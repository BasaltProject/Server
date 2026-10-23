package org.basaltmc.bukkit.log;

import org.basaltmc.bukkit.BasaltPluginClassLoader;
import org.basaltmc.bukkit.Natives;

/** Names the plugin a logger belongs to and writes its lines through the server log. */
public final class LogTags {
    private static final StackWalker WALKER = StackWalker.getInstance(StackWalker.Option.RETAIN_CLASS_REFERENCE);

    private LogTags() {}

    public static String of(ClassLoader loader, String fallback) {
        if (loader instanceof BasaltPluginClassLoader plugin) {
            return plugin.pluginName();
        }
        return fallback;
    }

    public static String ofCaller(String fallback) {
        return WALKER.walk(frames -> frames.map(frame -> frame.getDeclaringClass().getClassLoader())
            .filter(BasaltPluginClassLoader.class::isInstance).findFirst()).map(loader -> of(loader, fallback))
            .orElse(fallback);
    }

    public static void write(int level, String tag, String message, Throwable thrown) {
        Natives.log(level, tag, message);
        if (thrown != null) {
            java.io.StringWriter trace = new java.io.StringWriter();
            thrown.printStackTrace(new java.io.PrintWriter(trace));
            Natives.log(level, tag, trace.toString());
        }
    }
}
