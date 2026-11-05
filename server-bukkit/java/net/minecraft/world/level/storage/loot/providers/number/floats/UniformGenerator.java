// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.floats;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.RangeProvider;
public record UniformGenerator(Holder<ContextFloatProvider> min, Holder<ContextFloatProvider> max) implements ContextFloatProvider, RangeProvider<ContextFloatProvider>, org.basaltmc.bukkit.loot.LootJson.Node {
    public void writeJson(StringBuilder out) {
        out.append("{\"type\":\"minecraft:uniform\",\"min\":");
        org.basaltmc.bukkit.loot.LootJson.value(out, min);
        out.append(",\"max\":");
        org.basaltmc.bukkit.loot.LootJson.value(out, max);
        out.append('}');
    }
    public static final MapCodec<UniformGenerator> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.floats.UniformGenerator.class, "MAP_CODEC");
    public MapCodec<UniformGenerator> codec() {
        return MAP_CODEC;
    }
    public float getFloatUnsafe(LootContext context) {
        return (float) org.basaltmc.bukkit.loot.LootJson.number(this, true, context);
    }
}
