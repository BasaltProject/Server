// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;
import com.mojang.serialization.Codec;
public class GeodeLayerSettings {
    public static final Codec<GeodeLayerSettings> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.GeodeLayerSettings.class, "CODEC");
    public final double filling;
    public final double innerLayer;
    public final double middleLayer;
    public final double outerLayer;
    public GeodeLayerSettings(double filling, double innerLayer, double middleLayer, double outerLayer) {
        this.filling = filling;
        this.innerLayer = innerLayer;
        this.middleLayer = middleLayer;
        this.outerLayer = outerLayer;
    }
}
