package org.basaltmc.bukkit.entity.ops;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import org.basaltmc.bukkit.entity.EntityCore;
import org.bukkit.Location;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.attribute.Attribute;
import org.bukkit.attribute.AttributeInstance;
import org.bukkit.damage.DamageSource;
import org.bukkit.entity.Entity;
import org.bukkit.entity.EntityType;
import org.bukkit.entity.LivingEntity;
import org.bukkit.event.entity.EntityRegainHealthEvent;
import org.bukkit.inventory.EntityEquipment;
import org.bukkit.potion.PotionEffect;
import org.bukkit.potion.PotionEffectType;

public interface LivingEntityOps extends LivingEntity, EntityOps {
    default double getHealth() {
        return core().isTemplate() ? (core().property("health") instanceof Number staged ? staged.doubleValue()
            : getMaxHealth()) : core().number(EntityCore.kHealth);
    }

    default void setHealth(double health) {
        if (!core().isTemplate() && (Double.isNaN(health) || health < 0.0 || health > getMaxHealth())) {
            throw new IllegalArgumentException("Health must be between 0 and " + getMaxHealth() + ", but was "
                + health);
        }
        if (health == 0.0) {
            core().call("kill");
        } else {
            core().set("health", health);
        }
    }

    default double getMaxHealth() {
        return core().isTemplate() && !(core().property("max_health") instanceof Number) ? 20.0
            : core().real("max_health");
    }

    default void setMaxHealth(double health) {
        core().set("max_health", health);
    }

    default void resetMaxHealth() {
        AttributeInstance instance = getAttribute(Attribute.MAX_HEALTH);
        if (instance != null) {
            instance.setBaseValue(instance.getDefaultValue());
        }
    }

    default double getAbsorptionAmount() {
        return core().real("absorption");
    }

    default void setAbsorptionAmount(double amount) {
        core().set("absorption", Math.max(0.0, amount));
    }

    default void damage(double amount) {
        core().call("damage", amount, "generic", -1);
    }

    default void damage(double amount, Entity source) {
        core().call("damage", amount, source instanceof org.bukkit.entity.Player ? "player_attack" : "mob_attack",
            core().idOf(source));
    }

    default void damage(double amount, DamageSource source) {
        String cause = source.getDamageType().getKey().getKey();
        Entity causing = source.getCausingEntity() != null ? source.getCausingEntity() : source.getDirectEntity();
        core().call("damage", amount, cause, core().idOf(causing));
    }

    default void heal(double amount, EntityRegainHealthEvent.RegainReason reason) {
        EntityRegainHealthEvent event = new EntityRegainHealthEvent(this, amount, reason);
        org.bukkit.Bukkit.getPluginManager().callEvent(event);
        if (!event.isCancelled()) {
            core().set("health", Math.min(getMaxHealth(), getHealth() + event.getAmount()));
        }
    }

    default void kill(DamageSource source) {
        core().call("kill");
    }

    default AttributeInstance getAttribute(Attribute attribute) {
        return core().attribute(attribute);
    }

    default double getEyeHeight() {
        return getHeight() * 0.85;
    }

    default double getEyeHeight(boolean ignorePose) {
        return getEyeHeight();
    }

    default Location getEyeLocation() {
        Location here = getLocation();
        here.setY(here.getY() + getEyeHeight());
        return here;
    }

    default boolean hasAI() {
        return !core().flag("no_ai");
    }

    default void setAI(boolean ai) {
        core().set("no_ai", !ai);
    }

    default EntityEquipment getEquipment() {
        return core().equipment(this);
    }

    default double getLastDamage() {
        return core().real("last_damage");
    }

    default void setLastDamage(double damage) {
        core().set("last_damage", damage);
    }

    default int getNoDamageTicks() {
        return core().integer("no_damage_ticks");
    }

    default void setNoDamageTicks(int ticks) {
        core().set("no_damage_ticks", ticks);
    }

    default int getMaximumNoDamageTicks() {
        return 20;
    }

    default int getArrowsInBody() {
        return core().integer("arrows_in_body");
    }

