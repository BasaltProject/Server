// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentMap;
import net.minecraft.world.level.BaseCommandBlock;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;

import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.facade.TileNbt;

public class CommandBlockEntity extends BlockEntity {
    public CommandBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("command_block"), worldPosition, blockState);
    }

    public CommandBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    public String getCommand() {
        return nbt().getStringOr("Command", "");
    }

    public void setCommand(String command) {
        TileNbt.update(this, tag -> tag.putString("Command", command));
    }

    public boolean isPowered() {
        return nbt().getBooleanOr("powered", false);
    }

    public boolean isAutomatic() {
        return nbt().getBooleanOr("auto", false);
    }

    public boolean isConditionMet() {
        return nbt().getBooleanOr("conditionMet", false);
    }

    public int getSuccessCount() {
        return nbt().getIntOr("SuccessCount", 0);
    }

    public static enum Mode {
        SEQUENCE, AUTO, REDSTONE;
    }
}
