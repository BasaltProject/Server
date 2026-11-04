// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.facade.TileNbt;

public class SkullBlockEntity extends BlockEntity {
    public SkullBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("skull"), worldPosition, blockState);
    }

    public SkullBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    public net.minecraft.resources.Identifier getNoteBlockSound() {
        String sound = nbt().getStringOr("note_block_sound", "");
        return sound.isEmpty() ? null : net.minecraft.resources.Identifier.parse(sound);
    }
}
