// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.jfr.event;
import jdk.jfr.Category;
import jdk.jfr.Enabled;
import jdk.jfr.Event;
import jdk.jfr.EventType;
import jdk.jfr.Label;
import jdk.jfr.Name;
import jdk.jfr.StackTrace;
import net.minecraft.core.Holder;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.levelgen.structure.Structure;
@Name("minecraft.StructureGeneration")
@Label("Structure Generation")
@Category({"Minecraft", "World Generation"})
@StackTrace(false)
@Enabled(false)
public class StructureGenerationEvent extends Event {
    public static final String EVENT_NAME = "minecraft.StructureGeneration";
    public static final EventType TYPE = EventType.getEventType(StructureGenerationEvent.class);
    @Name(Fields.CHUNK_POS_X)
    @Label("Chunk X Position")
    public final int chunkPosX;
    @Name(Fields.CHUNK_POS_Z)
    @Label("Chunk Z Position")
    public final int chunkPosZ;
    @Name(Fields.STRUCTURE)
    @Label("Structure")
    public final String structure;
    @Name(Fields.LEVEL)
    @Label("Level Id")
    public final String level;
    @Name(Fields.SUCCESS)
    @Label("Success")
    public boolean success;
    public StructureGenerationEvent(ChunkPos sourceChunkPos, Holder<Structure> structure, ResourceKey<Level> level) {
        this.chunkPosX = sourceChunkPos.x();
        this.chunkPosZ = sourceChunkPos.z();
        this.structure = structure.unwrapKey().map(key -> key.identifier().toString()).orElse("unregistered");
        this.level = level.toString();
    }
    public interface Fields {
        String CHUNK_POS_X = "chunkPosX";
        String CHUNK_POS_Z = "chunkPosZ";
        String STRUCTURE = "structure";
        String LEVEL = "level";
        String SUCCESS = "success";
    }
}
