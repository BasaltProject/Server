package net.minecraft.world.level.block.entity;

import net.minecraft.core.BlockPos;
import net.minecraft.core.TypedInstance;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.debug.DebugValueSource;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import org.basaltmc.bukkit.WorldNatives;
import org.basaltmc.bukkit.facade.NbtCodec;
import org.basaltmc.bukkit.facade.ValueIo;

public class BlockEntity implements DebugValueSource, TypedInstance {
    protected boolean remove;

    private ServerLevel level;
    private BlockPos pos;
    private BlockEntityType<?> type;
    private CompoundTag snapshot;

    protected BlockEntity() {
    }

    public BlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        this.level = level;
        this.pos = pos.immutable();
        this.snapshot = nbt;
        String id = nbt.getStringOr("id", "minecraft:air");
        this.type = BlockEntityType.of(Identifier.parse(id));
    }

    private String world() {
        return level.getWorld().getName();
    }

    public BlockPos getBlockPos() {
        return pos;
    }

    public BlockState getBlockState() {
        return level.getBlockState(pos);
    }

    public Level getLevel() {
        return level;
    }

    public BlockEntityType getType() {
        return type;
    }

    public boolean isRemoved() {
        return remove || (level != null && WorldNatives.blockEntityNbt(world(), pos.getX(), pos.getY(), pos.getZ()) == null);
    }

    public CompoundTag nbt() {
        if (level == null) {
            return snapshot.copy();
        }
        CompoundTag live = NbtCodec.fromBytes(WorldNatives.blockEntityNbt(world(), pos.getX(), pos.getY(), pos.getZ()));
        return live != null ? live : snapshot.copy();
    }

    public void saveWithId(ValueOutput output) {
        ValueIo.write(output, nbt());
    }

    public void loadWithComponents(ValueInput input) {
        load(ValueIo.read(input));
    }

    public void load(CompoundTag tag) {
        if (level == null) {
            snapshot = tag.copy();
            return;
        }
        byte[] bytes = NbtCodec.toBytes(tag);
        if (!WorldNatives.setBlockEntityNbt(world(), pos.getX(), pos.getY(), pos.getZ(), bytes)
            && !WorldNatives.createBlockEntity(world(), pos.getX(), pos.getY(), pos.getZ(), bytes)) {
            return;
        }
        remove = false;
    }

    public void setBlockState(BlockState blockState) {
    }

    public void setChanged() {
    }
}
