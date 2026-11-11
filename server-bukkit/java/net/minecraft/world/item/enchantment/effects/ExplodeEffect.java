// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.effects;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.level.SimpleExplosionDamageCalculator;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.core.particles.ExplosionParticleInfo;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.random.WeightedList;
import net.minecraft.world.damagesource.DamageType;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.enchantment.EnchantedItemInUse;
import net.minecraft.world.item.enchantment.LevelBasedValue;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.phys.Vec3;
public record ExplodeEffect(boolean attributeToUser, Optional<Holder<DamageType>> damageType, Optional<LevelBasedValue> knockbackMultiplier, Optional<HolderSet<Block>> immuneBlocks, Vec3 offset, LevelBasedValue radius, boolean createFire, Level.ExplosionInteraction blockInteraction, ParticleOptions smallParticle, ParticleOptions largeParticle, WeightedList<ExplosionParticleInfo> blockParticles, Holder<SoundEvent> sound) implements EnchantmentEntityEffect {
    public static final MapCodec<ExplodeEffect> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.ExplodeEffect.class, "CODEC");
    public void apply(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse item, Entity entity, Vec3 position) {
        Vec3 at = position.add(offset);
        DamageSource cause = null;
        if (damageType.isPresent()) {
            cause = attributeToUser ? new DamageSource(damageType.get(), entity) : new DamageSource(damageType.get(), at);
        }
        serverLevel.explode(attributeToUser ? entity : null, cause,
            new SimpleExplosionDamageCalculator(blockInteraction != Level.ExplosionInteraction.NONE, damageType.isPresent(),
                knockbackMultiplier.map(value -> value.calculate(enchantmentLevel)), immuneBlocks),
            at.x(), at.y(), at.z(), Math.max(radius.calculate(enchantmentLevel), 0.0F), createFire, blockInteraction,
            smallParticle, largeParticle, blockParticles, sound);


    }
    public MapCodec<ExplodeEffect> codec() {
        return (com.mojang.serialization.MapCodec) CODEC;
    }
}
