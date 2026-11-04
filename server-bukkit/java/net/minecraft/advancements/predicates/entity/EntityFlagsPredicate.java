// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.*;
import net.minecraft.advancements.predicates.*;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.*;
import net.minecraft.world.phys.Vec3;
public record EntityFlagsPredicate(Optional<Boolean> isOnGround, Optional<Boolean> isOnFire, Optional<Boolean> isCrouching, Optional<Boolean> isSprinting, Optional<Boolean> isSwimming, Optional<Boolean> isFlying, Optional<Boolean> isBaby, Optional<Boolean> isInWater, Optional<Boolean> isFallFlying) implements EntitySubPredicate {
    public static final Codec<EntityFlagsPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        Codec.BOOL.optionalFieldOf("is_on_ground").forGetter(EntityFlagsPredicate::isOnGround),
        Codec.BOOL.optionalFieldOf("is_on_fire").forGetter(EntityFlagsPredicate::isOnFire),
        Codec.BOOL.optionalFieldOf("is_sneaking").forGetter(EntityFlagsPredicate::isCrouching),
        Codec.BOOL.optionalFieldOf("is_sprinting").forGetter(EntityFlagsPredicate::isSprinting),
        Codec.BOOL.optionalFieldOf("is_swimming").forGetter(EntityFlagsPredicate::isSwimming),
        Codec.BOOL.optionalFieldOf("is_flying").forGetter(EntityFlagsPredicate::isFlying),
        Codec.BOOL.optionalFieldOf("is_baby").forGetter(EntityFlagsPredicate::isBaby),
        Codec.BOOL.optionalFieldOf("is_in_water").forGetter(EntityFlagsPredicate::isInWater),
        Codec.BOOL.optionalFieldOf("is_fall_flying").forGetter(EntityFlagsPredicate::isFallFlying)).apply(i, EntityFlagsPredicate::new));
    private static boolean differs(Optional<Boolean> wanted, boolean actual) {
        return wanted.isPresent() && wanted.get() != actual;
    }
    public boolean matches(Entity entity) {
        if (differs(isOnGround, entity.onGround()) || differs(isOnFire, entity.isOnFire()) || differs(isCrouching, entity.isCrouching())
            || differs(isSprinting, entity.isSprinting()) || differs(isSwimming, entity.isSwimming())) {
            return false;
        }
        if (isFlying.isPresent()) {
            boolean flying = entity instanceof LivingEntity living
                && (living.isFallFlying() || living instanceof net.minecraft.world.entity.player.Player player && player.getAbilities().flying);
            if (flying != isFlying.get()) {
                return false;
            }
        }
        if (differs(isInWater, entity.isInWater())) {
            return false;
        }
        if (entity instanceof LivingEntity living) {
            if (differs(isFallFlying, living.isFallFlying()) || differs(isBaby, living.isBaby())) {
                return false;
            }
        }
        return true;
    }
    @Override
    public boolean matches(Entity entity, ServerLevel level, Vec3 position) {
        return matches(entity);
    }
    public static class Builder {
        private Optional<Boolean> isOnGround = Optional.empty();
        private Optional<Boolean> isOnFire = Optional.empty();
        private Optional<Boolean> isCrouching = Optional.empty();
        private Optional<Boolean> isSprinting = Optional.empty();
        private Optional<Boolean> isSwimming = Optional.empty();
        private Optional<Boolean> isFlying = Optional.empty();
        private Optional<Boolean> isBaby = Optional.empty();
        private Optional<Boolean> isInWater = Optional.empty();
        private Optional<Boolean> isFallFlying = Optional.empty();
        public static Builder flags() {
            return new Builder();
        }
        public Builder isOnGround(boolean value) {
            isOnGround = Optional.of(value);
            return this;
        }
        public Builder isOnFire(boolean value) {
            isOnFire = Optional.of(value);
            return this;
        }
        public Builder isCrouching(boolean value) {
            isCrouching = Optional.of(value);
            return this;
        }
        public Builder isSprinting(boolean value) {
            isSprinting = Optional.of(value);
            return this;
        }
        public Builder isSwimming(boolean value) {
            isSwimming = Optional.of(value);
            return this;
        }
        public Builder isFlying(boolean value) {
            isFlying = Optional.of(value);
            return this;
        }
        public Builder isBaby(boolean value) {
            isBaby = Optional.of(value);
            return this;
        }
        public Builder isInWater(boolean value) {
            isInWater = Optional.of(value);
            return this;
        }
        public Builder isFallFlying(boolean value) {
            isFallFlying = Optional.of(value);
            return this;
        }
        public EntityFlagsPredicate build() {
            return new EntityFlagsPredicate(isOnGround, isOnFire, isCrouching, isSprinting, isSwimming, isFlying, isBaby, isInWater, isFallFlying);
        }
    }
}
