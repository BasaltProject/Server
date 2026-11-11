// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import com.mojang.serialization.MapCodec;
public class NopProcessor implements StructureProcessor {
    public static final MapCodec<NopProcessor> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.NopProcessor.class, "MAP_CODEC");
    public static final NopProcessor INSTANCE = new NopProcessor();
    private NopProcessor() {
        
    }
    public MapCodec<NopProcessor> codec() {
        return MAP_CODEC;
    }
}
