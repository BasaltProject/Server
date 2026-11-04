// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import java.util.*;
import java.util.function.*;
import net.minecraft.core.*;
import net.minecraft.core.component.*;
import net.minecraft.network.chat.*;
import net.minecraft.server.level.*;
import net.minecraft.sounds.*;
import net.minecraft.world.*;
import net.minecraft.world.entity.*;
import net.minecraft.world.entity.player.*;
import net.minecraft.world.item.context.*;
import net.minecraft.world.level.*;
import net.minecraft.world.level.block.*;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.phys.*;
import net.minecraft.world.item.component.*;
import net.minecraft.world.entity.projectile.*;
import net.minecraft.world.entity.projectile.throwableitemprojectile.*;
import net.minecraft.world.entity.projectile.hurtingprojectile.*;
import net.minecraft.world.entity.projectile.arrow.*;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.advancements.triggers.CriteriaTriggers;
import net.minecraft.stats.Stats;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.enchantment.EnchantmentHelper;
import net.minecraft.world.level.block.entity.SignBlockEntity;
import net.minecraft.world.level.block.entity.SignTextSlot;
import net.minecraft.tags.*;

import net.minecraft.network.protocol.game.ClientboundSetEntityMotionPacket;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.ai.attributes.*;
import net.minecraft.world.entity.EquipmentSlotGroup;
import net.minecraft.world.entity.decoration.ArmorStand;
import net.minecraft.world.entity.TamableAnimal;
public class MaceItem extends Item {
    private static final int kDefaultAttackDamage = 5;
    private static final float kDefaultAttackSpeed = -3.4F;
    public static final float SMASH_ATTACK_FALL_THRESHOLD = 1.5F;
    private static final float kHeavyThreshold = 5.0F;
    public static final float SMASH_ATTACK_KNOCKBACK_RADIUS = 3.5F;
    private static final float kKnockbackPower = 0.7F;

    public MaceItem(Item.Properties properties) {
        super(properties);
    }

    public static ItemAttributeModifiers createAttributes() {
        return ItemAttributeModifiers.builder()
            .add(Attributes.ATTACK_DAMAGE, new AttributeModifier(BASE_ATTACK_DAMAGE_ID, kDefaultAttackDamage, AttributeModifier.Operation.ADD_VALUE), EquipmentSlotGroup.MAINHAND)
            .add(Attributes.ATTACK_SPEED, new AttributeModifier(BASE_ATTACK_SPEED_ID, kDefaultAttackSpeed, AttributeModifier.Operation.ADD_VALUE), EquipmentSlotGroup.MAINHAND)
            .build();
    }

    public static Tool createToolProperties() {
        return new Tool(List.of(), 1.0F, 2, false);
    }

    @Override
    public void hurtEnemy(ItemStack itemStack, LivingEntity mob, LivingEntity attacker) {
        if (!canSmashAttack(attacker)) {
            return;
        }
        ServerLevel level = (ServerLevel) attacker.level();
        attacker.setDeltaMovement(attacker.getDeltaMovement().with(Direction.Axis.Y, 0.01F));
        attacker.setIgnoreFallDamageFromCurrentImpulse(true, impactPosition(attacker));
        if (attacker instanceof ServerPlayer player) {
            player.connection.send(new ClientboundSetEntityMotionPacket(player));
        }
        if (mob.onGround()) {
            if (attacker instanceof ServerPlayer player) {
                player.setSpawnExtraParticlesOnFall(true);
            }
            SoundEvent sound = attacker.fallDistance > kHeavyThreshold ? SoundEvents.MACE_SMASH_GROUND_HEAVY : SoundEvents.MACE_SMASH_GROUND;
            level.playSound(null, attacker.getX(), attacker.getY(), attacker.getZ(), sound, attacker.getSoundSource(), 1.0F, 1.0F);
        } else {
            level.playSound(null, attacker.getX(), attacker.getY(), attacker.getZ(), SoundEvents.MACE_SMASH_AIR, attacker.getSoundSource(), 1.0F, 1.0F);
        }
        knockback(level, attacker, mob);
    }

