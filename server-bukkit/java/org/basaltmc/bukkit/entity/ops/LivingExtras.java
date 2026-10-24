package org.basaltmc.bukkit.entity.ops;

import com.destroystokyo.paper.block.TargetBlockInfo;
import com.destroystokyo.paper.entity.TargetEntityInfo;
import org.basaltmc.bukkit.entity.EntityCore;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.Sound;
import org.bukkit.block.Block;
import org.bukkit.block.BlockFace;
import org.bukkit.entity.Entity;
import org.bukkit.entity.LivingEntity;
import org.bukkit.inventory.EquipmentSlot;
import org.bukkit.inventory.ItemStack;
import org.bukkit.util.RayTraceResult;

/** Living-entity facts that read the same state every living entity keeps. */
final class LivingExtras {
    private LivingExtras() {}

    static RayTraceResult blocks(LivingEntity self, EntityCore core, int maxDistance, TargetBlockInfo.FluidMode fluids) {
        return core.world.rayTraceBlocks(self.getEyeLocation(), self.getEyeLocation().getDirection(), maxDistance,
            fluids.bukkit, false);
    }

    static TargetBlockInfo blockInfo(LivingEntity self, EntityCore core, int maxDistance,
            TargetBlockInfo.FluidMode fluids) {
        RayTraceResult hit = blocks(self, core, maxDistance, fluids);
        return hit == null || hit.getHitBlock() == null ? null
            : new TargetBlockInfo(hit.getHitBlock(), hit.getHitBlockFace());
    }

    static Block block(LivingEntity self, EntityCore core, int maxDistance, TargetBlockInfo.FluidMode fluids) {
        RayTraceResult hit = blocks(self, core, maxDistance, fluids);
        return hit == null ? null : hit.getHitBlock();
    }

    static BlockFace face(LivingEntity self, EntityCore core, int maxDistance, TargetBlockInfo.FluidMode fluids) {
        RayTraceResult hit = blocks(self, core, maxDistance, fluids);
        return hit == null ? null : hit.getHitBlockFace();
    }

    static TargetEntityInfo entityInfo(LivingEntity self, EntityCore core, int maxDistance, boolean ignoreBlocks) {
        RayTraceResult hit = core.world.rayTraceEntities(self.getEyeLocation(), self.getEyeLocation().getDirection(),
            maxDistance, 0.0, entity -> !entity.equals(self));
        if (hit == null || hit.getHitEntity() == null) {
            return null;
        }
        if (!ignoreBlocks) {
            RayTraceResult wall = blocks(self, core, maxDistance, TargetBlockInfo.FluidMode.NEVER);
            if (wall != null && wall.getHitPosition().distanceSquared(self.getEyeLocation().toVector())
                < hit.getHitPosition().distanceSquared(self.getEyeLocation().toVector())) {
                return null;
            }
        }
        return new TargetEntityInfo(hit.getHitEntity(), hit.getHitPosition());
    }

    static Sound sound(Object key, Sound fallback) {
        if (key instanceof String id && !id.isEmpty()) {
            NamespacedKey parsed = NamespacedKey.fromString(id);
            Sound found = parsed == null ? null : Registry.SOUNDS.get(parsed);
            if (found != null) {
                return found;
            }
        }
        return fallback;
    }

    static EquipmentSlot activeHand(EntityCore core) {
        return core.integer("active_slot") == 1 ? EquipmentSlot.OFF_HAND : EquipmentSlot.HAND;
    }

    static ItemStack activeItem(LivingEntity self, EntityCore core) {
        if (!(core.property("active_slot") instanceof Number slot) || slot.intValue() < 0 || self.getEquipment() == null) {
            return ItemStack.empty();
        }
        return self.getEquipment().getItem(activeHand(core));
    }

    static void startUsing(EntityCore core, EquipmentSlot hand) {
        if (hand != EquipmentSlot.HAND && hand != EquipmentSlot.OFF_HAND) {
            throw new IllegalArgumentException("Only the hands can use an item");
        }
        core.call("start_using", hand == EquipmentSlot.OFF_HAND ? 1L : 0L, 72000L);
    }

    static void slotBreak(EntityCore core, EquipmentSlot slot, java.util.Collection<org.bukkit.entity.Player> who) {
        long code = switch (slot) {
            case HAND -> 47L;
            case OFF_HAND -> 48L;
            case HEAD -> 49L;
            case CHEST -> 50L;
            case LEGS -> 51L;
            case FEET -> 52L;
            case BODY -> 65L;
            case SADDLE -> 68L;
        };
        java.util.List<Object> arguments = new java.util.ArrayList<>();
        arguments.add(code);
        if (who != null) {
            for (org.bukkit.entity.Player player : who) {
                arguments.add(player.getUniqueId().toString());
            }
            if (arguments.size() == 1) {
                return;
            }
        }
        core.call("entity_event", arguments.toArray());
    }

    @SuppressWarnings("unchecked")
    static <T> T memory(EntityCore core, org.bukkit.entity.memory.MemoryKey<T> key) {
        return (T) core.memories.get(key);
    }

    static boolean climbing(Entity self) {
        return org.bukkit.Tag.CLIMBABLE.isTagged(self.getLocation().getBlock().getType());
    }
}
