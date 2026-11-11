// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.rootplacers;
import com.mojang.serialization.MapCodec;
public class RootPlacerType<P extends RootPlacer> {
    private final MapCodec<P> codec;
    public static final RootPlacerType<MangroveRootPlacer> MANGROVE_ROOT_PLACER = new RootPlacerType<>(MangroveRootPlacer.CODEC);
    private RootPlacerType(MapCodec<P> codec) {
        this.codec = codec;
    }
    public MapCodec<P> codec() {
        return codec;
    }
}
