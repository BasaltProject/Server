// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.core.MappedRegistry;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.core.Holder;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;

public class BlockEntityType<T extends BlockEntity> {
    private final Identifier key;

    protected BlockEntityType() {
        this.key = null;
    }

    private BlockEntityType(Identifier key) {
        this.key = key;
    }

    public Identifier key() {
        return key;
    }

    @SuppressWarnings("unchecked")
    public static synchronized BlockEntityType<?> of(Identifier id) {
        MappedRegistry<BlockEntityType<?>> registry = (MappedRegistry<BlockEntityType<?>>) BuiltInRegistries.BLOCK_ENTITY_TYPE;
        BlockEntityType<?> found = registry.getValue(id);
        if (found == null) {
            found = new BlockEntityType<>(id);
            registry.register(id, found);
        }
        return found;
    }
    @SuppressWarnings("unchecked")
    public static <T extends BlockEntity> BlockEntityType<T> named(String path) {
        return (BlockEntityType<T>) of(Identifier.withDefaultNamespace(path));
    }
    public BlockEntityType(BlockEntitySupplier<? extends T> factory, Set<Block> validBlocks) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.entity.BlockEntityType#<init>");
    }
    public T create(BlockPos worldPosition, BlockState blockState) {
        CompoundTag tag = new CompoundTag();
        tag.putString("id", key.toString());
        tag.putInt("x", worldPosition.getX());
        tag.putInt("y", worldPosition.getY());
        tag.putInt("z", worldPosition.getZ());
        return (T) org.basaltmc.bukkit.facade.BlockEntityTypes.view(null, worldPosition, tag);
    }
    public boolean isValid(BlockState state) {
        return key.toString().equals(org.basaltmc.bukkit.facade.BlockEntityTypes.typeOf(state.getBlock().blockKey()));
    }
    public Holder.Reference<BlockEntityType<?>> builtInRegistryHolder() {
        return (Holder.Reference<BlockEntityType<?>>) BuiltInRegistries.BLOCK_ENTITY_TYPE.wrapAsHolder(this);
    }
    public T getBlockEntity(BlockGetter level, BlockPos pos) {
        BlockEntity found = level.getBlockEntity(pos);
        return found != null && found.getType() == this ? (T) found : null;
    }
    public boolean onlyOpCanSetNbt() {
        return switch (key.getPath()) {
            case "command_block", "lectern", "sign", "hanging_sign", "mob_spawner", "trial_spawner" -> true;
            default -> false;
        };
    }
    public interface BlockEntitySupplier<T extends BlockEntity> {
        T create(BlockPos worldPosition, BlockState blockState);
    }
}