    private Vec3 impactPosition(LivingEntity attacker) {
        return attacker.isIgnoringFallDamageFromCurrentImpulse() && attacker.currentImpulseImpactPos.y <= attacker.position().y
            ? attacker.currentImpulseImpactPos : attacker.position();
    }

    @Override
    public void postHurtEnemy(ItemStack itemStack, LivingEntity mob, LivingEntity attacker) {
        if (canSmashAttack(attacker)) {
            attacker.resetFallDistance();
        }
    }

    @Override
    public float getAttackDamageBonus(Entity victim, float ignoredDamage, DamageSource damageSource) {
        if (!(damageSource.getDirectEntity() instanceof LivingEntity attacker) || !canSmashAttack(attacker)) {
            return 0.0F;
        }
        double fall = attacker.fallDistance;
        double damage = fall <= 3.0 ? 4.0 * fall : fall <= 8.0 ? 12.0 + 2.0 * (fall - 3.0) : 22.0 + fall - 8.0;
        if (attacker.level() instanceof ServerLevel level) {
            return (float) (damage + EnchantmentHelper.modifyFallBasedDamage(level, attacker.getWeaponItem(), victim, damageSource, 0.0F) * fall);
        }
        return (float) damage;
    }

    private static void knockback(Level level, Entity attacker, Entity entity) {
        level.levelEvent(2013, entity.getOnPos(), 750);
        level.getEntitiesOfClass(LivingEntity.class, entity.getBoundingBox().inflate(SMASH_ATTACK_KNOCKBACK_RADIUS), knockbackPredicate(attacker, entity))
            .forEach(nearby -> {
                Vec3 offset = nearby.position().subtract(entity.position());
                double power = knockbackPower(attacker, nearby, offset);
                if (power > 0.0) {
                    Vec3 push = offset.normalize().scale(power);
                    nearby.push(push.x, kKnockbackPower, push.z);
                    if (nearby instanceof ServerPlayer other) {
                        other.connection.send(new ClientboundSetEntityMotionPacket(other));
                    }
                }
            });
    }

    private static Predicate<LivingEntity> knockbackPredicate(Entity attacker, Entity entity) {
        return nearby -> {
            boolean ownedPet = nearby instanceof TamableAnimal animal && entity instanceof LivingEntity owner && animal.isTame() && animal.isOwnedBy(owner);
            boolean marker = nearby instanceof ArmorStand stand && stand.isMarker();
            boolean flyingCreative = nearby instanceof Player player && player.isCreative() && player.getAbilities().flying;
            return !nearby.isSpectator() && nearby != attacker && nearby != entity && !attacker.isAlliedTo(nearby) && !ownedPet && !marker
                && entity.distanceToSqr(nearby) <= SMASH_ATTACK_KNOCKBACK_RADIUS * SMASH_ATTACK_KNOCKBACK_RADIUS && !flyingCreative;
        };
    }

    private static double knockbackPower(Entity attacker, LivingEntity nearby, Vec3 direction) {
        return (SMASH_ATTACK_KNOCKBACK_RADIUS - direction.length()) * kKnockbackPower * (attacker.fallDistance > kHeavyThreshold ? 2 : 1)
            * (1.0 - nearby.getAttributeValue(Attributes.KNOCKBACK_RESISTANCE));
    }

    public static boolean canSmashAttack(LivingEntity attacker) {
        return attacker.fallDistance > SMASH_ATTACK_FALL_THRESHOLD && !attacker.isFallFlying();
    }

    @Override
    public DamageSource getItemDamageSource(LivingEntity attacker) {
        return canSmashAttack(attacker) ? attacker.damageSources().mace(attacker) : super.getItemDamageSource(attacker);
    }
}
