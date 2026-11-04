// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.entity.npc.villager.Villager;
import net.minecraft.world.entity.schedule.Activity;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.Items;
import net.minecraft.world.phys.Vec3;

public class WorkAtComposter extends WorkAtPoi {
    private static final List<Item> COMPOSTABLE_ITEMS = List.of(Items.WHEAT_SEEDS, Items.BEETROOT_SEEDS);

    protected void useWorkstation(ServerLevel level, Villager body) {
        Optional<GlobalPos> jobSite = body.getBrain().getMemory(MemoryModuleType.JOB_SITE);
        if (jobSite.isPresent() && level.getBlockState(jobSite.get().pos()).is(net.minecraft.world.level.block.Blocks.COMPOSTER)) {
            makeBread(level, body);
            compostItems(level, body, jobSite.get());
        }
    }

    private void compostItems(ServerLevel level, Villager body, GlobalPos jobSitePos) {
        BlockPos pos = jobSitePos.pos();
        if (!(level.getWorld().getBlockAt(pos.getX(), pos.getY(), pos.getZ()).getBlockData() instanceof org.bukkit.block.data.Levelled composter)) {
            return;
        }
        int before = composter.getLevel();
        if (before == composter.getMaximumLevel()) {
            composter.setLevel(0);
            level.getWorld().getBlockAt(pos.getX(), pos.getY(), pos.getZ()).setBlockData(composter);
            level.getWorld().dropItemNaturally(new org.bukkit.Location(level.getWorld(), pos.getX() + 0.5, pos.getY() + 1.0, pos.getZ() + 0.5),
                new org.bukkit.inventory.ItemStack(org.bukkit.Material.BONE_MEAL));
        }
        int totalItemsToUse = 20;
        int[] seen = new int[COMPOSTABLE_ITEMS.size()];
        var inventory = body.getInventory();
        int current = composter.getLevel();
        for (int i = inventory.getContainerSize() - 1; i >= 0 && totalItemsToUse > 0; --i) {
            ItemStack stack = inventory.getItem(i);
            int index = COMPOSTABLE_ITEMS.indexOf(stack.getItem());
            if (index == -1) {
                continue;
            }
            int total = seen[index] + stack.getCount();
            seen[index] = total;
            int toUse = Math.min(Math.min(total - 10, totalItemsToUse), stack.getCount());
            if (toUse > 0) {
                totalItemsToUse -= toUse;
                for (int j = 0; j < toUse; ++j) {
                    if (level.getRandom().nextFloat() < 0.3F) {
                        current++;
                    }
                    stack.shrink(1);
                    if (current >= 7) {
                        break;
                    }
                }
                if (current >= 7) {
                    break;
                }
            }
        }
        current = Math.min(current, 7);
        if (current != composter.getLevel()) {
            composter.setLevel(current);
            level.getWorld().getBlockAt(pos.getX(), pos.getY(), pos.getZ()).setBlockData(composter);
        }
        level.levelEvent(1500, pos, current != before ? 1 : 0);
    }

    private void makeBread(ServerLevel level, Villager body) {
        var inventory = body.getInventory();
        if (inventory.countItem(Items.BREAD) <= 36) {
            int bread = Math.min(3, inventory.countItem(Items.WHEAT) / 3);
            if (bread != 0) {
                inventory.removeItemType(Items.WHEAT, bread * 3);
                ItemStack left = inventory.addItem(new ItemStack(Items.BREAD, bread));
                if (!left.isEmpty()) {
                    body.spawnAtLocation(level, left, 0.5F);
                }
            }
        }
    }
}
