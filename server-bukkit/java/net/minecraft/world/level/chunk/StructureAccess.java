// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk;

import it.unimi.dsi.fastutil.longs.LongSet;
import java.util.Map;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureStart;
public interface StructureAccess {
    default StructureStart getStartForStructure(Structure structure) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.StructureAccess#getStartForStructure");
    }
    default void setStartForStructure(Structure structure, StructureStart structureStart) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.StructureAccess#setStartForStructure");
    }
    default LongSet getReferencesForStructure(Structure structure) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.StructureAccess#getReferencesForStructure");
    }
    default void addReferenceForStructure(Structure structure, long reference) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.StructureAccess#addReferenceForStructure");
    }
    default Map<Structure, LongSet> getAllReferences() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.StructureAccess#getAllReferences");
    }
    default void setAllReferences(Map<Structure, LongSet> data) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.StructureAccess#setAllReferences");
    }
}
