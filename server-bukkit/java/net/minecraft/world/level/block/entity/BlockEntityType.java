package net.minecraft.world.level.block.entity;

import net.minecraft.core.MappedRegistry;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;

public class BlockEntityType<T> {
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
}
