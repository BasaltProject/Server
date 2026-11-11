// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.entity;
import java.util.List;
import java.util.stream.Stream;
import net.minecraft.world.level.ChunkPos;
public class ChunkEntities<T> {
    private final ChunkPos pos;
    private final List<T> entities;
    public ChunkEntities(ChunkPos pos, List<T> entities) {
        this.pos = pos;
        this.entities = entities;
    }
    public ChunkPos getPos() {
        return pos;
    }
    public Stream<T> getEntities() {
        return entities.stream();
    }
    public boolean isEmpty() {
        return entities.isEmpty();
    }
}
