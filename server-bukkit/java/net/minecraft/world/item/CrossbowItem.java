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

import com.mojang.serialization.Codec;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.item.enchantment.EnchantmentEffectComponents;
import org.joml.Quaternionf;
import org.joml.Vector3f;
public class CrossbowItem extends ProjectileWeaponItem {
    private static final float kMaxChargeDuration = 1.25F;
    public static final int DEFAULT_RANGE = 8;
    private static final float kArrowPower = 3.15F;
    private static final float kFireworkPower = 1.6F;
    public static final float MOB_ARROW_POWER = 1.6F;
    private static final ChargingSounds kDefaultSounds = new ChargingSounds(Optional.ofNullable(SoundEvents.CROSSBOW_LOADING_START),
        Optional.ofNullable(SoundEvents.CROSSBOW_LOADING_MIDDLE), Optional.ofNullable(SoundEvents.CROSSBOW_LOADING_END));
    private boolean startSoundPlayed = false;
    private boolean midLoadSoundPlayed = false;

    public CrossbowItem(Item.Properties properties) {
        super(properties);
    }

    @Override
    public Predicate<ItemStack> getSupportedHeldProjectiles() {
        return ARROW_OR_FIREWORK;
    }

    @Override
    public Predicate<ItemStack> getAllSupportedProjectiles() {
        return ARROW_ONLY;
    }

    @Override
    public InteractionResult use(Level level, Player player, InteractionHand hand) {
        ItemStack held = player.getItemInHand(hand);
        ChargedProjectiles charged = held.get(DataComponents.CHARGED_PROJECTILES);
        if (charged != null && !charged.isEmpty()) {
            performShooting(level, player, hand, held, charged.contains(Items.FIREWORK_ROCKET) ? kFireworkPower : kArrowPower, 1.0F, null);
            return InteractionResult.CONSUME;
        }
        if (player.getProjectile(held).isEmpty()) {
            return InteractionResult.FAIL;
        }
        startSoundPlayed = false;
        midLoadSoundPlayed = false;
        player.startUsingItem(hand);
        return InteractionResult.CONSUME;
    }

    @Override
    public boolean releaseUsing(ItemStack itemStack, Level level, LivingEntity entity, int remainingTime) {
        return chargeFraction(getUseDuration(itemStack, entity) - remainingTime, itemStack, entity) >= 1.0F && isCharged(itemStack);
    }

    private static boolean tryLoadProjectiles(LivingEntity shooter, ItemStack held) {
        List<ItemStack> drawn = draw(held, shooter.getProjectile(held), shooter);
        if (drawn.isEmpty()) {
            return false;
        }
        held.set(DataComponents.CHARGED_PROJECTILES, ChargedProjectiles.ofNonEmpty(drawn));
        return true;
    }

    public static boolean isCharged(ItemStack itemStack) {
        return !itemStack.getOrDefault(DataComponents.CHARGED_PROJECTILES, ChargedProjectiles.EMPTY).isEmpty();
    }

    @Override
    protected void shootProjectile(LivingEntity shooter, Projectile projectileEntity, int index, float power, float uncertainty, float angle,
            LivingEntity targetOverride) {
        Vector3f aim;
        if (targetOverride != null) {
            double dx = targetOverride.getX() - shooter.getX();
            double dz = targetOverride.getZ() - shooter.getZ();
            double flat = Math.sqrt(dx * dx + dz * dz);
            double dy = targetOverride.getY(0.3333333333333333) - projectileEntity.getY() + flat * 0.2F;
            aim = aimVector(shooter, new Vec3(dx, dy, dz), angle);
        } else {
            Vec3 up = shooter.getUpVector(1.0F);
            Quaternionf turn = new Quaternionf().setAngleAxis(angle * ((float) Math.PI / 180F), up.x, up.y, up.z);
            aim = shooter.getViewVector(1.0F).toVector3f().rotate(turn);
        }
        projectileEntity.shoot(aim.x(), aim.y(), aim.z(), power, uncertainty);
        shooter.level().playSound(null, shooter.getX(), shooter.getY(), shooter.getZ(), SoundEvents.CROSSBOW_SHOOT, shooter.getSoundSource(), 1.0F,
            shotPitch(shooter.getRandom(), index));
    }

    private static Vector3f aimVector(LivingEntity body, Vec3 original, float angle) {
        Vector3f direction = original.toVector3f().normalize();
        Vector3f side = new Vector3f(direction).cross(new Vector3f(0.0F, 1.0F, 0.0F));
        if (side.lengthSquared() <= 1.0E-7) {
            side = new Vector3f(direction).cross(body.getUpVector(1.0F).toVector3f());
        }
        Vector3f axis = new Vector3f(direction).rotateAxis((float) Math.PI / 2F, side.x, side.y, side.z);
        return new Vector3f(direction).rotateAxis(angle * ((float) Math.PI / 180F), axis.x, axis.y, axis.z);
    }

