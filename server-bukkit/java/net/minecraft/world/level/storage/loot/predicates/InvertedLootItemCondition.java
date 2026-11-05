// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
public record InvertedLootItemCondition(Holder<LootItemCondition> term) implements LootItemCondition, org.basaltmc.bukkit.loot.LootJson.Node {
    public void writeJson(StringBuilder out) {
        out.append("{\"type\":\"minecraft:inverted\",\"term\":");
        org.basaltmc.bukkit.loot.LootJson.value(out, term);
        out.append('}');
    }
    public static final MapCodec<InvertedLootItemCondition> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.predicates.InvertedLootItemCondition.class, "MAP_CODEC");
    public MapCodec<InvertedLootItemCondition> codec() {
        return MAP_CODEC;
    }
    public boolean test(LootContext context) {
        return !term.value().test(context);
    }
    public void validate(ValidationContext output) {
        return;
    }
    public static LootItemCondition.Builder invert(LootItemCondition.Builder term) {
        return () -> new InvertedLootItemCondition(Holder.direct(term.build()));
    }
    public static LootItemCondition.Builder invert(Holder<LootItemCondition> term) {
        return () -> new InvertedLootItemCondition(term);
    }
}
