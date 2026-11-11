// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;
import com.mojang.serialization.Codec;
public class GeodeCrackSettings {
    public static final Codec<GeodeCrackSettings> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.GeodeCrackSettings.class, "CODEC");
    public final double generateCrackChance;
    public final double baseCrackSize;
    public final int crackPointOffset;
    public GeodeCrackSettings(double generateCrackChance, double baseCrackSize, int crackPointOffset) {
        this.generateCrackChance = generateCrackChance;
        this.baseCrackSize = baseCrackSize;
        this.crackPointOffset = crackPointOffset;


    }
}
