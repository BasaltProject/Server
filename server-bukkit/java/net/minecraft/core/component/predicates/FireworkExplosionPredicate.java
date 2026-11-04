// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component.predicates;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import java.util.function.Predicate;
import net.minecraft.advancements.predicates.SingleComponentItemPredicate;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.component.DataComponents;
import net.minecraft.world.item.component.FireworkExplosion;
public record FireworkExplosionPredicate(FireworkPredicate predicate) implements SingleComponentItemPredicate<FireworkExplosion> {
    public static final Codec<FireworkExplosionPredicate> CODEC = FireworkPredicate.CODEC.xmap(FireworkExplosionPredicate::new, FireworkExplosionPredicate::predicate);
    public DataComponentType<FireworkExplosion> componentType() {
        return DataComponents.FIREWORK_EXPLOSION;
    }
    public boolean matches(FireworkExplosion value) {
        return predicate.test(value);
    }
    public static record FireworkPredicate(Optional<FireworkExplosion.Shape> shape, Optional<Boolean> twinkle, Optional<Boolean> trail) implements Predicate<FireworkExplosion> {
        public static final Codec<FireworkPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
            FireworkExplosion.Shape.CODEC.optionalFieldOf("shape").forGetter(FireworkPredicate::shape),
            Codec.BOOL.optionalFieldOf("has_twinkle").forGetter(FireworkPredicate::twinkle),
            Codec.BOOL.optionalFieldOf("has_trail").forGetter(FireworkPredicate::trail)).apply(i, FireworkPredicate::new));
        public boolean test(FireworkExplosion explosion) {
            if (shape.isPresent() && shape.get() != explosion.shape()) {
                return false;
            }
            if (twinkle.isPresent() && twinkle.get() != explosion.hasTwinkle()) {
                return false;
            }
            return trail.isEmpty() || trail.get() == explosion.hasTrail();
        }
    }
}
