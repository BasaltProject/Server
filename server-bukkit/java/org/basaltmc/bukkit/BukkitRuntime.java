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
        System.setProperty("log4j2.loggerContextFactory", "org.basaltmc.bukkit.log.BasaltLog4jFactory");
        java.util.logging.Logger root = java.util.logging.Logger.getLogger("");
        for (Handler existing : root.getHandlers()) {
            root.removeHandler(existing);
        }
        root.addHandler(new LogBridge());
        root.setLevel(Level.INFO);
    }

    /** A file named dump-threads in the working directory makes the VM print every thread's stack and delete it. */
    private static void watchForThreadDumps() {
        Thread watcher = new Thread(() -> {
            File trigger = new File("dump-threads");
            while (true) {
                try {
                    Thread.sleep(2000);
                    if (trigger.exists()) {
                        trigger.delete();
                        StringBuilder out = new StringBuilder("Thread dump:\n");
                        for (java.util.Map.Entry<Thread, StackTraceElement[]> entry
                                : Thread.getAllStackTraces().entrySet()) {
                            out.append("\"").append(entry.getKey().getName()).append("\" ")
                                .append(entry.getKey().getState()).append('\n');
                            for (StackTraceElement frame : entry.getValue()) {
                                out.append("    at ").append(frame).append('\n');
                            }
                        }
                        Natives.log(1, "Bukkit", out.toString());
                    }
                } catch (Throwable t) {
                    return;
                }
            }
        }, "basalt-thread-dumper");
        watcher.setDaemon(true);
        watcher.start();
    }

    public static boolean boot(String pluginsFolder, String dataFolder) {
        try {
            routeLogging();
            watchForThreadDumps();
            File folder = new File(pluginsFolder);
            folder.mkdirs();
            server = new org.bukkit.craftbukkit.CraftServer(new File(".").getAbsoluteFile(), folder);
            Bukkit.setServer(server);
            org.basaltmc.bukkit.misc.Serializables.register();
            return true;
        } catch (Throwable t) {
            t.printStackTrace();
            Natives.log(3, "Bukkit", "Boot failed: " + t);
            return false;
        }
    }

    public static boolean enable(String pluginsFolder) {
        try {
            File folder = new File(pluginsFolder);
            Natives.log(1, "Bukkit", "Loading Bukkit plugins from " + folder.getPath());
            BasaltPluginManager manager = server.pluginManagerImpl();
            server.schedulerImpl().runTaskTimer((Plugin) null, () -> {
                net.minecraft.server.MinecraftServer.currentTick = (int) Natives.currentTick();
                net.minecraft.server.level.ServerChunkCache.tickAll();
            }, 1L, 1L);
            Blocks.begin();
            try {
                manager.loadPlugins(folder);
                manager.enableAll(PluginLoadOrder.STARTUP);
                server.announceWorlds();
                manager.enableAll(PluginLoadOrder.POSTWORLD);
            } finally {
                Blocks.end();
            }
            Natives.log(1, "Bukkit", manager.getPlugins().length + " Bukkit plugin(s) loaded");
            return true;
        } catch (Throwable t) {
            t.printStackTrace();
            Natives.log(3, "Bukkit", "Enable failed: " + t);
            return false;
        }
    }

    public static String[] pluginList() {
        org.bukkit.plugin.Plugin[] plugins = server.pluginManagerImpl().getPlugins();
        String[] rows = new String[plugins.length];
        for (int index = 0; index < plugins.length; index++) {
            rows[index] = plugins[index].getName() + "\t" + plugins[index].getDescription().getVersion() + "\t"
                + (plugins[index].isEnabled() ? "1" : "0");
        }
        return rows;
    }

    public static boolean shutdown() {
        if (server != null) {
            server.pluginManagerImpl().disablePlugins();
        }
        org.basaltmc.bukkit.net.ChannelBridge.shutdown();
        BasaltScheduler.shutdownWorkers();
        return drainThreads();
    }

    private static boolean drainThreads() {
        boolean clean = true;
        long deadline = System.nanoTime() + 5_000_000_000L;
        Thread self = Thread.currentThread();
        for (Thread thread : Thread.getAllStackTraces().keySet()) {
            if (thread == self || thread.isDaemon() || !thread.isAlive() || thread.getThreadGroup() == null
                    || "main".equals(thread.getName())) {
                continue;
            }
            thread.interrupt();
            try {
                thread.join(Math.max(1, (deadline - System.nanoTime()) / 1_000_000L));
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                return false;
            }
            if (thread.isAlive()) {
                clean = false;
                Natives.log(2, "Bukkit", "Thread " + thread.getName() + " is still running at shutdown");
            }
        }
        return clean;
    }

    public static void runTask(long id) {
        server.schedulerImpl().run(id);
    }

    public static boolean event(String name, int priority, long generation, Object[] fields, boolean cancelled) {
        Blocks.begin();
        try {
            if (name.equals("world.chunk_load") && priority == 0) {
                org.basaltmc.bukkit.worldgen.CustomGenerators.chunkLoaded(fields);
            }
            return EventBridge.fire(name, priority, generation, fields, cancelled);
        } finally {
            Blocks.end();
        }
    }

    public static void channelMessage(String channel, String uuid, byte[] payload) {
        Blocks.begin();
        try {
            BasaltPlayer player = server.player(java.util.UUID.fromString(uuid), null);
            server.messengerImpl().dispatchIncomingMessage(player, channel, payload);
        } finally {
            Blocks.end();
        }
    }

    public static boolean command(String label, String senderName, String kind, String world, double x, double y,
        double z, String caller, String args) {
        CommandSender sender = Senders.of(server, kind, senderName, world, x, y, z, caller);
        if (sender == null) {
            return false;
        }
        String line = args == null || args.isEmpty() ? label : label + " " + args;
        Blocks.begin();
        try {
            return server.dispatchCommand(sender, line);
        } finally {
            Blocks.end();
        }
    }
}
