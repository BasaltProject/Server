// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen;
import net.minecraft.world.level.levelgen.synth.NormalNoise;
public class NoiseData {
    public static final NormalNoise DEFAULT_SHIFT = NormalNoise.createParity(-3, 1.0, 1.0, 1.0, 0.0);
    public static void bootstrap(BootstrapContext<NormalNoise> context) {
    }
}
