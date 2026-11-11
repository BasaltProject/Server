// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import java.util.Set;
import net.minecraft.advancements.predicates.LocationPredicate;
import net.minecraft.core.Direction;
import net.minecraft.core.Vec3i;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.level.storage.loot.LootContext;
public record LocationCheck(Optional<LocationPredicate> predicate, Vec3i offset) implements LootItemCondition {
    public static final MapCodec<LocationCheck> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.predicates.LocationCheck.class, "MAP_CODEC");
    public MapCodec<LocationCheck> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public boolean test(LootContext context) {
        var origin = context.getOptional(LootContextParams.ORIGIN);
        return origin != null && (predicate.isEmpty() || predicate.get().matches(context.getLevel(), origin.x() + offset.getX(),
            origin.y() + offset.getY(), origin.z() + offset.getZ()));
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return Set.of(LootContextParams.ORIGIN);
    }
    public static LootItemCondition.Builder checkLocation(LocationPredicate.Builder predicate) {
        return () -> new LocationCheck(Optional.of(predicate.build()), net.minecraft.core.Vec3i.ZERO);
    }
    public static LootItemCondition.Builder checkLocation(LocationPredicate.Builder predicate, Vec3i offset) {
        return () -> new LocationCheck(Optional.of(predicate.build()), offset);
    }
    public static LootItemCondition.Builder checkLocation(LocationPredicate.Builder predicate, Direction direction) {
        return checkLocation(predicate, direction.getUnitVec3i());
    }
}
