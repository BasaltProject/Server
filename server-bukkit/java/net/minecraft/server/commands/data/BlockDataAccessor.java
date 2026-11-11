// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands.data;
import net.minecraft.commands.arguments.NbtPathArgument;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.Tag;
import net.minecraft.network.chat.Component;
import net.minecraft.server.commands.ArgProvider;
import net.minecraft.world.level.block.entity.BlockEntity;
public class BlockDataAccessor implements DataAccessor {
    public static final ArgProvider.Factory<DataAccessor> PROVIDER = arg -> ArgProvider.create("block",
        () -> net.minecraft.commands.Commands.argument(arg, net.minecraft.commands.arguments.coordinates.BlockPosArgument.blockPos()),
        context -> {
            BlockPos at = net.minecraft.commands.arguments.coordinates.BlockPosArgument.getLoadedBlockPos(context, arg);
            BlockEntity found = context.getSource().getLevel().getBlockEntity(at);
            if (found == null) {
                throw new com.mojang.brigadier.exceptions.SimpleCommandExceptionType(Component.translatable("commands.data.block.invalid")).create();
            }
            return new BlockDataAccessor(found, at);
        });
    private final BlockEntity entity;
    private final BlockPos pos;
    public BlockDataAccessor(BlockEntity entity, BlockPos pos) {
        this.entity = entity;
        this.pos = pos;
        
    }
    public void setData(CompoundTag tag) {
        net.minecraft.world.level.block.state.BlockState state = entity.getLevel().getBlockState(pos);
        entity.load(tag);
        entity.setChanged();
        entity.getLevel().sendBlockUpdated(pos, state, state, 3);
    }
    public CompoundTag getData() {
        return entity.saveWithFullMetadata(entity.getLevel().registryAccess());
    }
    public Component getModifiedSuccess() {
        return Component.translatable("commands.data.block.modified", pos.getX(), pos.getY(), pos.getZ());
    }
    public Component getPrintSuccess(Tag data) {
        return Component.translatable("commands.data.block.query", pos.getX(), pos.getY(), pos.getZ(), net.minecraft.nbt.NbtUtils.toPrettyComponent(data));
    }
    public Component getPrintSuccess(NbtPathArgument.NbtPath path, double scale, int value) {
        return Component.translatable("commands.data.block.get", path.asString(), pos.getX(), pos.getY(), pos.getZ(), String.format(java.util.Locale.ROOT, "%.2f", scale), value);
    }
}
