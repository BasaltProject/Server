// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.variant;
import com.mojang.serialization.Codec;
import java.util.List;
public record SpawnPrioritySelectors(List<PriorityProvider.Selector<SpawnContext, SpawnCondition>> selectors) {
    public static final SpawnPrioritySelectors EMPTY = new SpawnPrioritySelectors(List.of());
    public static final Codec<SpawnPrioritySelectors> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.variant.SpawnPrioritySelectors.class, "CODEC");
    public static SpawnPrioritySelectors single(SpawnCondition condition, int priority) {
        return new SpawnPrioritySelectors(PriorityProvider.single(condition, priority));
    }
    public static SpawnPrioritySelectors fallback(int priority) {
        return new SpawnPrioritySelectors(PriorityProvider.alwaysTrue(priority));
    }
}
