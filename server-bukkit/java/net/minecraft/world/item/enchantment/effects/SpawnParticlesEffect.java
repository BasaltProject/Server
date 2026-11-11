// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.effects;
import net.minecraft.util.valueproviders.ConstantFloat;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.util.StringRepresentable;
import net.minecraft.util.valueproviders.FloatProvider;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.enchantment.EnchantedItemInUse;
import net.minecraft.world.phys.Vec3;
public record SpawnParticlesEffect(ParticleOptions particle, PositionSource horizontalPosition, PositionSource verticalPosition, VelocitySource horizontalVelocity, VelocitySource verticalVelocity, FloatProvider speed) implements EnchantmentEntityEffect {
    public static final MapCodec<SpawnParticlesEffect> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.SpawnParticlesEffect.class, "CODEC");
    public static PositionSource offsetFromEntityPosition(float offset) {
        return new PositionSource(PositionSourceType.ENTITY_POSITION, offset, 1.0F);
    }
    public static PositionSource inBoundingBox() {
        return new PositionSource(PositionSourceType.BOUNDING_BOX, 0.0F, 1.0F);
    }
    public static VelocitySource movementScaled(float scale) {
        return new VelocitySource(scale, ConstantFloat.ZERO);
    }
    public static VelocitySource fixedVelocity(FloatProvider provider) {
        return new VelocitySource(0.0F, provider);
    }
    public void apply(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse item, Entity entity, Vec3 position) {
        RandomSource random = entity.getRandom();
        Vec3 motion = entity.getKnownMovement();
        float width = entity.getBbWidth();
        float height = entity.getBbHeight();
        serverLevel.sendParticles(particle,
            horizontalPosition.getCoordinate(position.x(), position.x(), width, random),
            verticalPosition.getCoordinate(position.y(), position.y() + height / 2.0F, height, random),
            horizontalPosition.getCoordinate(position.z(), position.z(), width, random), 0,
            horizontalVelocity.getVelocity(motion.x(), random), verticalVelocity.getVelocity(motion.y(), random),
            horizontalVelocity.getVelocity(motion.z(), random), speed.sample(random));
    }
    public MapCodec<SpawnParticlesEffect> codec() {
        return (com.mojang.serialization.MapCodec) CODEC;
    }
    public static enum PositionSourceType implements StringRepresentable {
        ENTITY_POSITION, BOUNDING_BOX;
        public static final Codec<PositionSourceType> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.enchantment.effects.SpawnParticlesEffect.PositionSourceType.class, "CODEC");
        public double getCoordinate(double position, double center, float boundingBoxSpan, RandomSource random) {
            return this == ENTITY_POSITION ? position : center + (random.nextDouble() - 0.5) * boundingBoxSpan;
        }
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
        interface CoordinateSource {
            double getCoordinate(double pos, double center, float boundingBoxSpan, RandomSource random);
        }
    }
    public static record PositionSource(PositionSourceType type, float offset, float scale) {
        public static final MapCodec<PositionSource> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.SpawnParticlesEffect.PositionSource.class, "CODEC");
        public double getCoordinate(double position, double center, float boundingBoxSpan, RandomSource random) {
            return type.getCoordinate(position, center, boundingBoxSpan * scale, random) + offset;
        }
    }
    public static record VelocitySource(float movementScale, FloatProvider base) {
        public static final MapCodec<VelocitySource> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.SpawnParticlesEffect.VelocitySource.class, "CODEC");
        public double getVelocity(double movement, RandomSource random) {
            return movement * movementScale + base.sample(random);
        }
    }
}
