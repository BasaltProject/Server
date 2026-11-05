// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.phys.Vec3;
public record KineticWeapon(int contactCooldownTicks, int delayTicks, Optional<Condition> dismountConditions, Optional<Condition> knockbackConditions, Optional<Condition> damageConditions, float forwardMovement, float damageMultiplier, Optional<Holder<SoundEvent>> sound, Optional<Holder<SoundEvent>> hitSound) {
    public static final int HIT_FEEDBACK_TICKS = 10;
    public static final Codec<KineticWeapon> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.KineticWeapon.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, KineticWeapon> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.KineticWeapon.class, "STREAM_CODEC");
    public static Vec3 getMotion(Entity livingEntity) {
        Entity mover = !(livingEntity instanceof net.minecraft.world.entity.player.Player) && livingEntity.isPassenger()
            ? livingEntity.getRootVehicle() : livingEntity;
        return mover.getKnownSpeed().scale(20.0);
    }
    public void makeSound(Entity causer) {
        sound.ifPresent(event -> causer.level().playSound(causer, causer.getX(), causer.getY(), causer.getZ(), event, causer.getSoundSource(), 1.0F, 1.0F));
    }
    public void makeLocalHitSound(Entity causer) {
        hitSound.ifPresent(event -> causer.level().playLocalSound(causer.blockPosition(), event.value(), causer.getSoundSource(), 1.0F, 1.0F, false));
    }
    public int computeDamageUseDuration() {
        return delayTicks + damageConditions.map(Condition::maxDurationTicks).orElse(0);
    }
    public void damageEntities(ItemStack stack, int ticksRemaining, LivingEntity livingEntity, EquipmentSlot equipmentSlot) {
        int used = stack.getUseDuration(livingEntity) - ticksRemaining;
        if (used < delayTicks) {
            return;
        }
        used -= delayTicks;
        Vec3 look = livingEntity.getLookAngle();
        double attackerSpeed = look.dot(getMotion(livingEntity));
        double factor = livingEntity instanceof net.minecraft.world.entity.player.Player ? 1.0 : 0.2;
        AttackRange range = livingEntity.getAttackRangeWith(stack);
        double baseDamage = livingEntity.getAttributeBaseValue(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE);
        boolean affected = false;
        var along = net.minecraft.world.entity.projectile.ProjectileUtil.getHitEntitiesAlong(livingEntity, range,
            target -> PiercingWeapon.canHitEntity(livingEntity, target), net.minecraft.world.level.ClipContext.Block.COLLIDER);
        for (var hit : along.right().orElse(java.util.List.of())) {
            Entity target = hit.getEntity();
            if (target instanceof net.minecraft.world.entity.boss.enderdragon.EnderDragonPart part) {
                target = part.parentMob;
            }
            if (livingEntity.wasRecentlyStabbed(target, contactCooldownTicks)) {
                continue;
            }
            livingEntity.rememberStabbedEntity(target);
            double relative = Math.max(0.0, attackerSpeed - look.dot(getMotion(target)));
            boolean dismounts = dismountConditions.isPresent() && dismountConditions.get().test(used, attackerSpeed, relative, factor);
            boolean knocks = knockbackConditions.isPresent() && knockbackConditions.get().test(used, attackerSpeed, relative, factor);
            boolean damages = damageConditions.isPresent() && damageConditions.get().test(used, attackerSpeed, relative, factor);
            if (dismounts || knocks || damages) {
                float damage = (float) baseDamage + (float) net.minecraft.util.Mth.floor(relative * damageMultiplier);
                affected |= livingEntity.stabAttack(equipmentSlot, target, damage, damages, knocks, dismounts);
            }
        }
        if (affected) {
            livingEntity.level().broadcastEntityEvent(livingEntity, (byte) 2);
        }
    }
    public static record Condition(int maxDurationTicks, float minSpeed, float minRelativeSpeed) {
        public static final Codec<Condition> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.KineticWeapon.Condition.class, "CODEC");
        public static final StreamCodec<ByteBuf, Condition> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.KineticWeapon.Condition.class, "STREAM_CODEC");
        public boolean test(int ticksUsed, double attackerSpeed, double relativeSpeed, double entityFactor) {
            return ticksUsed <= maxDurationTicks && attackerSpeed >= (double) minSpeed * entityFactor && relativeSpeed >= (double) minRelativeSpeed * entityFactor;
        }
        public static Optional<Condition> ofAttackerSpeed(int untilTicks, float minAttackerSpeed) {
            return Optional.of(new Condition(untilTicks, minAttackerSpeed, 0.0F));
        }
        public static Optional<Condition> ofRelativeSpeed(int untilTicks, float minRelativeSpeed) {
            return Optional.of(new Condition(untilTicks, 0.0F, minRelativeSpeed));
        }
    }
}
