package org.basaltmc.bukkit.player;

import java.util.UUID;
import net.kyori.adventure.util.TriState;
import org.bukkit.GameMode;
import org.bukkit.attribute.Attribute;
import org.bukkit.attribute.AttributeInstance;

public abstract class PlayerState extends PlayerHud {
    private static final double kDefaultHealthScale = 20.0;
    private static final int kVanillaSaturatedRegenRate = 10;
    private static final int kVanillaUnsaturatedRegenRate = 80;
    private static final int kVanillaStarvationRate = 80;

    private volatile double healthScaleValue = kDefaultHealthScale;

    private UUID key() {
        return getUniqueId();
    }

    @Override
    public boolean getAllowFlight() {
        return PlayerCalls.flag(key(), "allow_flight");
    }

    @Override
    public void setAllowFlight(boolean flight) {
        PlayerCalls.set(key(), "allow_flight", flight);
    }

    @Override
    public boolean isFlying() {
        return PlayerCalls.flag(key(), "flying");
    }

    @Override
    public void setFlying(boolean value) {
        if (value && !getAllowFlight()) {
            throw new IllegalArgumentException("Cannot make player fly if getAllowFlight() is false");
        }
        PlayerCalls.set(key(), "flying", value);
    }

    @Override
    public float getFlySpeed() {
        return (float) PlayerCalls.real(key(), "fly_speed");
    }

    @Override
    public void setFlySpeed(float value) {
        validateSpeed(value);
        PlayerCalls.set(key(), "fly_speed", (double) value);
    }

    @Override
    public float getWalkSpeed() {
        return (float) PlayerCalls.real(key(), "walk_speed");
    }

    @Override
    public void setWalkSpeed(float value) {
        validateSpeed(value);
        PlayerCalls.set(key(), "walk_speed", (double) value);
    }

    private static void validateSpeed(float value) {
        if (value < -1.0F || value > 1.0F) {
            throw new IllegalArgumentException(value + " is too " + (value < 0 ? "low" : "high"));
        }
    }

    @Override
    public boolean isSprinting() {
        return PlayerCalls.flag(key(), "sprinting");
    }

    @Override
    public void setSprinting(boolean sprinting) {
        PlayerCalls.set(key(), "sprinting", sprinting);
    }

    @Override
    public void setSneaking(boolean sneak) {
        PlayerCalls.set(key(), "sneaking", sneak);
    }

    @Override
    public boolean isGliding() {
        return PlayerCalls.flag(key(), "gliding");
    }

    @Override
    public void setGliding(boolean gliding) {
        PlayerCalls.set(key(), "gliding", gliding);
    }

    @Override
    public boolean isSwimming() {
        return PlayerCalls.flag(key(), "swimming");
    }

    @Override
    public void setSwimming(boolean swimming) {
        PlayerCalls.set(key(), "swimming", swimming);
    }

    @Override
    public float getExhaustion() {
        return (float) PlayerCalls.real(key(), "exhaustion");
    }

    @Override
    public void setExhaustion(float value) {
        PlayerCalls.set(key(), "exhaustion", (double) value);
    }

    @Override
    public void setSaturation(float value) {
        PlayerCalls.set(key(), "saturation", (double) value);
    }

    @Override
    public void setTotalExperience(int exp) {
        if (exp < 0) {
            throw new IllegalArgumentException("Experience cannot be negative");
        }
        PlayerCalls.set(key(), "total_experience", (long) exp);
    }

    @Override
    public void giveExp(int amount) {
        PlayerCalls.call(key(), "give_exp", (long) amount);
    }

    @Override
    public void giveExp(int amount, boolean applyMending) {
        if (applyMending && amount > 0) {
            amount = applyMending(amount);
        }
        PlayerCalls.call(key(), "give_exp", (long) amount);
    }

    @Override
    public int applyMending(int amount) {
        int left = amount;
        for (org.bukkit.inventory.EquipmentSlot slot : org.bukkit.inventory.EquipmentSlot.values()) {
            if (left <= 0 || !slot.isHand() && !slot.isArmor()) {
                continue;
            }
            org.bukkit.inventory.ItemStack stack = getInventory().getItem(slot);
            if (stack == null || stack.getType().isAir() || stack.getType().getMaxDurability() <= 0
                || stack.getEnchantmentLevel(org.bukkit.enchantments.Enchantment.MENDING) <= 0) {
                continue;
            }
            org.bukkit.inventory.meta.Damageable meta = (org.bukkit.inventory.meta.Damageable) stack.getItemMeta();
            if (meta == null || meta.getDamage() <= 0) {
                continue;
            }
            int repaired = Math.min(meta.getDamage(), left * 2);
            meta.setDamage(meta.getDamage() - repaired);
            stack.setItemMeta(meta);
            getInventory().setItem(slot, stack);
            left -= (repaired + 1) / 2;
        }
        return left;
    }

    @Override
    public void giveExpLevels(int amount) {
        PlayerCalls.call(key(), "give_levels", (long) amount);
    }

    @Override
    public int getExpToLevel() {
        return (int) PlayerCalls.integer(key(), "exp_to_next_level");
    }

    @Override
    public int getExperiencePointsNeededForNextLevel() {
        return getExpToLevel();
    }

