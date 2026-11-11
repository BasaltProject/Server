// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;
import java.util.List;
import java.util.function.Predicate;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureCheckResult;
import net.minecraft.world.level.levelgen.structure.StructureStart;
import net.minecraft.world.level.levelgen.structure.placement.StructurePlacement;
public class EmptyStructureManager extends StructureManager {
    public static final EmptyStructureManager INSTANCE = new EmptyStructureManager();
    private EmptyStructureManager() {
        
    }
    public List<StructureStart> startsForStructure(int sectionX, int sectionZ, Structure structure) {
        return List.of();
    }
    public List<StructureStart> startsForStructure(int sectionX, int sectionZ, Predicate<Structure> matcher) {
        return List.of();
    }
    public boolean shouldGenerateStructures() {
        return false;
    }
    public StructureCheckResult checkStructurePresence(ChunkPos pos, Structure structure, StructurePlacement placement, boolean createReference) {
        return StructureCheckResult.START_NOT_PRESENT;

    }
}
