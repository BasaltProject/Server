package org.basaltmc.bukkit.player;

import org.basaltmc.bukkit.Blocks;
import org.basaltmc.bukkit.WorldEnvNatives;
import org.bukkit.GameMode;
import org.bukkit.Material;
import org.bukkit.block.Block;
import org.bukkit.entity.ExperienceOrb;
import org.bukkit.entity.Player;
import org.bukkit.event.block.BlockBreakEvent;
import org.bukkit.inventory.ItemStack;
import org.bukkit.inventory.meta.Damageable;

/** A player breaking a block by API: the event, then the drops and the wear of the tool, as a dig does. */
final class PlayerBreaking {
    private PlayerBreaking() {}

    static boolean breakBlock(Player player, Block block) {
        GameMode mode = player.getGameMode();
        if (block.isEmpty() || mode == GameMode.SPECTATOR || mode == GameMode.ADVENTURE) {
            return false;
        }
        boolean creative = mode == GameMode.CREATIVE;
        BlockBreakEvent event = new BlockBreakEvent(block, player);
        event.setDropItems(!creative);
        player.getServer().getPluginManager().callEvent(event);
        if (event.isCancelled()) {
            return false;
        }
        ItemStack tool = player.getInventory().getItemInMainHand();
        Blocks.flushAll();
        org.bukkit.block.data.BlockData before = block.getBlockData();
        boolean broken = event.isDropItems() ? block.breakNaturally(tool) : WorldEnvNatives.breakBlock(
            block.getWorld().getName(), block.getX(), block.getY(), block.getZ(), "", new String[0], new int[0], false);
        if (!broken) {
            return false;
        }
        block.getWorld().playEffect(block.getLocation(), org.bukkit.Effect.STEP_SOUND, before);
        if (!creative && event.getExpToDrop() > 0) {
            ExperienceOrb orb = block.getWorld().spawn(block.getLocation().add(0.5, 0.5, 0.5), ExperienceOrb.class);
            orb.setExperience(event.getExpToDrop());
        }
        if (!creative && tool.getType().getMaxDurability() > 0 && tool.getItemMeta() instanceof Damageable wear) {
            wear.setDamage(wear.getDamage() + 1);
            tool.setItemMeta(wear);
            if (wear.getDamage() >= tool.getType().getMaxDurability()) {
                player.getInventory().setItemInMainHand(new ItemStack(Material.AIR));
            } else {
                player.getInventory().setItemInMainHand(tool);
            }
        }
        return true;
    }
}
