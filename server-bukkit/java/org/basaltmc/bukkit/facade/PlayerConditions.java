package org.basaltmc.bukkit.facade;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.Dynamic;
import com.mojang.serialization.DynamicOps;
import java.util.List;
import java.util.Optional;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;

/** The `player` condition list of a trigger, kept as an entity predicate; conditions it cannot express never match. */
public final class PlayerConditions {
    public static final Codec<Optional<Holder<LootItemCondition>>> CODEC = new Codec<>() {
        @Override
        public <T> DataResult<com.mojang.datafixers.util.Pair<Optional<Holder<LootItemCondition>>, T>> decode(DynamicOps<T> ops, T input) {
            Dynamic<T> dynamic = new Dynamic<>(ops, input);
            List<Dynamic<T>> entries = dynamic.asListOpt(d -> d).result().orElse(List.of(dynamic));
            EntityPredicate found = null;
            for (Dynamic<T> entry : entries) {
                String condition = entry.get("condition").asString("");
                if (condition.endsWith("entity_properties") && entry.get("entity").asString("this").equals("this") && found == null) {
                    DataResult<EntityPredicate> parsed = EntityPredicate.CODEC.parse(entry.get("predicate").orElseEmptyMap());
                    if (parsed.result().isPresent()) {
                        found = parsed.result().get();
                        continue;
                    }
                }
                found = EntityPredicate.NEVER;
                break;
            }
            return DataResult.success(com.mojang.datafixers.util.Pair.of(Optional.ofNullable(found).map(predicate -> Holder.direct((LootItemCondition) new EntityPropertiesCondition(predicate))), input));
        }

        @Override
        public <T> DataResult<T> encode(Optional<Holder<LootItemCondition>> value, DynamicOps<T> ops, T prefix) {
            return DataResult.success(prefix);
        }
    };

    public static boolean test(Optional<Holder<LootItemCondition>> condition, ServerPlayer player, Entity subject) {
        if (condition.isEmpty()) {
            return true;
        }
        return condition.get().value() instanceof EntityPropertiesCondition properties && properties.predicate().matches(player, subject);
    }

    private PlayerConditions() {}
}
