// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProvider;
public record LootItemRandomChanceCondition(Holder<ContextFloatProvider> chance) implements LootItemCondition, org.basaltmc.bukkit.loot.LootJson.Node {
    public void writeJson(StringBuilder out) {
        out.append("{\"type\":\"minecraft:random_chance\",\"chance\":");
        org.basaltmc.bukkit.loot.LootJson.value(out, chance);
        out.append('}');
    }
    public static final MapCodec<LootItemRandomChanceCondition> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.predicates.LootItemRandomChanceCondition.class, "MAP_CODEC");
    public MapCodec<LootItemRandomChanceCondition> codec() {
        return MAP_CODEC;
    }
    public boolean test(LootContext context) {
        return context.getRandom().nextFloat() < chance.value().getFloat(context);
    }
    public static LootItemCondition.Builder randomChance(float probability) {
        return () -> new LootItemRandomChanceCondition(net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders.exactly(probability));
    }
    public static LootItemCondition.Builder randomChance(Holder<ContextFloatProvider> probability) {
        return () -> new LootItemRandomChanceCondition(probability);
    }
}
