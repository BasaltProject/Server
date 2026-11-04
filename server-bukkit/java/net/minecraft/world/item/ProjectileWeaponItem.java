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

import net.minecraft.util.Unit;
public abstract class ProjectileWeaponItem extends Item {
    public static final Predicate<ItemStack> ARROW_ONLY = stack -> stack.is(ItemTags.ARROWS);
    public static final Predicate<ItemStack> ARROW_OR_FIREWORK = ARROW_ONLY.or(stack -> stack.is(Items.FIREWORK_ROCKET));

    public ProjectileWeaponItem(Item.Properties properties) {
        super(properties);
    }

    public Predicate<ItemStack> getSupportedHeldProjectiles() {
        return getAllSupportedProjectiles();
    }

    public abstract Predicate<ItemStack> getAllSupportedProjectiles();

    public static ItemStack getHeldProjectile(LivingEntity entity, Predicate<ItemStack> valid) {
        ItemStack off = entity.getItemInHand(InteractionHand.OFF_HAND);
        if (valid.test(off)) {
            return off;
        }
        ItemStack main = entity.getItemInHand(InteractionHand.MAIN_HAND);
        return valid.test(main) ? main : ItemStack.EMPTY;
    }

    public abstract int getDefaultProjectileRange();

    protected void shoot(ServerLevel level, LivingEntity shooter, InteractionHand hand, ItemStack weapon, List<ItemStack> projectiles,
            float power, float uncertainty, boolean isCrit, LivingEntity targetOverride) {
        float spread = EnchantmentHelper.processProjectileSpread(level, weapon, shooter, 0.0F);
        float step = projectiles.size() == 1 ? 0.0F : 2.0F * spread / (projectiles.size() - 1);
        float start = (projectiles.size() - 1) % 2 * step / 2.0F;
        float sign = 1.0F;
        for (int i = 0; i < projectiles.size(); i++) {
            ItemStack ammo = projectiles.get(i);
            if (ammo.isEmpty()) {
                continue;
            }
            float angle = start + sign * ((i + 1) / 2) * step;
            sign = -sign;
            int index = i;
            Projectile.spawnProjectile(createProjectile(level, shooter, weapon, ammo, isCrit), level, ammo,
                entity -> shootProjectile(shooter, entity, index, power, uncertainty, angle, targetOverride));
            weapon.hurtAndBreak(getDurabilityUse(ammo), shooter, hand.asEquipmentSlot());
            if (weapon.isEmpty()) {
                break;
            }
        }
    }

    protected int getDurabilityUse(ItemStack projectile) {
        return 1;
    }

    protected abstract void shootProjectile(LivingEntity shooter, Projectile projectileEntity, int index, float power, float uncertainty,
        float angle, LivingEntity targetOverrride);

    protected Projectile createProjectile(Level level, LivingEntity shooter, ItemStack weapon, ItemStack projectile, boolean isCrit) {
        ArrowItem arrowItem = projectile.getItem() instanceof ArrowItem arrow ? arrow : (ArrowItem) Items.ARROW;
        AbstractArrow arrow = arrowItem.createArrow(level, projectile, shooter, weapon);
        if (isCrit) {
            arrow.setCritArrow(true);
        }
        return arrow;
    }

    protected static List<ItemStack> draw(ItemStack weapon, ItemStack projectile, LivingEntity shooter) {
        if (projectile.isEmpty()) {
            return List.of();
        }
        int count = shooter.level() instanceof ServerLevel serverLevel ? EnchantmentHelper.processProjectileCount(serverLevel, weapon, shooter, 1) : 1;
        List<ItemStack> drawn = new ArrayList<>(count);
        ItemStack spare = projectile.copy();
        for (int i = 0; i < count; i++) {
            ItemStack taken = useAmmo(weapon, i == 0 ? projectile : spare, shooter, i > 0);
            if (!taken.isEmpty()) {
                drawn.add(taken);
            }
        }
        return drawn;
    }

    protected static ItemStack useAmmo(ItemStack weapon, ItemStack projectile, LivingEntity holder, boolean forceInfinite) {
        int ammoToUse = !forceInfinite && !holder.hasInfiniteMaterials() && holder.level() instanceof ServerLevel serverLevel
            ? EnchantmentHelper.processAmmoUse(serverLevel, weapon, projectile, 1) : 0;
        if (ammoToUse > projectile.getCount()) {
            return ItemStack.EMPTY;
        }
        if (ammoToUse == 0) {
            ItemStack copy = projectile.copyWithCount(1);
            copy.set(DataComponents.INTANGIBLE_PROJECTILE, Unit.INSTANCE);
            return copy;
        }
        ItemStack used = projectile.split(ammoToUse);
        if (holder instanceof Player player) {
            var inventory = player.getInventory();
            for (int slot = 0; slot < inventory.getContainerSize(); slot++) {
                if (ItemStack.isSameItemSameComponents(inventory.getItem(slot), used)) {
                    inventory.removeItem(slot, ammoToUse);
                    break;
                }
            }
        }
        return used;
    }
}