    @Override
    protected Projectile createProjectile(Level level, LivingEntity shooter, ItemStack heldItem, ItemStack projectile, boolean isCrit) {
        if (projectile.is(Items.FIREWORK_ROCKET)) {
            return new FireworkRocketEntity(level, projectile, shooter, shooter.getX(), shooter.getEyeY() - 0.15F, shooter.getZ(), true);
        }
        Projectile created = super.createProjectile(level, shooter, heldItem, projectile, isCrit);
        if (created instanceof AbstractArrow arrow) {
            arrow.setSoundEvent(SoundEvents.CROSSBOW_HIT);
        }
        return created;
    }

    @Override
    protected int getDurabilityUse(ItemStack projectile) {
        return projectile.is(Items.FIREWORK_ROCKET) ? 3 : 1;
    }

    public void performShooting(Level level, LivingEntity shooter, InteractionHand hand, ItemStack weapon, float power, float uncertainty,
            LivingEntity targetOverride) {
        if (!(level instanceof ServerLevel serverLevel)) {
            return;
        }
        ChargedProjectiles charged = weapon.set(DataComponents.CHARGED_PROJECTILES, ChargedProjectiles.EMPTY);
        if (charged == null || charged.isEmpty()) {
            return;
        }
        shoot(serverLevel, shooter, hand, weapon, charged.itemCopies().toList(), power, uncertainty, shooter instanceof Player, targetOverride);
        if (shooter instanceof ServerPlayer player) {
            if (CriteriaTriggers.SHOT_CROSSBOW != null) {
                CriteriaTriggers.SHOT_CROSSBOW.trigger(player, weapon);
            }
            player.awardStat(Stats.ITEM_USED.get(weapon.getItem()));
        }
    }

    private static float shotPitch(RandomSource random, int index) {
        if (index == 0) {
            return 1.0F;
        }
        boolean high = (index & 1) == 1;
        return 1.0F / (random.nextFloat() * 0.5F + 1.8F) + (high ? 0.63F : 0.43F);
    }

    @Override
    public void onUseTick(Level level, LivingEntity entity, ItemStack itemStack, int ticksRemaining) {
        if (level.isClientSide()) {
            return;
        }
        ChargingSounds sounds = chargingSounds(itemStack);
        float progress = (float) (itemStack.getUseDuration(entity) - ticksRemaining) / getChargeDuration(itemStack, entity);
        if (progress < 0.2F) {
            startSoundPlayed = false;
            midLoadSoundPlayed = false;
        }
        if (progress >= 0.2F && !startSoundPlayed) {
            startSoundPlayed = true;
            sounds.start().ifPresent(sound -> level.playSound(null, entity.getX(), entity.getY(), entity.getZ(), sound.value(), SoundSource.PLAYERS, 0.5F, 1.0F));
        }
        if (progress >= 0.5F && !midLoadSoundPlayed) {
            midLoadSoundPlayed = true;
            sounds.mid().ifPresent(sound -> level.playSound(null, entity.getX(), entity.getY(), entity.getZ(), sound.value(), SoundSource.PLAYERS, 0.5F, 1.0F));
        }
        if (progress >= 1.0F && !isCharged(itemStack) && tryLoadProjectiles(entity, itemStack)) {
            sounds.end().ifPresent(sound -> level.playSound(null, entity.getX(), entity.getY(), entity.getZ(), sound.value(), entity.getSoundSource(),
                1.0F, 1.0F / (level.getRandom().nextFloat() * 0.5F + 1.0F) + 0.2F));
        }
    }

    @Override
    public int getUseDuration(ItemStack itemStack, LivingEntity user) {
        return 72000;
    }

    public static int getChargeDuration(ItemStack crossbow, LivingEntity user) {
        return Mth.floor(EnchantmentHelper.modifyCrossbowChargingTime(crossbow, user, kMaxChargeDuration) * 20.0F);
    }

    @Override
    public ItemUseAnimation getUseAnimation(ItemStack itemStack) {
        return ItemUseAnimation.CROSSBOW;
    }

    private static ChargingSounds chargingSounds(ItemStack itemStack) {
        return EnchantmentHelper.pickHighestLevel(itemStack, EnchantmentEffectComponents.CROSSBOW_CHARGING_SOUNDS).orElse(kDefaultSounds);
    }

    private static float chargeFraction(int timeHeld, ItemStack itemStack, LivingEntity holder) {
        return Math.min((float) timeHeld / getChargeDuration(itemStack, holder), 1.0F);
    }

    @Override
    public boolean useOnRelease(ItemStack itemStack) {
        return itemStack.is(this);
    }

    @Override
    public int getDefaultProjectileRange() {
        return DEFAULT_RANGE;
    }

    public static record ChargingSounds(Optional<Holder<SoundEvent>> start, Optional<Holder<SoundEvent>> mid, Optional<Holder<SoundEvent>> end) {
        public static final Codec<ChargingSounds> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.CrossbowItem.ChargingSounds.class, "CODEC");
    }

    public static enum ChargeType implements StringRepresentable {
        NONE("none"), ARROW("arrow"), ROCKET("rocket");

        public static final Codec<ChargeType> CODEC = StringRepresentable.fromEnum(ChargeType::values);
        private final String name;

        private ChargeType(String name) {
            this.name = name;
        }

        @Override
        public String getSerializedName() {
            return name;
        }
    }
}
