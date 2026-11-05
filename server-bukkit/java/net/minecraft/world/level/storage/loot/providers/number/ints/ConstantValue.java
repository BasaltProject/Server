// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.ints;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
public record ConstantValue(int value) implements ContextIntProvider, org.basaltmc.bukkit.loot.LootJson.Node {
    public void writeJson(StringBuilder out) {
        out.append(value);
    }
    public static final MapCodec<ConstantValue> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.ints.ConstantValue.class, "MAP_CODEC");
    public static final Codec<ConstantValue> INLINE_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.providers.number.ints.ConstantValue.class, "INLINE_CODEC");
    public MapCodec<ConstantValue> codec() {
        return MAP_CODEC;
    }
    public void validate(ValidationContext context) {
        return;
    }
    public int getIntUnsafe(LootContext random) {
        return value;
    }
}