    default void setArrowsInBody(int count, boolean fireEvent) {
        core().set("arrows_in_body", Math.max(0, count));
    }

    default int getRemainingAir() {
        return core().integer("air");
    }

    default void setRemainingAir(int air) {
        core().set("air", air);
    }

    default int getMaximumAir() {
        return core().integer("max_air");
    }

    default boolean getCanPickupItems() {
        return core().flag("can_pick_up_loot");
    }

    default void setCanPickupItems(boolean pickup) {
        core().set("can_pick_up_loot", pickup);
    }

    default boolean getRemoveWhenFarAway() {
        return !core().flag("persistent");
    }

    default void setRemoveWhenFarAway(boolean remove) {
        core().set("persistent", !remove);
    }

    default boolean isSleeping() {
        return core().flag("sleeping");
    }

    default boolean isLeashed() {
        return core().integer("leash_holder") >= 0;
    }

    default Entity getLeashHolder() {
        Entity holder = core().entity(core().integer("leash_holder"));
        if (holder == null) {
            throw new IllegalStateException("Entity not leashed");
        }
        return holder;
    }

    default boolean setLeashHolder(Entity holder) {
        return core().set("leash_holder", holder == null ? -1 : holder.getEntityId());
    }

    default Collection<PotionEffect> getActivePotionEffects() {
        List<PotionEffect> out = new ArrayList<>();
        if (core().call("effects") instanceof String[] flat) {
            for (int at = 0; at + 5 < flat.length; at += 6) {
                PotionEffectType type = Registry.EFFECT.get(NamespacedKey.fromString(flat[at]));
                if (type != null) {
                    out.add(new PotionEffect(type, Integer.parseInt(flat[at + 1]), Integer.parseInt(flat[at + 2]),
                        flat[at + 3].equals("1"), flat[at + 4].equals("1"), flat[at + 5].equals("1")));
                }
            }
        }
        return out;
    }

    default boolean addPotionEffect(PotionEffect effect) {
        return core().test("add_effect", effect.getType().getKey().toString(), effect.getDuration(),
            effect.getAmplifier(), effect.isAmbient(), effect.hasParticles(), effect.hasIcon());
    }

    default boolean addPotionEffects(Collection<PotionEffect> effects) {
        boolean all = true;
        for (PotionEffect effect : effects) {
            all &= addPotionEffect(effect);
        }
        return all;
    }

    default boolean hasPotionEffect(PotionEffectType type) {
        return core().test("has_effect", type.getKey().toString());
    }

    default PotionEffect getPotionEffect(PotionEffectType type) {
        for (PotionEffect effect : getActivePotionEffects()) {
            if (effect.getType().equals(type)) {
                return effect;
            }
        }
        return null;
    }

    default void removePotionEffect(PotionEffectType type) {
        core().call("remove_effect", type.getKey().toString());
    }

    default boolean clearActivePotionEffects() {
        boolean any = !getActivePotionEffects().isEmpty();
        core().call("clear_effects");
        return any;
    }

    default void knockback(double strength, double directionX, double directionZ) {
        core().call("knockback", strength, directionX, directionZ);
    }

    default <T extends org.bukkit.entity.Projectile> T launchProjectile(Class<? extends T> type) {
        return launchProjectile(type, null);
    }

    default <T extends org.bukkit.entity.Projectile> T launchProjectile(Class<? extends T> type,
            org.bukkit.util.Vector velocity) {
        return launchProjectile(type, velocity, null);
    }

