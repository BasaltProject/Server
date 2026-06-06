package org.basaltmc.hello;

import org.bukkit.event.EventHandler;
import org.bukkit.event.Listener;
import org.bukkit.event.block.BlockBreakEvent;
import org.bukkit.event.block.BlockDamageEvent;
import org.bukkit.event.block.BlockPlaceEvent;
import org.bukkit.event.entity.EntityDamageEvent;
import org.bukkit.event.player.PlayerInteractEvent;
import org.bukkit.event.player.PlayerChangedWorldEvent;
import org.bukkit.event.world.ChunkLoadEvent;
import org.bukkit.plugin.java.JavaPlugin;

/** Logs the world-facing events Basalt maps, and un-cancels block placing to prove restore works. */
final class EventProbe implements Listener {
    private final JavaPlugin plugin;
    private int chunks;

    EventProbe(JavaPlugin plugin) {
        this.plugin = plugin;
    }

    @EventHandler
    public void onInteract(PlayerInteractEvent event) {
        plugin.getLogger().info("EVENT PlayerInteractEvent " + event.getAction() + " block=" + event.getClickedBlock());
    }

    @EventHandler
    public void onDamage(BlockDamageEvent event) {
        plugin.getLogger().info("EVENT BlockDamageEvent " + event.getBlock() + " by " + event.getPlayer().getName());
    }

    @EventHandler
    public void onBreak(BlockBreakEvent event) {
        plugin.getLogger().info("EVENT BlockBreakEvent " + event.getBlock().getType());
    }

    @EventHandler
    public void onPlace(BlockPlaceEvent event) {
        plugin.getLogger().info("EVENT BlockPlaceEvent " + event.getBlockPlaced() + " item=" + event.getItemInHand());
    }

    @EventHandler
    public void onEntityDamage(EntityDamageEvent event) {
        plugin.getLogger().info("EVENT EntityDamageEvent " + event.getEntityType() + " " + event.getCause() + " " + event.getDamage());
    }

    @EventHandler
    public void onWorld(PlayerChangedWorldEvent event) {
        plugin.getLogger().info("EVENT PlayerChangedWorldEvent from " + event.getFrom().getName());
    }

    @EventHandler
    public void onChunk(ChunkLoadEvent event) {
        if (++chunks <= 3) {
            plugin.getLogger().info("EVENT ChunkLoadEvent " + event.getChunk() + " new=" + event.isNewChunk());
        }
    }
}
