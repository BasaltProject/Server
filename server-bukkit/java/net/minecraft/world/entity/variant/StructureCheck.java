// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.variant;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.HolderSet;
import net.minecraft.world.level.levelgen.structure.Structure;
public record StructureCheck(HolderSet<Structure> requiredStructures) implements SpawnCondition {
    public static final MapCodec<StructureCheck> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.entity.variant.StructureCheck.class, "MAP_CODEC");
    public boolean test(SpawnContext context) {
        return false;
    }
    public MapCodec<StructureCheck> codec() {
        return MAP_CODEC;
    }
}