    @SuppressWarnings("unchecked")
    default <T extends org.bukkit.entity.Projectile> T launchProjectile(Class<? extends T> type,
            org.bukkit.util.Vector velocity, java.util.function.Consumer<? super T> before) {
        org.bukkit.entity.EntityType kind = org.basaltmc.bukkit.entity.EntitySpawner.typeOf(type);
        Location eye = getEyeLocation();
        org.bukkit.util.Vector direction = eye.getDirection();
        double speed = switch (kind) {
            case ARROW, SPECTRAL_ARROW -> 3.0;
            case TRIDENT -> 2.5;
            case WIND_CHARGE, BREEZE_WIND_CHARGE -> 1.5;
            case FIREBALL, SMALL_FIREBALL, DRAGON_FIREBALL, WITHER_SKULL -> 0.1;
            default -> 1.5;
        };
        org.bukkit.util.Vector launch = velocity != null ? velocity : direction.multiply(speed);
        Entity made = org.basaltmc.bukkit.entity.EntitySpawner.spawn(core().world, eye, kind,
            org.bukkit.event.entity.CreatureSpawnEvent.SpawnReason.DEFAULT, projectile -> {
                if (projectile instanceof org.basaltmc.bukkit.entity.Cored cored) {
                    cored.core().set("owner_id", getEntityId());
                    cored.core().velocity(launch);
                }
                if (before != null) {
                    before.accept((T) projectile);
                }
            });
        return (T) made;
    }

    default org.bukkit.entity.Player getKiller() {
        String killer = core().text("last_hurt_by_player");
        return killer == null ? null : org.bukkit.Bukkit.getPlayer(java.util.UUID.fromString(killer));
    }

    default org.bukkit.util.RayTraceResult rayTraceEntities(int maxDistance, boolean ignoreBlocks) {
        Location eye = getEyeLocation();
        org.bukkit.util.Vector direction = eye.getDirection();
        double limit = maxDistance;
        if (!ignoreBlocks) {
            try {
                org.bukkit.util.RayTraceResult block = core().world.rayTraceBlocks(eye, direction, maxDistance,
                    org.bukkit.FluidCollisionMode.NEVER, true);
                if (block != null) {
                    limit = block.getHitPosition().distance(eye.toVector());
                }
            } catch (UnsupportedOperationException unavailable) {
                limit = maxDistance;
            }
        }
        org.bukkit.util.RayTraceResult nearest = null;
        double best = Double.MAX_VALUE;
        for (Entity other : core().world.getNearbyEntities(eye, maxDistance, maxDistance, maxDistance,
            candidate -> candidate != this)) {
            org.bukkit.util.RayTraceResult hit = other.getBoundingBox().expand(0.1)
                .rayTrace(eye.toVector(), direction, limit);
            if (hit != null) {
                double distance = hit.getHitPosition().distanceSquared(eye.toVector());
                if (distance < best) {
                    best = distance;
                    nearest = new org.bukkit.util.RayTraceResult(hit.getHitPosition(), other, hit.getHitBlockFace());
                }
            }
        }
        return nearest;
    }

    default Entity getTargetEntity(int maxDistance, boolean ignoreBlocks) {
        org.bukkit.util.RayTraceResult result = rayTraceEntities(maxDistance, ignoreBlocks);
        return result == null ? null : result.getHitEntity();
    }

    default org.bukkit.entity.EntityCategory getCategory() {
        EntityType type = core().type;
        return org.bukkit.Tag.ENTITY_TYPES_UNDEAD.isTagged(type) ? org.bukkit.entity.EntityCategory.UNDEAD
            : org.bukkit.Tag.ENTITY_TYPES_ARTHROPOD.isTagged(type) ? org.bukkit.entity.EntityCategory.ARTHROPOD
            : org.bukkit.Tag.ENTITY_TYPES_ILLAGER.isTagged(type) ? org.bukkit.entity.EntityCategory.ILLAGER
            : org.bukkit.Tag.ENTITY_TYPES_AQUATIC.isTagged(type) ? org.bukkit.entity.EntityCategory.WATER
            : org.bukkit.entity.EntityCategory.NONE;
    }

    default boolean canBreatheUnderwater() {
        return org.bukkit.Tag.ENTITY_TYPES_CAN_BREATHE_UNDER_WATER.isTagged(core().type);
    }

    default boolean hasLineOfSight(Entity other) {
        return hasLineOfSight(other.getLocation().add(0, other.getHeight() * 0.85, 0));
    }

