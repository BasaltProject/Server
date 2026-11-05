// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world;
public class DifficultyInstance {
    private Difficulty base = Difficulty.PEACEFUL;
    private float effective;
    protected DifficultyInstance() {
    }
    public DifficultyInstance(Difficulty base, long totalGameTime, long localGameTime, float moonBrightness) {
        this.base = base;
        if (base == Difficulty.PEACEFUL) {
            return;
        }
        float global = net.minecraft.util.Mth.clamp((totalGameTime - 72000.0F) / 1440000.0F, 0.0F, 1.0F) * 0.25F;
        float local = net.minecraft.util.Mth.clamp(localGameTime / 3600000.0F, 0.0F, 1.0F) * (base == Difficulty.HARD ? 1.0F : 0.75F);
        local += net.minecraft.util.Mth.clamp(moonBrightness * 0.25F, 0.0F, global);
        if (base == Difficulty.EASY) {
            local *= 0.5F;
        }
        this.effective = base.getId() * (0.75F + global + local);
    }
    public Difficulty getDifficulty() {
        return base;
    }
    public float getEffectiveDifficulty() {
        return effective;
    }
    public boolean isHard() {
        return effective >= Difficulty.HARD.ordinal();
    }
    public boolean isHarderThan(float requiredDifficulty) {
        return effective > requiredDifficulty;
    }
    public float getSpecialMultiplier() {
        return effective < 2.0F ? 0.0F : effective > 4.0F ? 1.0F : (effective - 2.0F) / 2.0F;
    }
}
