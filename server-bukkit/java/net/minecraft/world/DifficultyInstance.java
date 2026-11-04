// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world;
public class DifficultyInstance {
    protected DifficultyInstance() {
    }
    public DifficultyInstance(Difficulty base, long totalGameTime, long localGameTime, float moonBrightness) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.DifficultyInstance#<init>");
    }
    public Difficulty getDifficulty() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.DifficultyInstance#getDifficulty");
    }
    public float getEffectiveDifficulty() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.DifficultyInstance#getEffectiveDifficulty");
    }
    public boolean isHard() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.DifficultyInstance#isHard");
    }
    public boolean isHarderThan(float requiredDifficulty) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.DifficultyInstance#isHarderThan");
    }
    public float getSpecialMultiplier() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.DifficultyInstance#getSpecialMultiplier");
    }
}
