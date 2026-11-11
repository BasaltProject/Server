// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.gameevent;
import java.util.Collections;
import java.util.List;
import java.util.ArrayList;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.core.SectionPos;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.phys.Vec3;
public class GameEventDispatcher {
    private final ServerLevel level;
    public GameEventDispatcher(ServerLevel level) {
        this.level = level;
    }
    public void post(Holder<GameEvent> gameEvent, Vec3 position, GameEvent.Context context) {
        int radius = gameEvent.value().notificationRadius();
        BlockPos center = BlockPos.containing(position);
        int minX = SectionPos.blockToSectionCoord(center.getX() - radius);
        int minY = SectionPos.blockToSectionCoord(center.getY() - radius);
        int minZ = SectionPos.blockToSectionCoord(center.getZ() - radius);
        int maxX = SectionPos.blockToSectionCoord(center.getX() + radius);
        int maxY = SectionPos.blockToSectionCoord(center.getY() + radius);
        int maxZ = SectionPos.blockToSectionCoord(center.getZ() + radius);
        List<GameEvent.ListenerInfo> byDistance = new ArrayList<>();
        GameEventListenerRegistry.ListenerVisitor visitor = (listener, at) -> {
            if (listener.getDeliveryMode() == GameEventListener.DeliveryMode.BY_DISTANCE) {
                byDistance.add(new GameEvent.ListenerInfo(gameEvent, position, context, listener, at));
            } else {
                listener.handleGameEvent(level, gameEvent, context, position);
            }
        };
        for (int cx = minX; cx <= maxX; cx++) {
            for (int cz = minZ; cz <= maxZ; cz++) {
                ChunkAccess chunk = level.getChunkSource().getChunkNow(cx, cz);
                for (int sy = minY; chunk != null && sy <= maxY; sy++) {
                    chunk.getListenerRegistry(sy).visitInRangeListeners(gameEvent, position, context, visitor);
                }
            }
        }
        Collections.sort(byDistance);
        for (GameEvent.ListenerInfo info : byDistance) {
            info.recipient().handleGameEvent(level, info.gameEvent(), info.context(), info.source());
        }

    }
}