    default boolean hasLineOfSight(Location location) {
        Location eye = getEyeLocation();
        org.bukkit.util.Vector way = location.toVector().subtract(eye.toVector());
        double distance = way.length();
        if (distance < 1.0E-6) {
            return true;
        }
        return core().world.rayTraceBlocks(eye, way.normalize(), distance, org.bukkit.FluidCollisionMode.NEVER,
            true) == null;
    }

    default void attack(Entity target) {
        double damage = getAttribute(Attribute.ATTACK_DAMAGE) == null ? 1.0
            : getAttribute(Attribute.ATTACK_DAMAGE).getValue();
        if (target instanceof LivingEntity living) {
            living.damage(damage, this);
        }
    }

    private List<org.bukkit.block.Block> sightLine(java.util.Set<org.bukkit.Material> transparent, int maxDistance,
            int keep) {
        List<org.bukkit.block.Block> blocks = new ArrayList<>();
        org.bukkit.util.BlockIterator iterator = new org.bukkit.util.BlockIterator(this, Math.min(maxDistance, 120));
        while (iterator.hasNext()) {
            org.bukkit.block.Block block = iterator.next();
            blocks.add(block);
            if (keep != 0 && blocks.size() > keep) {
                blocks.remove(0);
            }
            org.bukkit.Material material = block.getType();
            if (transparent == null ? !material.isAir() : !transparent.contains(material)) {
                break;
            }
        }
        return blocks;
    }

    default List<org.bukkit.block.Block> getLineOfSight(java.util.Set<org.bukkit.Material> transparent,
            int maxDistance) {
        return sightLine(transparent, maxDistance, 0);
    }

    default org.bukkit.block.Block getTargetBlock(java.util.Set<org.bukkit.Material> transparent, int maxDistance) {
        return sightLine(transparent, maxDistance, 1).get(0);
    }

    default List<org.bukkit.block.Block> getLastTwoTargetBlocks(java.util.Set<org.bukkit.Material> transparent,
            int maxDistance) {
        return sightLine(transparent, maxDistance, 2);
    }

    default org.bukkit.util.RayTraceResult rayTraceBlocks(double maxDistance, org.bukkit.FluidCollisionMode mode) {
        Location eye = getEyeLocation();
        return core().world.rayTraceBlocks(eye, eye.getDirection(), maxDistance, mode, false);
    }

    default org.bukkit.block.Block getTargetBlockExact(int maxDistance, org.bukkit.FluidCollisionMode mode) {
        org.bukkit.util.RayTraceResult result = rayTraceBlocks(maxDistance, mode);
        return result == null ? null : result.getHitBlock();
    }

    default org.bukkit.block.BlockFace getTargetBlockFace(int maxDistance, org.bukkit.FluidCollisionMode mode) {
        org.bukkit.util.RayTraceResult result = rayTraceBlocks(maxDistance, mode);
        return result == null ? null : result.getHitBlockFace();
    }

    default boolean canUseEquipmentSlot(org.bukkit.inventory.EquipmentSlot slot) {
        return getType() != org.bukkit.entity.EntityType.PLAYER
            || slot != org.bukkit.inventory.EquipmentSlot.BODY && slot != org.bukkit.inventory.EquipmentSlot.SADDLE;
    }

    default float getSoundPitch() {
        java.util.concurrent.ThreadLocalRandom random = java.util.concurrent.ThreadLocalRandom.current();
        float pitch = (random.nextFloat() - random.nextFloat()) * 0.2F + 1.0F;
        return this instanceof org.bukkit.entity.Ageable ageable && !ageable.isAdult() ? pitch + 0.5F : pitch;
    }

    default org.bukkit.inventory.ItemStack damageItemStack(org.bukkit.inventory.ItemStack stack, int amount) {
        return stack == null || stack.getType().isAir() ? stack : stack.damage(amount, this);
    }

    default void damageItemStack(org.bukkit.inventory.EquipmentSlot slot, int amount) {
        org.bukkit.inventory.ItemStack held = getEquipment().getItem(slot);
        if (held != null && !held.getType().isAir()) {
            getEquipment().setItem(slot, held.damage(amount, this));
        }
    }
}
