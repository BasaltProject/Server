// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureType;
public class StrongholdStructure extends Structure {
    public static final MapCodec<StrongholdStructure> CODEC = simpleCodec(StrongholdStructure::new);
    public StrongholdStructure(Structure.StructureSettings settings) {
        super(settings);
    }
    public Optional<Structure.GenerationStub> findGenerationPoint(Structure.GenerationContext context) {
        return Optional.empty();
    }
    public StructureType<?> type() {
        return StructureType.STRONGHOLD;
    }
}
