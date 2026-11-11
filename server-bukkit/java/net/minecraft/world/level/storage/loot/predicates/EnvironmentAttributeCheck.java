// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import com.mojang.serialization.MapCodec;
import java.util.Set;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.attribute.EnvironmentAttribute;
import net.minecraft.world.level.storage.loot.LootContext;
public record EnvironmentAttributeCheck<Value>(EnvironmentAttribute<Value> attribute, Value value) implements LootItemCondition {
    public static final MapCodec<EnvironmentAttributeCheck<?>> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.predicates.EnvironmentAttributeCheck.class, "MAP_CODEC");
    public MapCodec<EnvironmentAttributeCheck<Value>> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return attribute.isPositional() ? Set.of(LootContextParams.ORIGIN) : Set.of();
    }
    public boolean test(LootContext context) {
        return value.equals(context.getLevel().environmentAttributes().getValue(context, attribute));
    }
    public static <Value> LootItemCondition.Builder environmentAttribute(EnvironmentAttribute<Value> attribute, Value value) {
        return () -> new EnvironmentAttributeCheck<>(attribute, value);


    }
}
