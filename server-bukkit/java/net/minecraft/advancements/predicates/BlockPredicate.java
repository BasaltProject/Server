// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates;

import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Collection;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.HolderSet;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.tags.TagKey;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.pattern.BlockInWorld;
import org.basaltmc.bukkit.facade.HolderSets;
import org.basaltmc.bukkit.facade.ReflectiveCodecs;

public record BlockPredicate(Optional<HolderSet<Block>> blocks, Optional<StatePropertiesPredicate> properties, Optional<NbtPredicate> nbt, DataComponentMatchers components) {
    public static final MapCodec<BlockPredicate> MAP_CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
        ReflectiveCodecs.holderSet(Block.class).optionalFieldOf("blocks").forGetter(BlockPredicate::blocks),
        StatePropertiesPredicate.CODEC.optionalFieldOf("state").forGetter(BlockPredicate::properties),
        NbtPredicate.CODEC.optionalFieldOf("nbt").forGetter(BlockPredicate::nbt),
        DataComponentMatchers.CODEC.forGetter(BlockPredicate::components)).apply(i, BlockPredicate::new));
    public static final Codec<BlockPredicate> CODEC = MAP_CODEC.codec();
    public static final StreamCodec<RegistryFriendlyByteBuf, BlockPredicate> STREAM_CODEC = ByteBufCodecs.fromCodecWithRegistries(CODEC);

    public boolean matches(ServerLevel level, BlockPos pos) {
        if (!level.isLoaded(pos)) {
            return false;
        }
        if (!matchesState(level.getBlockState(pos))) {
            return false;
        }
        if (!willMatchBlockEntity()) {
            return true;
        }
        BlockEntity entity = level.getBlockEntity(pos);
        return entity != null && matchesBlockEntity(level, entity);
    }

    public boolean willMatchBlockEntity() {
        return nbt.isPresent() || !components.isEmpty();
    }

    public boolean matches(BlockInWorld blockInWorld) {
        if (!matchesState(blockInWorld.getState())) {
            return false;
        }
        if (!willMatchBlockEntity()) {
            return true;
        }
        BlockEntity entity = blockInWorld.getEntity();
        return entity != null && matchesBlockEntity(blockInWorld.getLevel(), entity);
    }

    public boolean matchesState(BlockState state) {
        if (blocks.isPresent() && !HolderSets.contains(blocks.get(), state.getBlock().builtInRegistryHolder())) {
            return false;
        }
        return properties.isEmpty() || properties.get().matches(state);
    }

    public boolean matchesBlockEntity(LevelReader level, BlockEntity blockEntity) {
        if (nbt.isPresent() && !nbt.get().matches(blockEntity.saveWithFullMetadata((net.minecraft.core.HolderLookup.Provider) null))) {
            return false;
        }
        return components.isEmpty() || components.test(blockEntity.collectComponents());
    }

    public boolean requiresNbt() {
        return nbt.isPresent();
    }

    public static class Builder {
        private Optional<HolderSet<Block>> blocks = Optional.empty();
        private Optional<StatePropertiesPredicate> properties = Optional.empty();
        private Optional<NbtPredicate> nbt = Optional.empty();
        private DataComponentMatchers components = DataComponentMatchers.ANY;

        private Builder() {}

        public static Builder block() {
            return new Builder();
        }

        public Builder of(HolderGetter<Block> lookup, Block... blocks) {
            return of(lookup, java.util.Arrays.asList(blocks));
        }

        public Builder of(HolderGetter<Block> lookup, Collection<Block> blocks) {
            this.blocks = Optional.of(HolderSet.direct(Block::builtInRegistryHolder, blocks));
            return this;
        }

        public Builder of(HolderGetter<Block> lookup, TagKey<Block> tag) {
            this.blocks = Optional.of(lookup.getOrThrow(tag));
            return this;
        }

        public Builder hasNbt(CompoundTag nbt) {
            this.nbt = Optional.of(new NbtPredicate(nbt));
            return this;
        }

        public Builder setProperties(StatePropertiesPredicate.Builder properties) {
            this.properties = properties.build();
            return this;
        }

        public Builder components(DataComponentMatchers components) {
            this.components = components;
            return this;
        }

        public BlockPredicate build() {
            return new BlockPredicate(blocks, properties, nbt, components);
        }
    }
}
