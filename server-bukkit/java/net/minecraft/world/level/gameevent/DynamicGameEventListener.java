// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.gameevent;
import net.minecraft.world.level.chunk.status.ChunkStatus;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.chunk.ChunkAccess;
import java.util.function.Consumer;
import net.minecraft.core.SectionPos;
import net.minecraft.server.level.ServerLevel;
public class DynamicGameEventListener<T extends GameEventListener> {
    private static void withRegistry(LevelReader level, SectionPos section, Consumer<GameEventListenerRegistry> action) {
        if (section == null) {
            return;
        }
        ChunkAccess chunk = level.getChunk(section.x(), section.z(), ChunkStatus.FULL, false);
        if (chunk != null) {
            action.accept(chunk.getListenerRegistry(section.y()));
        }
    }
    private final T listener;
    private SectionPos lastSection;
    public DynamicGameEventListener(T listener) {
        this.listener = listener;
    }
    public void add(ServerLevel level) {
        move(level);
    }
    public T getListener() {
        return listener;
    }
    public void remove(ServerLevel level) {
        withRegistry(level, lastSection, registry -> registry.unregister(listener));
    }
    public void move(ServerLevel level) {
        listener.getListenerSource().getPosition(level).map(SectionPos::of).ifPresent(current -> {
            if (current.equals(lastSection)) {
                return;
            }
            withRegistry(level, lastSection, registry -> registry.unregister(listener));
            lastSection = current;
            withRegistry(level, current, registry -> registry.register(listener));
        });
    }
}
