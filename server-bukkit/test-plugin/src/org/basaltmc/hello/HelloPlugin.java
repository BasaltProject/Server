package org.basaltmc.hello;

import org.bukkit.Bukkit;
import org.bukkit.command.Command;
import org.bukkit.command.CommandSender;
import org.bukkit.event.EventHandler;
import org.bukkit.event.EventPriority;
import org.bukkit.event.Listener;
import org.bukkit.event.player.PlayerJoinEvent;
import org.bukkit.event.player.PlayerQuitEvent;
import org.bukkit.plugin.java.JavaPlugin;
import org.bukkit.scheduler.BukkitRunnable;

public final class HelloPlugin extends JavaPlugin implements Listener {
    private int ticks;

    @Override
    public void onLoad() {
        getLogger().info("onLoad");
    }

    @Override
    public void onEnable() {
        saveDefaultConfig();
        getLogger().info("onEnable, greeting = " + getConfig().getString("greeting"));
        Bukkit.getPluginManager().registerEvents(this, this);
        long interval = getConfig().getLong("tick-interval", 100);
        new BukkitRunnable() {
            @Override
            public void run() {
                ticks++;
                getLogger().info("scheduled task ran, count " + ticks + ", players online: "
                    + Bukkit.getOnlinePlayers().size());
            }
        }.runTaskTimer(this, 20L, interval);
        Bukkit.getScheduler().runTaskLater(this, () -> getLogger().info("runTaskLater fired"), 40L);
    }

    @Override
    public void onDisable() {
        getLogger().info("onDisable after " + ticks + " scheduled runs");
    }

    @EventHandler(priority = EventPriority.NORMAL)
    public void onJoin(PlayerJoinEvent event) {
        String greeting = getConfig().getString("greeting", "Hello!").replace("%player%", event.getPlayer().getName());
        getLogger().info("PlayerJoinEvent for " + event.getPlayer().getName());
        event.getPlayer().sendMessage(greeting);
    }

    @EventHandler
    public void onQuit(PlayerQuitEvent event) {
        getLogger().info("PlayerQuitEvent for " + event.getPlayer().getName());
    }

    @Override
    public boolean onCommand(CommandSender sender, Command command, String label, String[] args) {
        if (command.getName().equals("hellotime")) {
            sender.sendMessage("World time: " + Bukkit.getWorlds().get(0).getFullTime());
            return true;
        }
        String who = args.length > 0 ? args[0] : sender.getName();
        sender.sendMessage("Hello from Bukkit on Basalt, " + who + "!");
        return true;
    }
}
