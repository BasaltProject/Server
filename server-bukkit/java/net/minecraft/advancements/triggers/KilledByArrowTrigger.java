// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import net.minecraft.world.item.Items;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import net.minecraft.core.registries.Registries;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.phys.Vec3;
import com.mojang.serialization.Codec;
import java.util.Collection;
import java.util.List;
import java.util.Optional;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContextSource;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class KilledByArrowTrigger extends SimpleCriterionTrigger<KilledByArrowTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, Collection<Entity> victims, ItemStack firedByWeapon) {
        List<LootContext> contexts = victims.stream().map(victim -> EntityPredicate.createContext(player, victim)).toList();
        int kinds = (int) victims.stream().map(Entity::getType).distinct().count();
        trigger(player, instance -> instance.matches(contexts, kinds, firedByWeapon));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, List<Holder<LootItemCondition>> victims, MinMaxBounds.Ints uniqueEntityTypes, Optional<ItemPredicate> firedFromWeapon) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    TriggerKit.conds("victims").forGetter(TriggerInstance::victims),
    MinMaxBounds.Ints.CODEC.optionalFieldOf("unique_entity_types", MinMaxBounds.Ints.ANY).forGetter(TriggerInstance::uniqueEntityTypes),
    ItemPredicate.CODEC.optionalFieldOf("fired_from_weapon").forGetter(TriggerInstance::firedFromWeapon)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> crossbowKilled(HolderGetter<Item> items, EntityPredicate.Builder... victims) {
        return CriteriaTriggers.KILLED_BY_ARROW.createCriterion(new TriggerInstance(TriggerKit.none(), EntityPredicate.wrap(victims),
            MinMaxBounds.Ints.ANY, Optional.of(ItemPredicate.Builder.item().of(items, Items.CROSSBOW).build())));
        }
        public static Criterion<TriggerInstance> crossbowKilled(HolderGetter<Item> items, MinMaxBounds.Ints uniqueEntityTypes) {
        return CriteriaTriggers.KILLED_BY_ARROW.createCriterion(new TriggerInstance(TriggerKit.none(), List.of(),
            uniqueEntityTypes, Optional.of(ItemPredicate.Builder.item().of(items, Items.CROSSBOW).build())));
        }
        public boolean matches(Collection<LootContext> victims, int uniqueEntityTypes, ItemStack firedFromWeapon) {
        if (this.firedFromWeapon.isPresent() && (firedFromWeapon == null || !this.firedFromWeapon.get().test(firedFromWeapon))) {
            return false;
        }
        List<LootContext> unclaimed = new java.util.ArrayList<>(victims);
        for (Holder<LootItemCondition> wanted : this.victims) {
            LootContext hit = unclaimed.stream().filter(wanted.value()::test).findFirst().orElse(null);
            if (hit == null) {
                return false;
            }
            unclaimed.remove(hit);
        }
        return this.uniqueEntityTypes.matches(uniqueEntityTypes);
        }
        public void validate(ValidationContextSource validator) {
        TriggerKit.check(validator, "player", player);
        TriggerKit.checkAll(validator, "victims", victims);
        }
    }
}
