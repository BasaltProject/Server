// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.ints;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.RangeProvider;
public record UniformGenerator(Holder<ContextIntProvider> min, Holder<ContextIntProvider> max) implements ContextIntProvider, RangeProvider<ContextIntProvider>, org.basaltmc.bukkit.loot.LootJson.Node {
    public void writeJson(StringBuilder out) {
        out.append("{\"type\":\"minecraft:uniform\",\"min\":");
        org.basaltmc.bukkit.loot.LootJson.value(out, min);
        out.append(",\"max\":");
        org.basaltmc.bukkit.loot.LootJson.value(out, max);
        out.append('}');
    }
    public static final MapCodec<UniformGenerator> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.ints.UniformGenerator.class, "MAP_CODEC");
    public MapCodec<UniformGenerator> codec() {
        return MAP_CODEC;
    }
    public int getIntUnsafe(LootContext context) {
        return (int) org.basaltmc.bukkit.loot.LootJson.number(this, false, context);
    }
}
