// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.floats;
import com.mojang.serialization.MapCodec;
import net.minecraft.world.attribute.EnvironmentAttribute;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.EnvironmentAttributeProvider;
public record EnvironmentAttributeValue(EnvironmentAttribute<?> attribute) implements ContextFloatProvider, EnvironmentAttributeProvider {
    private static <V> float floatOf(LootContext context, EnvironmentAttribute<V> attribute) {
        return attribute.type().toFloat(context.getLevel().environmentAttributes().getValue(context, attribute));
    }
    public static final MapCodec<EnvironmentAttributeValue> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.floats.EnvironmentAttributeValue.class, "MAP_CODEC");
    public MapCodec<EnvironmentAttributeValue> codec() {
        return MAP_CODEC;
    }
    public float getFloatUnsafe(LootContext context) {
        return floatOf(context, attribute);
    }
}
