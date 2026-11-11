// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.nbt;
import net.minecraft.advancements.predicates.NbtPredicate;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.Set;
import net.minecraft.nbt.Tag;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.LootContextArg;
public class ContextNbtProvider implements NbtProvider {
    private final LootContextArg<Tag> source;
    public static final MapCodec<ContextNbtProvider> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.nbt.ContextNbtProvider.class, "MAP_CODEC");
    public static final Codec<ContextNbtProvider> INLINE_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.providers.nbt.ContextNbtProvider.class, "INLINE_CODEC");
    private ContextNbtProvider(LootContextArg<Tag> source) {
        this.source = source;
    }
    public MapCodec<ContextNbtProvider> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public Tag get(LootContext context) {
        return source.get(context);
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return java.util.Set.of(source.contextParam());
    }
    public static NbtProvider forContextEntity(LootContext.EntityTarget source) {
        return new ContextNbtProvider(new EntitySource(source.contextParam()));
    }
    static record BlockEntitySource(ContextKey<? extends BlockEntity> contextParam) implements LootContextArg.Getter<BlockEntity, Tag> {
        public Tag get(BlockEntity blockEntity) {
            return blockEntity.saveWithFullMetadata(blockEntity.getLevel().registryAccess());
        }
    }
    static record EntitySource(ContextKey<? extends Entity> contextParam) implements LootContextArg.Getter<Entity, Tag> {
        public Tag get(Entity entity) {
            return NbtPredicate.getEntityTagToCompare(entity);


        }
    }
}
