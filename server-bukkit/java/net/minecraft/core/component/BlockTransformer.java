// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import io.netty.buffer.ByteBuf;
import java.util.List;
import java.util.Optional;
import java.util.function.IntFunction;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.core.registries.Registries;
import net.minecraft.core.registries.codec.RegistryCodecs;
import net.minecraft.advancements.triggers.CriteriaTriggers;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.sounds.SoundSource;
import net.minecraft.util.ByIdMap;
import net.minecraft.util.ExtraCodecs;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.context.UseOnContext;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.ChestBlock;
import net.minecraft.world.level.block.CopperChestBlock;
import net.minecraft.world.level.block.state.properties.ChestType;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.level.levelgen.blockpredicates.BlockPredicate;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
import net.minecraft.world.level.levelgen.feature.stateproviders.RuleBasedStateProvider;
import net.minecraft.world.level.storage.loot.LootTable;
public record BlockTransformer(List<BlockTransformData> transforms) {
    public static final Codec<BlockTransformer> DIRECT_CODEC = BlockTransformData.CODEC.listOf(1, 200).xmap(BlockTransformer::new, BlockTransformer::transforms);
    public static final Codec<Holder<BlockTransformer>> CODEC = RegistryCodecs.holder(Registries.BLOCK_TRANSFORMER);
    public static final StreamCodec<RegistryFriendlyByteBuf, Holder<BlockTransformer>> STREAM_CODEC = ByteBufCodecs.holderRegistry(Registries.BLOCK_TRANSFORMER);
    public InteractionResult transformBlock(UseOnContext context) {
        if (blocksWithOffhandShield(context)) {
            return InteractionResult.PASS;
        }
        BlockPos pos = context.getClickedPos();
        Level level = context.getLevel();
        Direction face = context.getClickedFace();
        for (BlockTransformData data : transforms) {
            if (data.disallowedFaces().contains(face)) {
                continue;
            }
            BlockState target = data.blockStateProvider.value().getOptionalState(level, level.getRandom(), pos);
            if (target == null) {
                continue;
            }
            BlockState placed = data.updateFromNeighbors ? Block.updateFromNeighbourShapes(target, level, pos) : target;
            Player player = context.getPlayer();
            ItemStack held = context.getItemInHand();
            if (player instanceof ServerPlayer serverPlayer) {
                CriteriaTriggers.ITEM_USED_ON_BLOCK.trigger(serverPlayer, pos, held);
            }
            BlockState before = level.getBlockState(pos);
            if (level instanceof ServerLevel serverLevel) {
                data.loot.ifPresent(table -> Block.dropFromBlockInteractLootTable(serverLevel, table, pos, before, level.getBlockEntity(pos), held, player,
                        (sl, stack) -> data.dropStrategy.pop(sl, pos, face, stack)));
            }
            if (held.isStackable()) {
                held.consume(data.consumeOnUse ? 1 : 0, player);
            } else if (player != null) {
                held.hurtAndBreak(data.itemDamagePerUse, player, context.getHand().asEquipmentSlot());
            }
            level.setBlock(pos, placed, 11);
            level.playSound(player, pos, data.sound.value(), SoundSource.BLOCKS, 1.0F, 1.0F);
            data.particle.send(level, player, pos);
            level.gameEvent(GameEvent.BLOCK_CHANGE, pos, GameEvent.Context.of(player, placed));
            if (data.transformType == TransformType.COPPER_CHEST && before.getBlock() instanceof CopperChestBlock && before.getValue(ChestBlock.TYPE) != ChestType.SINGLE) {
                BlockPos other = ChestBlock.getConnectedBlockPos(pos, before);
                level.gameEvent(GameEvent.BLOCK_CHANGE, other, GameEvent.Context.of(player, level.getBlockState(other)));
                data.particle.send(level, player, other);
            }
            return InteractionResult.SUCCESS;
        }
        return InteractionResult.PASS;
    }
    private static boolean blocksWithOffhandShield(UseOnContext context) {
        Player player = context.getPlayer();
        return context.getHand().equals(InteractionHand.MAIN_HAND) && player.getOffhandItem().has(net.minecraft.core.component.DataComponents.BLOCKS_ATTACKS) && !player.isSecondaryUseActive();
    }
    public static record BlockTransformData(Holder<BlockStateProvider> blockStateProvider, Holder<SoundEvent> sound, TransformParticle particle,
            List<Direction> disallowedFaces, Optional<ResourceKey<LootTable>> loot, DropStrategy dropStrategy, boolean updateFromNeighbors,
            TransformType transformType, boolean consumeOnUse, int itemDamagePerUse) {
        public static final Codec<BlockTransformData> CODEC = RecordCodecBuilder.create(i -> i.group(
                BlockStateProvider.CODEC.fieldOf("block_state_provider").forGetter(BlockTransformData::blockStateProvider),
                SoundEvent.CODEC.optionalFieldOf("sound", BuiltInRegistries.SOUND_EVENT.wrapAsHolder(SoundEvents.EMPTY)).forGetter(BlockTransformData::sound),
                TransformParticle.CODEC.optionalFieldOf("particle", TransformParticle.NONE).forGetter(BlockTransformData::particle),
                Direction.CODEC.listOf().optionalFieldOf("disallowed_faces", List.of()).forGetter(BlockTransformData::disallowedFaces),
                LootTable.KEY_CODEC.optionalFieldOf("loot").forGetter(BlockTransformData::loot),
                DropStrategy.CODEC.optionalFieldOf("drop_strategy", DropStrategy.FROM_MIDDLE).forGetter(BlockTransformData::dropStrategy),
                Codec.BOOL.optionalFieldOf("update_from_neighbors", true).forGetter(BlockTransformData::updateFromNeighbors),
                TransformType.CODEC.optionalFieldOf("transform_type", TransformType.SINGLE_BLOCK).forGetter(BlockTransformData::transformType),
                Codec.BOOL.optionalFieldOf("consume_on_use", true).forGetter(BlockTransformData::consumeOnUse),
                ExtraCodecs.NON_NEGATIVE_INT.optionalFieldOf("item_damage_per_use", 0).forGetter(BlockTransformData::itemDamagePerUse))
                .apply(i, BlockTransformData::new));
        public static final StreamCodec<RegistryFriendlyByteBuf, BlockTransformData> STREAM_CODEC = StreamCodec.composite(
                ByteBufCodecs.fromCodecWithRegistries(BlockStateProvider.CODEC), BlockTransformData::blockStateProvider,
                SoundEvent.STREAM_CODEC, BlockTransformData::sound,
                TransformParticle.STREAM_CODEC, BlockTransformData::particle,
                Direction.STREAM_CODEC.apply(ByteBufCodecs.list()), BlockTransformData::disallowedFaces,
                ResourceKey.<LootTable>streamCodec(lootRegistry()).apply(ByteBufCodecs::optional), BlockTransformData::loot,
                DropStrategy.STREAM_CODEC, BlockTransformData::dropStrategy,
                ByteBufCodecs.BOOL, BlockTransformData::updateFromNeighbors,
                TransformType.STREAM_CODEC, BlockTransformData::transformType,
                ByteBufCodecs.BOOL, BlockTransformData::consumeOnUse,
                ByteBufCodecs.VAR_INT, BlockTransformData::itemDamagePerUse,
                BlockTransformData::new);
        @SuppressWarnings({"unchecked", "rawtypes"})
        private static ResourceKey<? extends net.minecraft.core.Registry<LootTable>> lootRegistry() {
            return (ResourceKey) Registries.LOOT_TABLE;
        }
        public static Builder builder(Holder<BlockStateProvider> targetStateProvider) {
            return new Builder(targetStateProvider);
        }
        public static Builder builder(BlockStateProvider targetStateProvider) {
            return builder(Holder.direct(targetStateProvider));
        }
        public static Builder builder(BlockPredicate predicate, Block block) {
            return builder(RuleBasedStateProvider.builder().ifTrueThenProvide(predicate, block).build());
        }
        public static class Builder {
            private final Holder<BlockStateProvider> provider;
            private Holder<SoundEvent> sound = BuiltInRegistries.SOUND_EVENT.wrapAsHolder(SoundEvents.EMPTY);
            private TransformParticle particle = TransformParticle.NONE;
            private List<Direction> faces = List.of();
            private Optional<ResourceKey<LootTable>> loot = Optional.empty();
            private DropStrategy drops = DropStrategy.FROM_MIDDLE;
            private boolean neighbors = true;
            private TransformType type = TransformType.SINGLE_BLOCK;
            private boolean consume = true;
            private int damage = 1;
            private Builder(Holder<BlockStateProvider> targetStateProvider) {
                this.provider = targetStateProvider;
            }
            public Builder sound(Holder<SoundEvent> sound) {
                this.sound = sound;
                return this;
            }
            public Builder particle(TransformParticle particle) {
                this.particle = particle;
                return this;
            }
            public Builder disallowedFaces(List<Direction> disallowedFaces) {
                this.faces = disallowedFaces;
                return this;
            }
            public Builder loot(ResourceKey<LootTable> loot) {
                this.loot = Optional.of(loot);
                return this;
            }
            public Builder dropStrategy(DropStrategy dropStrategy) {
                this.drops = dropStrategy;
                return this;
            }
            public Builder updateFromNeighbors(boolean updateFromNeighbors) {
                this.neighbors = updateFromNeighbors;
                return this;
            }
            public Builder transformType(TransformType transformType) {
                this.type = transformType;
                return this;
            }
            public Builder consumeOnUse(boolean consumeOnUse) {
                this.consume = consumeOnUse;
                return this;
            }
            public Builder itemDamagePerUse(int itemDamagePerUse) {
                this.damage = itemDamagePerUse;
                return this;
            }
            public BlockTransformData build() {
                return new BlockTransformData(provider, sound, particle, faces, loot, drops, neighbors, type, consume, damage);
            }
        }
    }
    public static enum TransformParticle implements StringRepresentable {
        NONE(0, "none", 0), SCRAPE(1, "scrape", 3005), WAX_ON(2, "wax_on", 3003), WAX_OFF(3, "wax_off", 3004);
        public static final Codec<TransformParticle> CODEC = StringRepresentable.fromValues(TransformParticle::values);
        private static final IntFunction<TransformParticle> BY_ID = ByIdMap.continuous(TransformParticle::getId, values(), ByIdMap.OutOfBoundsStrategy.ZERO);
        public static final StreamCodec<ByteBuf, TransformParticle> STREAM_CODEC = ByteBufCodecs.idMapper(BY_ID, TransformParticle::getId);
        private final int id;
        private final String label;
        private final int event;
        TransformParticle(int id, String label, int levelEvent) {
            this.id = id;
            this.label = label;
            this.event = levelEvent;
        }
        public void send(Level level, Entity player, BlockPos pos) {
            if (this != NONE) {
                level.levelEvent(player, event, pos, 0);
            }
        }
        public String getSerializedName() {
            return label;
        }
        public int getId() {
            return id;
        }
        public int getLevelEvent() {
            return event;
        }
    }
    public static enum DropStrategy implements StringRepresentable {
        CLICKED_FACE(0, "clicked_face", ResourcePopper.FROM_FACE), FROM_MIDDLE(1, "from_middle", ResourcePopper.FROM_MIDDLE);
        public static final Codec<DropStrategy> CODEC = StringRepresentable.fromValues(DropStrategy::values);
        private static final IntFunction<DropStrategy> BY_ID = ByIdMap.continuous(DropStrategy::getId, values(), ByIdMap.OutOfBoundsStrategy.ZERO);
        public static final StreamCodec<ByteBuf, DropStrategy> STREAM_CODEC = ByteBufCodecs.idMapper(BY_ID, DropStrategy::getId);
        private final int id;
        private final String label;
        private final ResourcePopper popper;
        DropStrategy(int id, String label, ResourcePopper popper) {
            this.id = id;
            this.label = label;
            this.popper = popper;
        }
        public String getSerializedName() {
            return label;
        }
        public int getId() {
            return id;
        }
        public void pop(Level level, BlockPos pos, Direction direction, ItemStack stack) {
            popper.pop(level, pos, direction, stack);
        }
        public interface ResourcePopper {
            ResourcePopper FROM_FACE = Block::popResourceFromFace;
            ResourcePopper FROM_MIDDLE = (level, pos, face, stack) -> Block.popResource(level, pos, stack);
            void pop(Level level, BlockPos pos, Direction direction, ItemStack stack);
        }
    }
    public static enum TransformType implements StringRepresentable {
        SINGLE_BLOCK(0, "single_block"), COPPER_CHEST(1, "copper_chest");
        public static final Codec<TransformType> CODEC = StringRepresentable.fromValues(TransformType::values);
        private static final IntFunction<TransformType> BY_ID = ByIdMap.continuous(TransformType::getId, values(), ByIdMap.OutOfBoundsStrategy.ZERO);
        public static final StreamCodec<ByteBuf, TransformType> STREAM_CODEC = ByteBufCodecs.idMapper(BY_ID, TransformType::getId);
        private final int id;
        private final String label;
        TransformType(int id, String label) {
            this.id = id;
            this.label = label;
        }
        public String getSerializedName() {
            return label;
        }
        public int getId() {
            return id;
        }
    }
}
