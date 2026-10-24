package org.basaltmc.bukkit.entity.ops;

import org.bukkit.block.BlockState;
import org.basaltmc.bukkit.BasaltBlockData;
import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.Material;
import org.bukkit.block.data.BlockData;
import org.bukkit.entity.FallingBlock;

public interface FallingBlockOps extends FallingBlock, Cored {
    default BlockData getBlockData() {
        return BasaltBlockData.of(core().integer("object_data"));
    }

    default void setBlockData(BlockData data) {
        core().set("object_data", BasaltBlockData.stateOf(data));
    }

    default Material getMaterial() {
        return getBlockData().getMaterial();
    }

    default boolean getDropItem() {
        return !core().nbtFlag("NoDropItem");
    }

    default void setDropItem(boolean drop) {
        core().nbtSet("NoDropItem", "bool", !drop);
    }

    default boolean canHurtEntities() {
        return core().nbtFlag("HurtEntities");
    }

    default void setHurtEntities(boolean hurt) {
        core().nbtSet("HurtEntities", "bool", hurt);
    }

    default BlockState getBlockState() {
        return getBlockData().createBlockState();
    }

    default void setBlockState(BlockState state) {
        setBlockData(state.getBlockData());
    }

    default boolean getCancelDrop() {
        return core().nbtFlag("CancelDrop");
    }

    default void setCancelDrop(boolean cancel) {
        core().nbtSet("CancelDrop", "bool", cancel);
    }

    default float getDamagePerBlock() {
        return (float) core().nbtReal("FallHurtAmount");
    }

    default void setDamagePerBlock(float damage) {
        core().nbtSet("FallHurtAmount", "float", (double) damage);
    }

    default int getMaxDamage() {
        return core().nbtInt("FallHurtMax");
    }

    default void setMaxDamage(int damage) {
        core().nbtSet("FallHurtMax", "int", damage);
    }

    default boolean doesAutoExpire() {
        return !core().nbtFlag("BasaltNoExpire");
    }

    default void shouldAutoExpire(boolean expires) {
        core().nbtSet("BasaltNoExpire", "bool", !expires);
    }
}
