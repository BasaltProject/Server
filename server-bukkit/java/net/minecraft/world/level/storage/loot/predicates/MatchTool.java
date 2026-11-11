// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import java.util.Set;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.level.storage.loot.LootContext;
public record MatchTool(Optional<ItemPredicate> predicate) implements LootItemCondition {
    public static final MapCodec<MatchTool> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.predicates.MatchTool.class, "MAP_CODEC");
    public MapCodec<MatchTool> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return Set.of(LootContextParams.TOOL);
    }
    public boolean test(LootContext context) {
        var tool = context.getOptional(LootContextParams.TOOL);
        return tool != null && (predicate.isEmpty() || predicate.get().test(tool));
    }
    public static LootItemCondition.Builder toolMatches(ItemPredicate.Builder predicate) {
        return () -> new MatchTool(Optional.of(predicate.build()));
    }
}
