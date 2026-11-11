// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.hoglin;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.world.item.enchantment.EnchantmentHelper;
import net.minecraft.util.RandomSource;
import net.minecraft.world.phys.Vec3;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
public interface HoglinBase {
    int ATTACK_ANIMATION_DURATION = 0;
    float PROBABILITY_OF_SPAWNING_AS_BABY = 0;
    int getAttackAnimationRemainingTicks();
    static boolean hurtAndThrowTarget(ServerLevel level, LivingEntity body, LivingEntity target) {
        float attack = (float) body.getAttributeValue(Attributes.ATTACK_DAMAGE);
        float damage = !body.isBaby() && (int) attack > 0 ? attack / 2.0F + level.getRandom().nextInt((int) attack) : attack;
        DamageSource source = body.damageSources().mobAttack(body);
        boolean hurt = target.hurtServer(level, source, damage);
        if (hurt) {
            EnchantmentHelper.doPostAttackEffects(level, target, source);
            if (!body.isBaby()) {
                throwTarget(body, target);
            }
        }
        return hurt;
    }
    static void throwTarget(LivingEntity body, LivingEntity target) {
        double power = body.getAttributeValue(Attributes.ATTACK_KNOCKBACK) - target.getAttributeValue(Attributes.KNOCKBACK_RESISTANCE);
        if (power <= 0.0) {
            return;
        }
        RandomSource random = body.level().getRandom();
        float angle = random.nextInt(21) - 10;
        double horizontal = power * (random.nextFloat() * 0.5F + 0.2F);
        Vec3 push = new Vec3(target.getX() - body.getX(), 0.0, target.getZ() - body.getZ()).normalize().scale(horizontal).yRot(angle);
        double vertical = power * random.nextFloat() * 0.5F;
        target.push(push.x, vertical, push.z);
        target.syncVelocity = true;
    }
}
