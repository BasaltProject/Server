// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.jfr.event;
import jdk.jfr.Category;
import jdk.jfr.Enabled;
import jdk.jfr.Event;
import jdk.jfr.EventType;
import jdk.jfr.Label;
import jdk.jfr.Name;
import jdk.jfr.StackTrace;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.Level;
@Name("minecraft.ChunkGeneration")
@Label("Chunk Generation")
@Category({"Minecraft", "World Generation"})
@StackTrace(false)
@Enabled(false)
public class ChunkGenerationEvent extends Event {
    public static final String EVENT_NAME = "minecraft.ChunkGeneration";
    public static final EventType TYPE = EventType.getEventType(ChunkGenerationEvent.class);
    @Name(Fields.WORLD_POS_X)
    @Label("First Block X World Position")
    public final int worldPosX;
    @Name(Fields.WORLD_POS_Z)
    @Label("First Block Z World Position")
    public final int worldPosZ;
    @Name(Fields.CHUNK_POS_X)
    @Label("Chunk X Position")
    public final int chunkPosX;
    @Name(Fields.CHUNK_POS_Z)
    @Label("Chunk Z Position")
    public final int chunkPosZ;
    @Name(Fields.STATUS)
    @Label("Status")
    public final String targetStatus;
    @Name(Fields.LEVEL)
    @Label("Level Id")
    public final String level;
    public ChunkGenerationEvent(ChunkPos pos, ResourceKey<Level> dimension, String name) {
        this.targetStatus = name;
        this.level = dimension.toString();
        this.chunkPosX = pos.x();
        this.chunkPosZ = pos.z();
        this.worldPosX = pos.getMinBlockX();
        this.worldPosZ = pos.getMinBlockZ();
    }
    public static class Fields {
        public static final String WORLD_POS_X = "worldPosX";
        public static final String WORLD_POS_Z = "worldPosZ";
        public static final String CHUNK_POS_X = "chunkPosX";
        public static final String CHUNK_POS_Z = "chunkPosZ";
        public static final String STATUS = "status";
        public static final String LEVEL = "level";
        private Fields() {
        }
    }
}
