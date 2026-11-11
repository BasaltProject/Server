// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.registries.Registries;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.phys.Vec3;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContextSource;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class PickedUpItemTrigger extends SimpleCriterionTrigger<PickedUpItemTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, ItemStack itemStack, Entity entity) {
        LootContext context = entity == null ? null : EntityPredicate.createContext(player, entity);
        trigger(player, instance -> instance.matches(player, itemStack, context));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, Optional<ItemPredicate> item, Optional<Holder<LootItemCondition>> entity) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    ItemPredicate.CODEC.optionalFieldOf("item").forGetter(TriggerInstance::item),
    TriggerKit.cond("entity").forGetter(TriggerInstance::entity)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> thrownItemPickedUpByEntity(Optional<Holder<LootItemCondition>> player, Optional<ItemPredicate> item, Optional<Holder<LootItemCondition>> entity) {
        return CriteriaTriggers.THROWN_ITEM_PICKED_UP_BY_ENTITY.createCriterion(new TriggerInstance(player, item, entity));
        }
        public static Criterion<TriggerInstance> thrownItemPickedUpByPlayer(Optional<Holder<LootItemCondition>> player, Optional<ItemPredicate> item, Optional<Holder<LootItemCondition>> entity) {
        return CriteriaTriggers.THROWN_ITEM_PICKED_UP_BY_PLAYER.createCriterion(new TriggerInstance(player, item, entity));
        }
        public boolean matches(ServerPlayer player, ItemStack itemStack, LootContext pickedUpBy) {
        return (item.isEmpty() || item.get().test(itemStack)) && TriggerKit.passes(entity, pickedUpBy);
        }
        public void validate(ValidationContextSource validator) {
        TriggerKit.check(validator, "player", player);
        TriggerKit.check(validator, "entity", entity);
        }
    }
}
