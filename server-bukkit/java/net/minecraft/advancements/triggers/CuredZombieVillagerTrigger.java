// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.registries.Registries;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.phys.Vec3;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.monster.zombie.Zombie;
import net.minecraft.world.entity.npc.villager.Villager;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContextSource;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class CuredZombieVillagerTrigger extends SimpleCriterionTrigger<CuredZombieVillagerTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, Zombie zombie, Villager villager) {
        LootContext zombieContext = EntityPredicate.createContext(player, zombie);
        LootContext villagerContext = EntityPredicate.createContext(player, villager);
        trigger(player, instance -> instance.matches(zombieContext, villagerContext));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, Optional<Holder<LootItemCondition>> zombie, Optional<Holder<LootItemCondition>> villager) implements SimpleCriterionTrigger.SimpleInstance {
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    TriggerKit.cond("zombie").forGetter(TriggerInstance::zombie),
    TriggerKit.cond("villager").forGetter(TriggerInstance::villager)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> curedZombieVillager() {
        return CriteriaTriggers.CURED_ZOMBIE_VILLAGER.createCriterion(new TriggerInstance(TriggerKit.none(), TriggerKit.none(), TriggerKit.none()));
        }
        public boolean matches(LootContext zombie, LootContext villager) {
        return TriggerKit.passes(this.zombie, zombie) && TriggerKit.passes(this.villager, villager);
        }
        public void validate(ValidationContextSource validator) {
        TriggerKit.check(validator, "player", player);
        TriggerKit.check(validator, "zombie", zombie);
        TriggerKit.check(validator, "villager", villager);
        }
    }
}
