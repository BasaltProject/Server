// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import java.util.List;
public class StructureProcessorList {
    private final List<StructureProcessor> processors;
    public StructureProcessorList(List<StructureProcessor> list) {
        this.processors = list;
    }
    public List<StructureProcessor> list() {
        return processors;
    }
    public String toString() {
        return "ProcessorList[" + processors + "]";
    }
}
