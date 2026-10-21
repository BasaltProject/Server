package org.basaltmc.bukkit;

import java.io.File;
import java.util.logging.Handler;
import java.util.logging.Level;
import java.util.logging.LogRecord;
import org.bukkit.Bukkit;
import org.bukkit.command.CommandSender;
import org.bukkit.plugin.Plugin;
import org.bukkit.plugin.PluginLoadOrder;

public final class BukkitRuntime {
    private static BasaltServer server;

    private BukkitRuntime() {}

    private static final class LogBridge extends Handler {
        @Override
        public void publish(LogRecord record) {
            int level = record.getLevel().intValue();
            int mapped = level >= Level.SEVERE.intValue() ? 3 : level >= Level.WARNING.intValue() ? 2
                : level >= Level.INFO.intValue() ? 1 : 0;
            String name = record.getLoggerName() == null ? "Bukkit" : record.getLoggerName();
            String message = new java.util.logging.SimpleFormatter().formatMessage(record);
            Natives.log(mapped, name, message);
            if (record.getThrown() != null) {
                java.io.StringWriter trace = new java.io.StringWriter();
                record.getThrown().printStackTrace(new java.io.PrintWriter(trace));
                Natives.log(mapped, name, trace.toString());
            }
        }

        @Override
        public void flush() {}

        @Override
        public void close() {}
    }

    private static void routeLogging() {
        java.util.logging.Logger root = java.util.logging.Logger.getLogger("");
        for (Handler existing : root.getHandlers()) {
            root.removeHandler(existing);
        }
        root.addHandler(new LogBridge());
        root.setLevel(Level.INFO);
    }

    public static boolean boot(String pluginsFolder, String dataFolder) {
        try {
            routeLogging();
            File folder = new File(pluginsFolder);
            folder.mkdirs();
            server = new BasaltServer(new File(".").getAbsoluteFile(), folder);
            Bukkit.setServer(server);
            Natives.log(1, "Bukkit", "Loading Bukkit plugins from " + folder.getPath());
            BasaltPluginManager manager = server.pluginManagerImpl();
            manager.loadPlugins(folder);
            manager.enableAll(PluginLoadOrder.STARTUP);
            manager.enableAll(PluginLoadOrder.POSTWORLD);
            Natives.log(1, "Bukkit", manager.getPlugins().length + " Bukkit plugin(s) loaded");
            return true;
        } catch (Throwable t) {
            t.printStackTrace();
            Natives.log(3, "Bukkit", "Boot failed: " + t);
            return false;
        }
    }

    public static void shutdown() {
        if (server != null) {
            server.pluginManagerImpl().disablePlugins();
        }
    }

    public static void runTask(long id) {
        server.schedulerImpl().run(id);
    }

    public static boolean event(String name, int priority, long generation, Object[] fields, boolean cancelled) {
        return EventBridge.fire(name, priority, generation, fields, cancelled);
    }

    public static boolean command(String label, String senderName, boolean console, String args) {
        CommandSender sender = console ? server.consoleImpl() : server.playerByName(senderName);
        if (sender == null) {
            return false;
        }
        String line = args == null || args.isEmpty() ? label : label + " " + args;
        return server.dispatchCommand(sender, line);
    }
}
