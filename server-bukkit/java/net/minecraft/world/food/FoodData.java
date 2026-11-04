// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.food;

import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;

public class FoodData {
    private final org.bukkit.entity.Player owner;
    private int foodLevel = 20;
    private float saturationLevel = 5.0F;
    private float exhaustionLevel;
    private int tickTimer;

    public FoodData() {
        this(null);
    }

    public FoodData(org.bukkit.entity.Player owner) {
        this.owner = owner;
    }

    public void eat(int food, float saturationModifier) {
        int level = Math.min(getFoodLevel() + food, 20);
        setFoodLevel(level);
        setSaturation(Math.min(getSaturationLevel() + food * saturationModifier * 2.0F, level));
    }

    public void eat(FoodProperties foodProperties) {
        int level = Math.min(getFoodLevel() + foodProperties.nutrition(), 20);
        setFoodLevel(level);
        setSaturation(Math.min(getSaturationLevel() + foodProperties.saturation(), level));
    }

    public void tick(ServerPlayer player) {
        return;
    }

    public void readAdditionalSaveData(ValueInput input) {
        setFoodLevel(input.getIntOr("foodLevel", 20));
        tickTimer = input.getIntOr("foodTickTimer", 0);
        setSaturation(input.getFloatOr("foodSaturationLevel", 5.0F));
        exhaustionLevel = input.getFloatOr("foodExhaustionLevel", 0.0F);
    }

    public void addAdditionalSaveData(ValueOutput output) {
        output.putInt("foodLevel", getFoodLevel());
        output.putInt("foodTickTimer", tickTimer);
        output.putFloat("foodSaturationLevel", getSaturationLevel());
        output.putFloat("foodExhaustionLevel", owner == null ? exhaustionLevel : owner.getExhaustion());
    }

    public int getFoodLevel() {
        return owner == null ? foodLevel : owner.getFoodLevel();
    }

    public boolean hasEnoughFood() {
        return getFoodLevel() > 17;
    }

    public boolean needsFood() {
        return getFoodLevel() < 20;
    }

    public void addExhaustion(float amount) {
        if (owner == null) {
            exhaustionLevel = Math.min(exhaustionLevel + amount, 40.0F);
        } else {
            owner.setExhaustion(Math.min(owner.getExhaustion() + amount, 40.0F));
        }
    }

    public float getSaturationLevel() {
        return owner == null ? saturationLevel : owner.getSaturation();
    }

    public void setFoodLevel(int food) {
        if (owner == null) {
            foodLevel = food;
        } else {
            owner.setFoodLevel(food);
        }
    }

    public void setSaturation(float saturation) {
        if (owner == null) {
            saturationLevel = saturation;
        } else {
            owner.setSaturation(saturation);
        }
    }
}
