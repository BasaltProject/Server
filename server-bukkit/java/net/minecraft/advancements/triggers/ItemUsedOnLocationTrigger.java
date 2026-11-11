// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import net.minecraft.world.level.storage.loot.Validatable;
import net.minecraft.world.level.storage.loot.parameters.LootContextParamSets;
import net.minecraft.world.level.storage.loot.predicates.AllOfCondition;
import net.minecraft.world.level.storage.loot.predicates.LocationCheck;
import net.minecraft.world.level.storage.loot.predicates.MatchBlock;
import net.minecraft.world.level.storage.loot.predicates.MatchTool;
import net.minecraft.advancements.predicates.StatePropertiesPredicate;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.registries.Registries;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.phys.Vec3;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.advancements.predicates.LocationPredicate;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.item.ItemInstance;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.properties.Property;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContextSource;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class ItemUsedOnLocationTrigger extends SimpleCriterionTrigger<ItemUsedOnLocationTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, BlockPos pos, ItemInstance tool) {
        TriggerKit.fireNative(this, player, pos, tool, -1.0);
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, Optional<Holder<LootItemCondition>> location) implements SimpleCriterionTrigger.SimpleInstance {
        private static TriggerInstance onLocation(LocationPredicate.Builder location, ItemPredicate.Builder item) {
            return new TriggerInstance(TriggerKit.none(), Optional.of(Holder.direct(AllOfCondition.allOf(LocationCheck.checkLocation(location), MatchTool.toolMatches(item)).build())));
        }
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    TriggerKit.cond("location").forGetter(TriggerInstance::location)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> placedBlock(HolderGetter<Block> blocks, Block block) {
        Holder<LootItemCondition> where = Holder.direct(MatchBlock.blockMatches(blocks, block).build());
        return CriteriaTriggers.PLACED_BLOCK.createCriterion(new TriggerInstance(TriggerKit.none(), Optional.of(where)));
        }
        public static Criterion<TriggerInstance> placedBlock(LootItemCondition.Builder locationCondition) {
        return CriteriaTriggers.PLACED_BLOCK.createCriterion(new TriggerInstance(TriggerKit.none(), Optional.of(Holder.direct(locationCondition.build()))));
        }
        public static <T extends Comparable<T>> Criterion<TriggerInstance> placedBlockWithProperties(HolderGetter<Block> blocks, Block block, Property<T> property, String propertyValue) {
        StatePropertiesPredicate.Builder wanted = StatePropertiesPredicate.Builder.properties().hasProperty(property, propertyValue);
        Holder<LootItemCondition> where = Holder.direct(MatchBlock.blockMatches(blocks, block, wanted).build());
        return CriteriaTriggers.PLACED_BLOCK.createCriterion(new TriggerInstance(TriggerKit.none(), Optional.of(where)));
        }
        public static Criterion<TriggerInstance> placedBlockWithProperties(HolderGetter<Block> blocks, Block block, Property<Boolean> property, boolean propertyValue) {
        return placedBlockWithProperties(blocks, block, property, Boolean.toString(propertyValue));
        }
        public static Criterion<TriggerInstance> placedBlockWithProperties(HolderGetter<Block> blocks, Block block, Property<Integer> property, int propertyValue) {
        return placedBlockWithProperties(blocks, block, property, Integer.toString(propertyValue));
        }
        public static <T extends Comparable<T> & StringRepresentable> Criterion<TriggerInstance> placedBlockWithProperties(HolderGetter<Block> blocks, Block block, Property<T> properties, T propertyValue) {
        return placedBlockWithProperties(blocks, block, properties, propertyValue.getSerializedName());
        }
        public static Criterion<TriggerInstance> itemUsedOnBlock(LocationPredicate.Builder location, ItemPredicate.Builder item) {
        return CriteriaTriggers.ITEM_USED_ON_BLOCK.createCriterion(onLocation(location, item));
        }
        public static Criterion<TriggerInstance> allayDropItemOnBlock(LocationPredicate.Builder location, ItemPredicate.Builder item) {
        return CriteriaTriggers.ALLAY_DROP_ITEM_ON_BLOCK.createCriterion(onLocation(location, item));
        }
        public boolean matches(LootContext locationContext) {
        return TriggerKit.passes(this.location, locationContext);
        }
        public void validate(ValidationContextSource validator) {
        TriggerKit.check(validator, "player", player);
        Validatable.validateHolder(validator.context(LootContextParamSets.ADVANCEMENT_LOCATION), "location", location);
        }
    }
}
