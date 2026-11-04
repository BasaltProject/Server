// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Optional;
import net.minecraft.advancements.CriterionTriggerInstance;
import net.minecraft.advancements.predicates.DamagePredicate;
import net.minecraft.advancements.predicates.DamageSourcePredicate;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.ItemStack;
import org.basaltmc.bukkit.facade.PlayerConditions;
public final class CriteriaTriggers {
    private static final Map<Identifier, CriterionTrigger<?>> kByName = new LinkedHashMap<>();
    private static final Map<CriterionTrigger<?>, Identifier> kById = new java.util.IdentityHashMap<>();
    public static final Codec<CriterionTrigger<?>> CODEC = Identifier.CODEC.comapFlatMap(id -> {
        CriterionTrigger<?> found = get(id);
        return found == null ? com.mojang.serialization.DataResult.error(() -> "Unknown trigger: " + id) : com.mojang.serialization.DataResult.success(found);
    }, CriteriaTriggers::idOf);
    private static <T extends CriterionTrigger<?>> T register(String name, T trigger) {
        Identifier id = Identifier.withDefaultNamespace(name);
        kByName.put(id, trigger);
        kById.put(trigger, id);
        return trigger;
    }
    public static CriterionTrigger<?> get(Identifier id) {
        return kByName.get(id);
    }
    public static Identifier idOf(CriterionTrigger<?> trigger) {
        return kById.get(trigger);
    }
    public static Iterable<Identifier> names() {
        return kByName.keySet();
    }
    public record ImpossibleInstance() implements CriterionTriggerInstance {}
    public static final class ImpossibleTrigger implements CriterionTrigger<ImpossibleInstance> {
        @Override
        public Codec<ImpossibleInstance> codec() {
            return com.mojang.serialization.MapCodec.unit(new ImpossibleInstance()).codec();
        }
    }
    public record PlayerInstance(Optional<Holder<LootItemCondition>> player) implements SimpleCriterionTrigger.SimpleInstance {}
    public static final class PlayerTrigger extends SimpleCriterionTrigger<PlayerInstance> {
        private static final Codec<PlayerInstance> kCodec = RecordCodecBuilder.create(i -> i.group(
            PlayerConditions.CODEC.optionalFieldOf("player", Optional.empty()).forGetter(PlayerInstance::player)).apply(i, PlayerInstance::new));
        @Override
        public Codec<PlayerInstance> codec() {
            return kCodec;
        }
        public void trigger(ServerPlayer player) {
            trigger(player, instance -> true);
        }
    }
    public record KilledInstance(Optional<Holder<LootItemCondition>> player, Optional<Holder<LootItemCondition>> entityPredicate, Optional<DamageSourcePredicate> killingBlow) implements SimpleCriterionTrigger.SimpleInstance {
        public boolean matches(ServerPlayer player, Entity entity, DamageSource source) {
            return (killingBlow.isEmpty() || killingBlow.get().matches(player, source))
                && org.basaltmc.bukkit.facade.PlayerConditions.test(entityPredicate, player, entity);
        }
    }
    public static final class KilledTrigger extends SimpleCriterionTrigger<KilledInstance> {
        private static final Codec<KilledInstance> kCodec = RecordCodecBuilder.create(i -> i.group(
            PlayerConditions.CODEC.optionalFieldOf("player", Optional.empty()).forGetter(KilledInstance::player),
            PlayerConditions.CODEC.optionalFieldOf("entity", Optional.empty()).forGetter(KilledInstance::entityPredicate),
            DamageSourcePredicate.CODEC.optionalFieldOf("killing_blow").forGetter(KilledInstance::killingBlow)).apply(i, KilledInstance::new));
        @Override
        public Codec<KilledInstance> codec() {
            return kCodec;
        }
        public void trigger(ServerPlayer player, Entity entity, DamageSource source) {
            trigger(player, instance -> instance.matches(player, entity, source));
        }
    }
    public record HurtEntityInstance(Optional<Holder<LootItemCondition>> player, Optional<DamagePredicate> damage, Optional<Holder<LootItemCondition>> entity) implements SimpleCriterionTrigger.SimpleInstance {}
    public static final class PlayerHurtEntityTrigger extends SimpleCriterionTrigger<HurtEntityInstance> {
        private static final Codec<HurtEntityInstance> kCodec = RecordCodecBuilder.create(i -> i.group(
            PlayerConditions.CODEC.optionalFieldOf("player", Optional.empty()).forGetter(HurtEntityInstance::player),
            DamagePredicate.CODEC.optionalFieldOf("damage").forGetter(HurtEntityInstance::damage),
            PlayerConditions.CODEC.optionalFieldOf("entity", Optional.empty()).forGetter(HurtEntityInstance::entity)).apply(i, HurtEntityInstance::new));
        @Override
        public Codec<HurtEntityInstance> codec() {
            return kCodec;
        }
        public void trigger(ServerPlayer player, Entity victim, DamageSource source, float dealt, float taken, boolean blocked) {
            trigger(player, instance -> (instance.damage().isEmpty() || instance.damage().get().matches(player, source, dealt, taken, blocked))
                && org.basaltmc.bukkit.facade.PlayerConditions.test(instance.entity(), player, victim));
        }
    }
    public record ConsumeItemInstance(Optional<Holder<LootItemCondition>> player, Optional<ItemPredicate> item) implements SimpleCriterionTrigger.SimpleInstance {}
    public static final class ConsumeItemTrigger extends SimpleCriterionTrigger<ConsumeItemInstance> {
        private static final Codec<ConsumeItemInstance> kCodec = RecordCodecBuilder.create(i -> i.group(
            PlayerConditions.CODEC.optionalFieldOf("player", Optional.empty()).forGetter(ConsumeItemInstance::player),
            ItemPredicate.CODEC.optionalFieldOf("item").forGetter(ConsumeItemInstance::item)).apply(i, ConsumeItemInstance::new));
        @Override
        public Codec<ConsumeItemInstance> codec() {
            return kCodec;
        }
        public void trigger(ServerPlayer player, ItemStack stack) {
            trigger(player, instance -> instance.item().isEmpty() || instance.item().get().test(stack));
        }
    }
    public record OpaqueInstance(Optional<Holder<LootItemCondition>> player) implements SimpleCriterionTrigger.SimpleInstance {}
    public static final class OpaqueTrigger extends SimpleCriterionTrigger<OpaqueInstance> {
        static final Codec<OpaqueInstance> kCodec = RecordCodecBuilder.create(i -> i.group(
            PlayerConditions.CODEC.optionalFieldOf("player", Optional.empty()).forGetter(OpaqueInstance::player)).apply(i, OpaqueInstance::new));
        @Override
        public Codec<OpaqueInstance> codec() {
            return kCodec;
        }
    }
    public static final class BlockItemTrigger extends NativeTrigger {
        public void trigger(ServerPlayer player, net.minecraft.core.BlockPos pos, ItemStack stack) {
            fire(player, true, pos.getX(), pos.getY(), pos.getZ(), stack, -1.0);
        }
    }
    public static final class ItemTrigger extends NativeTrigger {
        public void trigger(ServerPlayer player, ItemStack stack) {
            fire(player, false, 0, 0, 0, stack, -1.0);
        }
    }
    public static final class EnderEyeTrigger extends NativeTrigger {
        public void trigger(ServerPlayer player, net.minecraft.core.BlockPos pos) {
            double dx = player.getX() - pos.getX();
            double dz = player.getZ() - pos.getZ();
            fire(player, false, 0, 0, 0, ItemStack.EMPTY, dx * dx + dz * dz);
        }
    }
    public abstract static class NativeTrigger extends SimpleCriterionTrigger<OpaqueInstance> {
        @Override
        public Codec<OpaqueInstance> codec() {
            return OpaqueTrigger.kCodec;
        }
        protected void fire(ServerPlayer player, boolean hasBlock, int x, int y, int z, ItemStack stack, double distance) {
            String item = stack.isEmpty() ? null : net.minecraft.core.registries.BuiltInRegistries.ITEM.getKey(stack.getItem()).toString();
            org.basaltmc.bukkit.MiscNatives.advTrigger(player.getUUID().toString(), idOf(this).toString(), hasBlock, x, y, z, item,
                stack.getCount(), distance);
        }
    }
    public static final ImpossibleTrigger IMPOSSIBLE = register("impossible", new ImpossibleTrigger());
    public static final KilledTrigger PLAYER_KILLED_ENTITY = register("player_killed_entity", new KilledTrigger());
    public static final KilledTrigger ENTITY_KILLED_PLAYER = register("entity_killed_player", new KilledTrigger());
    public static final PlayerHurtEntityTrigger PLAYER_HURT_ENTITY = register("player_hurt_entity", new PlayerHurtEntityTrigger());
    public static final ConsumeItemTrigger CONSUME_ITEM = register("consume_item", new ConsumeItemTrigger());
    public static final PlayerTrigger TICK = register("tick", new PlayerTrigger());
    public static final PlayerTrigger LOCATION = register("location", new PlayerTrigger());
    public static final PlayerTrigger SLEPT_IN_BED = register("slept_in_bed", new PlayerTrigger());
    public static final PlayerTrigger HERO_OF_THE_VILLAGE = register("hero_of_the_village", new PlayerTrigger());
    public static final PlayerTrigger VOLUNTARY_EXILE = register("voluntary_exile", new PlayerTrigger());
    public static final PlayerTrigger AVOID_VIBRATION = register("avoid_vibration", new PlayerTrigger());
    public static final EnderEyeTrigger USED_ENDER_EYE = register("used_ender_eye", new EnderEyeTrigger());
    public static final BlockItemTrigger PLACED_BLOCK = register("placed_block", new BlockItemTrigger());
    public static final BlockItemTrigger ITEM_USED_ON_BLOCK = register("item_used_on_block", new BlockItemTrigger());
    public static final ItemTrigger FILLED_BUCKET = register("filled_bucket", new ItemTrigger());
    public static final ItemTrigger SHOT_CROSSBOW = register("shot_crossbow", new ItemTrigger());
    static {
        for (String other : new String[] {"bee_nest_destroyed", "bred_animals", "brewed_potion", "changed_dimension", "channeled_lightning",
            "construct_beacon", "cured_zombie_villager", "effects_changed", "enchanted_item", "enter_block", "entity_hurt_player",
            "fall_after_explosion", "fall_from_height", "filled_bucket", "fishing_rod_hooked", "inventory_changed", "item_durability_changed",
            "item_used_on_block", "kill_mob_near_sculk_catalyst", "killed_by_arrow", "levitation", "lightning_strike", "nether_travel",
            "placed_block", "player_generates_container_loot", "player_interacted_with_entity", "recipe_crafted", "recipe_unlocked",
            "ride_entity_in_lava", "shot_crossbow", "spear_mobs", "started_riding", "summoned_entity", "tame_animal", "target_hit",
            "thrown_item_picked_up_by_entity", "thrown_item_picked_up_by_player", "trade", "used_totem", "using_item", "default_block_use",
            "any_block_use", "crafter_recipe_crafted", "slide_down_block", "player_sneaked"}) {
            if (kByName.get(Identifier.withDefaultNamespace(other)) == null) {
                register(other, new OpaqueTrigger());
            }
        }
    }
    private CriteriaTriggers() {}
}
