package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Minecart;

public interface MinecartOps extends Minecart, Cored {
    default org.bukkit.block.data.BlockData getDisplayBlockData() {
        org.bukkit.block.data.BlockData stored = EntityNbt.blockData(org.basaltmc.bukkit.item.Nbt.map(
            EntityNbt.compound(core(), "").get("DisplayState")));
        return stored != null ? stored : org.bukkit.Bukkit.createBlockData("minecraft:air");
    }

    default void setDisplayBlockData(org.bukkit.block.data.BlockData data) {
        if (data == null || data.getMaterial().isAir()) {
            core().nbtRemove("DisplayState");
        } else {
            EntityNbt.put(core(), "DisplayState", EntityNbt.blockState(data));
        }
    }

    default org.bukkit.material.MaterialData getDisplayBlock() {
        return new org.bukkit.material.MaterialData(getDisplayBlockData().getMaterial());
    }

    default void setDisplayBlock(org.bukkit.material.MaterialData data) {
        setDisplayBlockData(data == null ? null : org.bukkit.Bukkit.createBlockData(data.getItemType()));
    }

    default int getDisplayBlockOffset() {
        return core().nbtInt("DisplayOffset", 6);
    }

    default void setDisplayBlockOffset(int offset) {
        core().nbtSet("DisplayOffset", "int", offset);
    }

    default org.bukkit.Material getMinecartMaterial() {
        return switch (getType()) {
            case CHEST_MINECART -> org.bukkit.Material.CHEST_MINECART;
            case FURNACE_MINECART -> org.bukkit.Material.FURNACE_MINECART;
            case TNT_MINECART -> org.bukkit.Material.TNT_MINECART;
            case HOPPER_MINECART -> org.bukkit.Material.HOPPER_MINECART;
            case COMMAND_BLOCK_MINECART -> org.bukkit.Material.COMMAND_BLOCK_MINECART;
            default -> org.bukkit.Material.MINECART;
        };
    }

    default double getDamage() {
        return core().real("vehicle_damage");
    }

    default void setDamage(double damage) {
        core().set("vehicle_damage", damage);
    }

    default double getMaxSpeed() {
        return core().real("cart_max_speed");
    }

    default void setMaxSpeed(double speed) {
        if (speed >= 0.0) {
            core().set("cart_max_speed", speed);
        }
    }

    default boolean isSlowWhenEmpty() {
        return core().flag("cart_slow_when_empty");
    }

    default void setSlowWhenEmpty(boolean slow) {
        core().set("cart_slow_when_empty", slow);
    }

    default org.bukkit.util.Vector getFlyingVelocityMod() {
        return new org.bukkit.util.Vector(core().real("cart_flying_x"), core().real("cart_flying_y"),
            core().real("cart_flying_z"));
    }

    default void setFlyingVelocityMod(org.bukkit.util.Vector flying) {
        core().set("cart_flying_x", flying.getX());
        core().set("cart_flying_y", flying.getY());
        core().set("cart_flying_z", flying.getZ());
    }

    default org.bukkit.util.Vector getDerailedVelocityMod() {
        return new org.bukkit.util.Vector(core().real("cart_derailed_x"), core().real("cart_derailed_y"),
            core().real("cart_derailed_z"));
    }

    default void setDerailedVelocityMod(org.bukkit.util.Vector derailed) {
        core().set("cart_derailed_x", derailed.getX());
        core().set("cart_derailed_y", derailed.getY());
        core().set("cart_derailed_z", derailed.getZ());
    }

    default net.kyori.adventure.util.TriState getFrictionState() {
        return switch (core().integer("cart_friction")) {
            case 0 -> net.kyori.adventure.util.TriState.FALSE;
            case 1 -> net.kyori.adventure.util.TriState.TRUE;
            default -> net.kyori.adventure.util.TriState.NOT_SET;
        };
    }

    default void setFrictionState(net.kyori.adventure.util.TriState state) {
        if (state == null) {
            throw new IllegalArgumentException("state may not be null");
        }
        core().set("cart_friction", state == net.kyori.adventure.util.TriState.TRUE ? 1
            : state == net.kyori.adventure.util.TriState.FALSE ? 0 : -1);
    }
}
