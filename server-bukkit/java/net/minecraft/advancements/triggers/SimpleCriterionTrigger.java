// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.function.Predicate;
import net.minecraft.advancements.CriterionTriggerInstance;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import org.basaltmc.bukkit.facade.PlayerConditions;
import net.minecraft.server.PlayerAdvancements;
import net.minecraft.server.level.ServerPlayer;
public abstract class SimpleCriterionTrigger<T extends SimpleCriterionTrigger.SimpleInstance> implements CriterionTrigger<T> {
    protected void trigger(ServerPlayer player, Predicate<T> matcher) {
        PlayerAdvancements advancements = player.getAdvancements();
        Map<PlayerAdvancements.TriggerInstanceKey, T> listeners = advancements.getTriggerMapForType(this);
        if (listeners == null || listeners.isEmpty()) {
            return;
        }
        List<PlayerAdvancements.TriggerInstanceKey> matched = new ArrayList<>();
        for (Map.Entry<PlayerAdvancements.TriggerInstanceKey, T> entry : listeners.entrySet()) {
            T instance = entry.getValue();
            if (matcher.test(instance) && PlayerConditions.test(instance.player(), player, player)) {
                matched.add(entry.getKey());
            }
        }
        for (PlayerAdvancements.TriggerInstanceKey key : matched) {
            advancements.award(key.advancement(), key.criterion());
        }
    }
    public interface SimpleInstance extends CriterionTriggerInstance {
        Optional<Holder<LootItemCondition>> player();
        default void validate(net.minecraft.world.level.storage.loot.ValidationContextSource validator) {
            org.basaltmc.bukkit.facade.TriggerKit.check(validator, "player", player());
        }
    }
}
