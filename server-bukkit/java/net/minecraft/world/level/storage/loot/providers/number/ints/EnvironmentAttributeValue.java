// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.ints;
import com.mojang.serialization.MapCodec;
import net.minecraft.world.attribute.EnvironmentAttribute;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.EnvironmentAttributeProvider;
public record EnvironmentAttributeValue(EnvironmentAttribute<?> attribute) implements ContextIntProvider, EnvironmentAttributeProvider {
    private static <Value> int asInt(LootContext context, EnvironmentAttribute<Value> attribute) {
        return attribute.type().toInt(context.getLevel().environmentAttributes().getValue(context, attribute));
    }
    public static final MapCodec<EnvironmentAttributeValue> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.ints.EnvironmentAttributeValue.class, "MAP_CODEC");
    public MapCodec<EnvironmentAttributeValue> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public int getIntUnsafe(LootContext context) throws ArithmeticException {
        return asInt(context, attribute);
    }
}
