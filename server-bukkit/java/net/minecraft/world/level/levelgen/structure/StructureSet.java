// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure;
import com.mojang.serialization.Codec;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.world.level.levelgen.structure.placement.StructurePlacement;
public record StructureSet(List<StructureSelectionEntry> structures, StructurePlacement placement) {
    public static final Codec<StructureSet> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.structure.StructureSet.class, "DIRECT_CODEC");
    public static final Codec<Holder<StructureSet>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.structure.StructureSet.class, "CODEC");
    public StructureSet(Holder<Structure> singleEntry, StructurePlacement placement) {
        this(List.of(new StructureSelectionEntry(singleEntry, 1)), placement);
    }
    public static StructureSelectionEntry entry(Holder<Structure> structure, int weight) {
        return new StructureSelectionEntry(structure, weight);
    }
    public static StructureSelectionEntry entry(Holder<Structure> structure) {
        return new StructureSelectionEntry(structure, 1);
    }
    public static record StructureSelectionEntry(Holder<Structure> structure, int weight) {
        public static final Codec<StructureSelectionEntry> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.structure.StructureSet.StructureSelectionEntry.class, "CODEC");
    }
}
