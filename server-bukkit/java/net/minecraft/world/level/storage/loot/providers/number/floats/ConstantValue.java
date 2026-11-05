// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.floats;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
public record ConstantValue(float value) implements ContextFloatProvider, org.basaltmc.bukkit.loot.LootJson.Node {
    public void writeJson(StringBuilder out) {
        out.append(value);
    }
    public static final MapCodec<ConstantValue> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.floats.ConstantValue.class, "MAP_CODEC");
    public static final Codec<ConstantValue> INLINE_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.providers.number.floats.ConstantValue.class, "INLINE_CODEC");
    public MapCodec<ConstantValue> codec() {
        return MAP_CODEC;
    }
    public void validate(ValidationContext context) {
        return;
    }
    public float getFloatUnsafe(LootContext random) {
        return value;
    }
}