    @Override
    public int calculateTotalExperiencePoints() {
        int level = getLevel();
        double points = level <= 16 ? level * level + 6.0 * level
            : level <= 31 ? 2.5 * level * level - 40.5 * level + 360.0 : 4.5 * level * level - 162.5 * level + 2220.0;
        return (int) (points + getExp() * getExpToLevel());
    }

    @Override
    public void setExperienceLevelAndProgress(int experience) {
        int level = 0;
        int left = experience;
        while (true) {
            int needed = level >= 30 ? 112 + (level - 30) * 9 : level >= 15 ? 37 + (level - 15) * 5 : 7 + level * 2;
            if (left < needed) {
                setLevel(level);
                setExp(needed == 0 ? 0.0F : (float) left / needed);
                return;
            }
            left -= needed;
            level++;
        }
    }

    @Override
    public void sendExperienceChange(float progress) {
        sendExperienceChange(progress, getLevel());
    }

    @Override
    public void sendExperienceChange(float progress, int level) {
        PlayerCalls.call(key(), "fake_experience", (double) progress, (long) level);
    }

    @Override
    public void sendHealthUpdate() {
        PlayerCalls.call(key(), "send_health");
    }

    @Override
    public void sendHealthUpdate(double health, int foodLevel, float saturation) {
        PlayerCalls.call(key(), "fake_health", health, (long) foodLevel, (double) saturation);
    }

    @Override
    public boolean isHealthScaled() {
        return PlayerCalls.get(key(), "health_scale") != null;
    }

    @Override
    public void setHealthScaled(boolean scale) {
        PlayerCalls.set(key(), "health_scale", scale ? (Object) healthScaleValue : null);
    }

    @Override
    public void setHealthScale(double scale) {
        if (scale <= 0.0 || Double.isNaN(scale) || Double.isInfinite(scale)) {
            throw new IllegalArgumentException("scale must be greater than 0 and finite");
        }
        healthScaleValue = scale;
        PlayerCalls.set(key(), "health_scale", scale);
    }

    @Override
    public double getHealthScale() {
        return healthScaleValue;
    }

    @Override
    public boolean isSleepingIgnored() {
        return PlayerCalls.flag(key(), "sleeping_ignored");
    }

    @Override
    public void setSleepingIgnored(boolean isSleeping) {
        PlayerCalls.set(key(), "sleeping_ignored", isSleeping);
    }

    @Override
    public boolean isCollidable() {
        return PlayerCalls.flag(key(), "collidable");
    }

    @Override
    public void setCollidable(boolean collidable) {
        PlayerCalls.set(key(), "collidable", collidable);
    }

    @Override
    public boolean getAffectsSpawning() {
        return PlayerCalls.flag(key(), "affects_spawning");
    }

    @Override
    public void setAffectsSpawning(boolean affects) {
        PlayerCalls.set(key(), "affects_spawning", affects);
    }

    @Override
    public TriState hasFlyingFallDamage() {
        return PlayerCalls.flag(key(), "flying_fall_damage") ? TriState.TRUE : TriState.FALSE;
    }

    @Override
    public void setFlyingFallDamage(TriState flyingFallDamage) {
        PlayerCalls.set(key(), "flying_fall_damage", flyingFallDamage == TriState.TRUE);
    }

    @Override
    public int getExpCooldown() {
        return (int) PlayerCalls.integer(key(), "exp_cooldown");
    }

    @Override
    public void setExpCooldown(int ticks) {
        PlayerCalls.set(key(), "exp_cooldown", (long) ticks);
    }

    @Override
    public float getAttackCooldown() {
        return (float) PlayerCalls.real(key(), "attack_cooldown");
    }

    @Override
    public float getCooledAttackStrength(float adjustTicks) {
        double delay = getCooldownPeriod();
        double ticks = getAttackCooldown() * delay + adjustTicks;
        return (float) Math.max(0.0, Math.min(1.0, ticks / delay));
    }

    @Override
    public float getCooldownPeriod() {
        AttributeInstance speed = getAttribute(Attribute.ATTACK_SPEED);
        double value = speed == null || speed.getValue() <= 0.0 ? 4.0 : speed.getValue();
        return (float) (20.0 / value);
    }

    @Override
    public void resetCooldown() {
        PlayerCalls.call(key(), "reset_attack_cooldown");
    }

    @Override
    public int getSaturatedRegenRate() {
        return kVanillaSaturatedRegenRate;
    }

    @Override
    public int getUnsaturatedRegenRate() {
        return kVanillaUnsaturatedRegenRate;
    }

    @Override
    public int getStarvationRate() {
        return kVanillaStarvationRate;
    }

    @Override
    public int getEnchantmentSeed() {
        return (int) PlayerCalls.integer(key(), "enchantment_seed");
    }

    @Override
    public void setEnchantmentSeed(int seed) {
        PlayerCalls.set(key(), "enchantment_seed", (long) seed);
    }

    @Override
    public int getDeathScreenScore() {
        return (int) PlayerCalls.integer(key(), "score");
    }

    @Override
    public void setDeathScreenScore(int score) {
        PlayerCalls.set(key(), "score", (long) score);
    }

    @Override
    public GameMode getPreviousGameMode() {
        String text = PlayerCalls.text(key(), "previous_gamemode");
        return text == null || text.isEmpty() ? null : GameMode.valueOf(text.toUpperCase(java.util.Locale.ROOT));
    }
}
