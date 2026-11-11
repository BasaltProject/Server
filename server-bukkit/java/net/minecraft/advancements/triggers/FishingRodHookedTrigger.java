// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import net.minecraft.world.entity.item.ItemEntity;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import net.minecraft.core.registries.Registries;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.phys.Vec3;
import com.mojang.serialization.Codec;
import java.util.Collection;
import java.util.Optional;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.projectile.FishingHook;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContextSource;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class FishingRodHookedTrigger extends SimpleCriterionTrigger<FishingRodHookedTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, ItemStack rod, FishingHook hook, Collection<ItemStack> items) {
        Entity target = hook.getHookedIn() != null ? hook.getHookedIn() : hook;
        LootContext hooked = EntityPredicate.createContext(player, target);
        trigger(player, instance -> instance.matches(rod, hooked, items));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, Optional<ItemPredicate> rod, Optional<Holder<LootItemCondition>> entity, Optional<ItemPredicate> item) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    ItemPredicate.CODEC.optionalFieldOf("rod").forGetter(TriggerInstance::rod),
    TriggerKit.cond("entity").forGetter(TriggerInstance::entity),
    ItemPredicate.CODEC.optionalFieldOf("item").forGetter(TriggerInstance::item)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> fishedItem(Optional<ItemPredicate> rod, Optional<EntityPredicate> entity, Optional<ItemPredicate> item) {
        return CriteriaTriggers.FISHING_ROD_HOOKED.createCriterion(new TriggerInstance(TriggerKit.none(), rod, EntityPredicate.wrap(entity), item));
        }
        public boolean matches(ItemStack rod, LootContext hookedIn, Collection<ItemStack> items) {
        if (this.rod.isPresent() && !this.rod.get().test(rod)) {
            return false;
        }
        if (this.entity.isPresent() && !this.entity.get().value().test(hookedIn)) {
            return false;
        }
        if (this.item.isEmpty()) {
            return true;
        }
        ItemPredicate wanted = this.item.get();
        boolean caught = hookedIn.getOptional(LootContextParams.THIS_ENTITY) instanceof ItemEntity floating && wanted.test(floating.getItem());
        return caught || items.stream().anyMatch(wanted::test);
        }
        public void validate(ValidationContextSource validator) {
        TriggerKit.check(validator, "player", player);
        TriggerKit.check(validator, "entity", entity);
        }
    }
}
